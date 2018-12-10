(******************************************************************************
 *
 * Seq OCaml 
 * ctx.ml: Context (variable table) definitions 
 *
 * Author: inumanag
 *
 ******************************************************************************)

open Core

(** Variable table description  *)
module VTable = 
struct
  (** Variable table is a dictionary that maps names to LLVM types *)
  type t = 
    (string, elt list) Hashtbl.t
  (** Describes potential kinf of variable *)
  and elt = 
    | Var  of Llvm.Types.var_t * annotation
    | Func of Llvm.Types.func_t
    | Type of Llvm.Types.typ_t
  (** Each assignable variable is annotated with 
      [base] function pointer, and flags describing does
      it belong to toplevel or not, and is it global or not  *)
  and annotation = 
    { base: Llvm.Types.func_t;
      toplevel: bool;
      global: bool }
end

(** Context type *)
type t = 
  { (** context filename *)
    filename: string;
    (** [SeqModule] pointer *)
    mdl: Llvm.Types.func_t;
    (** base function pointer *)
    base: Llvm.Types.func_t;
    (* block pointer *)
    block: Llvm.Types.block_t;
    (** try-catch pointer for expressions *)
    trycatch: Llvm.Types.stmt_t;

    (** stack of blocks. Top of stack is the current (deepest) block.
        Each block maintains a set of variables on stack. *)
    stack: ((string) Hash_set.t) Stack.t;
    (** Variable lookup table *)
    map: VTable.t;

    (** function that parses a file within current context 
        (used for processing [import] statements) *)
    parse_file: (t -> string -> unit) }

(** [add_block context] pushed a new block to context stack *)
let add_block ctx = 
  Stack.push ctx.stack (String.Hash_set.create ())

(** [init ...] initializes an empty context with toplevel block
    and adds internal POD types to the vtable *)
let init filename mdl base block parse_file =
  let ctx = 
    { filename;
      mdl;
      base;
      block;
      stack = Stack.create ();
      map = String.Table.create ();
      parse_file;
      trycatch = Ctypes.null }
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
    let data = [ VTable.Type (fn ()) ] in
    Hashtbl.set ctx.map ~key ~data);
  ctx

(** [dump context] dumps [context] vtable to debug output  *)
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
    match ass with
    | VTable.Var _  -> sprintf "(*var*)", ""
    | VTable.Func _ -> sprintf "(*fun*)", ""
    | VTable.Type _ -> sprintf "(*typ*)", ""
  in
  let sorted = 
    Hashtbl.to_alist ctx.map |>
    List.map ~f:(fun (a, b) -> (a, List.hd_exn b)) |>
    List.sort ~compare:sortf
  in
  List.iter sorted ~f:(fun (key, data) -> 
    let pre, pos = prn_assignable 3 data in
    dbg "   %s %s %s" pre key pos)

(** [var ~toplevel context var] is a helper that creates 
    a new assignable non-global variable *)
let var (ctx: t) ?(toplevel=false) var =
  VTable.Var (var, 
    { base = ctx.base; global = false; toplevel })

(** [add context name var] adds a variable to the current block *)
let add ctx key var =
  begin match Hashtbl.find ctx.map key with
    | None -> 
      Hashtbl.set ctx.map ~key ~data:[var]
    | Some lst -> 
      Hashtbl.set ctx.map ~key ~data:(var :: lst)
  end;
  Hash_set.add (Stack.top_exn ctx.stack) key

(** [clear_block context] pops the current block 
    and removes all block variables from vtable *)
let clear_block ctx =
  Hash_set.iter (Stack.pop_exn ctx.stack) ~f:(fun key ->
    match Hashtbl.find ctx.map key with
    | Some [_] -> 
      Hashtbl.remove ctx.map key
    | Some (_ :: items) -> 
      Hashtbl.set ctx.map ~key ~data:items
    | Some [] | None ->
      failwith (sprintf "can't find context variable %s" key))

(** [in_scope context name] checks is a variable [name] present 
    in the current scope and returns it if so *)
let in_scope ctx key =
  match Hashtbl.find ctx.map key with
  | Some (hd :: _) -> Some hd
  | _ -> None

(** [in_block context name] checks is a variable [name] present 
    in the current block and returns it if so *)
let in_block ctx key =
  if Stack.length ctx.stack = 0 then 
    None
  else if Hash_set.exists (Stack.top_exn ctx.stack) ~f:((=) key) then
    Some (List.hd_exn @@ Hashtbl.find_exn ctx.map key)
  else 
    None
