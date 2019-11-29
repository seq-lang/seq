(* *****************************************************************************
 * Seq.Jit: JIT handling module
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Seqaml

(** JIT context. *)
type t =
  { mutable cnt : int (** Execution counter. Used for displaying the prompt. *)
  ; ctx : Ctx.t (** Execution context. *)
  }

(** Initialize a JIT context. *)
let init () : t =
  try
    let anon_fn = Llvm.Func.func "<anon_init>" in
    let ctx =
      Ctx.init_module
        ~argv:false
        ~filename:"<jit>"
        ~mdl:(Llvm.JIT.init ())
        ~base:anon_fn
        ~block:(Llvm.Block.func anon_fn)
        ~jit:true
        (Runner.exec_string ~debug:false ~jit:true)
    in
    let jit = { cnt = 1; ctx } in
    (* load stdlib *)
    Llvm.JIT.func ctx.mdl anon_fn;
    jit
  with
  | Err.CompilerError (typ, pos_lst) ->
    eprintf "%s\n%!" @@ Err.to_string ~pos_lst typ;
    exit 1

(** Execute [code] within a JIT context [jit]. *)
let exec (jit : t) code =
  let anon_fn = Llvm.Func.func (sprintf "<anon_%d>" jit.cnt) in
  let anon_ctx =
    { jit.ctx with
      base = anon_fn
    ; block = Llvm.Block.func anon_fn
    ; map = Hashtbl.copy jit.ctx.map
    }
  in
  Ctx.add_block anon_ctx;
  jit.cnt <- jit.cnt + 1;
  Runner.exec_string ~file:"<jit>" ~jit:true anon_ctx code;
  try
    Llvm.JIT.func jit.ctx.mdl anon_fn;
    Hash_set.iter (Stack.pop_exn anon_ctx.stack) ~f:(fun key ->
        match Hashtbl.find anon_ctx.map key with
        | Some ((v, ann) :: items) ->
          (* eprintf "%s: %b %b %b\n%!" key ann.toplevel ann.global ann.internal; *)
          if ann.toplevel && ann.global && not ann.internal
          then Ctx.add ~ctx:jit.ctx ~toplevel:true ~global:true key v
        | _ -> ())
  with
  | Err.SeqCError (msg, pos) -> raise @@ Err.CompilerError (Compiler msg, [ pos ])

let locate cmd f l c = 
  let open Option.Monad_infix in
  Hashtbl.find Ctx.inspect_lookup f
  >>= (fun t -> Hashtbl.find t l)
  >>= (fun s -> Stack.find_map s ~f:(fun Ctx.{ pos; el; name } ->
    if pos.col < c || pos.col + pos.len >= c 
    then None
    else match Llvm.JIT.get_pos el with
      | None -> None
      | Some orig_pos ->
        eprintf "%%JIT%%: found %s, orig pos %s"
          name (Ast.Ann.to_string orig_pos);
        Some 1
  ))
  

(** JIT entry point. *)
let repl () =
  let banner = String.make 78 '=' in
  eprintf "\027[102m%s\027[0m \n" banner;
  eprintf "\027[102m=%s%s%s=\027[0m \n"
    (String.make 34 ' ') "Seq REPL" (String.make 34 ' ');
  eprintf "\027[102m%s\027[0m \n%!" banner;
  let jit = init () in
  let start = ref true in
  let code = ref "" in
  try
    while true do
      try
        if !start
        then (
          eprintf "\027[92min[%d]>\027[0m \n%!" jit.cnt;
          start := false);
        let s = In_channel.(input_line_exn stdin) in
        code := !code ^ s ^ "\n"
      with
      | End_of_file ->
        eprintf "------------------\n%!";
        match String.is_prefix ~prefix:"%%" !code with
        | true ->
          let ll = Array.of_list @@ String.split ~on:' ' !code in
          let cmd = ll.(0) in
          let file = ll.(1) in
          let line = Int.of_string @@ ll.(2) in
          let pos = Int.of_string @@ ll.(3) in
          ignore @@ locate cmd file line pos;
          code := "";
          start := true
        | false -> 
          (try exec jit !code with
          | Err.CompilerError (typ, pos_lst) ->
            eprintf "%s\n%!" @@ Err.to_string ~pos_lst ~file:!code typ);
          if !code = ""
          then raise Exit
          else (
            code := "";
            start := true)
    done
  with
  | Exit -> eprintf "\n\027[31mbye (%d) \027[0m\n%!" jit.cnt


let jits: (nativeint, t) Hashtbl.t = Hashtbl.Poly.create ()

let c_init () =
  let hnd = init () in
  let p = Ctypes.raw_address_of_ptr hnd.ctx.mdl in
  Hashtbl.set jits ~key:p ~data:hnd;
  (* eprintf "[lib] %nx\n%!" p; *)
  p

let c_exec hnd code =
  (* eprintf "[lib] looking for %nx ... \n%!" hnd; *)
  let jit = Hashtbl.find_exn jits hnd in
  (* Hashtbl.iter_keys jit.ctx.map ~f:(fun k ->
    eprintf "[lib] keys: %s ... \n%!" k); *)
  try
    exec jit code
  with
  | Err.CompilerError (typ, pos_lst) ->
    eprintf "%s\n%!" @@ Err.to_string ~pos_lst ~file:code typ

let c_close hnd =
  let jit = Hashtbl.find_exn jits hnd in
  eprintf "Closing JIT handle, %d commands executed\n%!" jit.cnt;
  ()
