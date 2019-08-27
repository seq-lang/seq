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
          if ann.toplevel && ann.global && not ann.internal
          then Ctx.add ~ctx:jit.ctx ~toplevel:true ~global:true key v
        | _ -> ())
  with
  | Err.SeqCError (msg, pos) -> raise @@ Err.CompilerError (Compiler msg, [ pos ])

(** JIT entry point. *)
let repl () =
  let banner = String.make 78 '=' in
  eprintf "\027[102m%s\n" banner;
  eprintf "Seq JIT\n";
  eprintf "%s\027[0m \n" banner;
  let jit = init () in
  let start = ref true in
  let code = ref "" in
  try
    while true do
      try
        if !start
        then (
          eprintf "\027[92min[%d]>\027[0m \n" jit.cnt;
          start := false);
        let s = In_channel.(input_line_exn stdin) in
        code := !code ^ s ^ "\n"
      with
      | End_of_file ->
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
