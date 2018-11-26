(* 786 *)

open Core

module Assignable = 
struct
  (* realized type with matching ASTs *)
  type t = 
    (string, elt list) Hashtbl.t
  and elt = 
    | Var  of Llvm.Types.var_t
    | Func of Llvm.Types.func_t
    | Type of Llvm.Types.typ_t
end

type t = {
  (* filename; TODO: remove *)
  filename: string;
  prefix: string;

  (* module pointer *)
  mdl: Llvm.Types.func_t;
  (* base function *)
  base: Llvm.Types.func_t;
  (* block pointer *)
  block: Llvm.Types.block_t;

  (* stack of blocks and variable hashsets *)
  stack: ((string) Hash_set.t) Stack.t;
  (* vtable *)
  map: Assignable.t;

  (* function for file parsing (import statements) *)
  parse_file: (t -> string -> unit);
}

let add_block ctx = 
  Stack.push ctx.stack (String.Hash_set.create ())

let init filename mdl base block parse_file =
  let ctx = 
    { filename;
      prefix = "";
      mdl;
      base;
      block;
      stack = Stack.create ();
      map = String.Table.create ();
      parse_file; }
  in
  add_block ctx;
  
  (* initialize POD types *)
  let pairs = Llvm.Type.(
    [ "void", void; 
      "int", int; 
      "str", str;  
      "seq", seq; 
      "bool", bool; 
      "float", float; 
      "byte", byte ]) 
  in
  List.iter pairs ~f:(fun (key, fn) -> 
    let data = [ Assignable.Type (fn ()) ] in
    Hashtbl.set ctx.map ~key ~data);
  ctx

let dump ctx =
  let open Util in
  dbg "=== == - CONTEXT DUMP - == ===";
  dbg "-> Filename: %s" ctx.filename;
  dbg "-> Keys:";

  let sortf (xa, xb) (ya, yb) = 
    compare (xb, xa) (yb, ya) 
  in
  let ind x = String.make x ' ' in
  let prn_assignable _ ass = 
    let open Assignable in
    match ass with
    | Var _ -> sprintf "(*var*)", ""
    | Func _ -> sprintf "(*fun*)", ""
    | Type _ -> sprintf "(*typ*)", ""
  in
  let sorted = 
    Hashtbl.to_alist ctx.map |>
    List.map ~f:(fun (a, b) -> (a, List.hd_exn b)) |>
    List.sort ~compare:sortf
  in
  List.iter sorted ~f:(fun (key, data) -> 
    let pre, pos = prn_assignable 3 data in
    dbg "   %s %s %s" pre key pos)

let add ctx key var =
  begin match Hashtbl.find ctx.map key with
    | None -> 
      Hashtbl.set ctx.map ~key ~data:[var]
    | Some lst -> 
      Hashtbl.set ctx.map ~key ~data:(var :: lst)
  end;
  Hash_set.add (Stack.top_exn ctx.stack) key

let clear_block ctx =
  Hash_set.iter (Stack.pop_exn ctx.stack) ~f:(fun key ->
    match Hashtbl.find ctx.map key with
    | Some [item] -> 
      Hashtbl.remove ctx.map key
    | Some (_ :: items) -> 
      Hashtbl.set ctx.map ~key ~data:items
    | Some [] | None ->
      failwith (sprintf "can't find context variable %s" key))

let in_scope ctx key =
  match Hashtbl.find ctx.map key with
  | Some (hd :: _) -> Some hd
  | _ -> None

let in_block ctx key =
  if Stack.length ctx.stack = 0 then 
    None
  else if Hash_set.exists (Stack.top_exn ctx.stack) ~f:((=) key) then
    Some (List.hd_exn @@ Hashtbl.find_exn ctx.map key)
  else 
    None

