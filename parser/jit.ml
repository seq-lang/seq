(******************************************************************************
 *
 * Seq OCaml 
 * jit.ml: JIT handling module
 *
 * Author: inumanag
 *
 ******************************************************************************)

open Core
open Err

let asp = ANSITerminal.sprintf

type t = 
  { mutable cnt: int;
    ctx: Ctx.t }

let init () : t =
  try 
    let anon_fn = Llvm.Func.func "<anon_init>" in
    let ctx = Ctx.init "<jit>"
      ~argv:false
      (Llvm.JIT.init ())
      anon_fn (Llvm.Block.func anon_fn)
      Parser.parse_file 
    in
    let jit = { cnt = 1; ctx } in
    (* load stdlib *)
    Llvm.JIT.func ctx.mdl anon_fn;
    jit
  with CompilerError (typ, pos_lst) ->
    print_error typ pos_lst;
    exit 1

let exec (jit: t) code = 
  let anon_fn = Llvm.Func.func (sprintf "<anon_%d>" jit.cnt) in
  let anon_ctx = 
    { jit.ctx with 
      base = anon_fn; 
      block = Llvm.Block.func anon_fn;
      map = Hashtbl.copy jit.ctx.map } 
  in 
  Ctx.add_block anon_ctx;
  jit.cnt <- jit.cnt + 1;
  Parser.parse_string ~file:"<jit>" ~jit:true anon_ctx code;
  try 
    Llvm.JIT.func jit.ctx.mdl anon_fn;
    Hash_set.iter (Stack.pop_exn anon_ctx.stack) ~f:(fun key ->
      match Hashtbl.find anon_ctx.map key with
      | Some ((v, ann) :: items) -> 
        if ann.toplevel && ann.global && (not ann.internal) then 
          Ctx.add jit.ctx ~toplevel:true ~global:true key v;
      | _ -> ())
  with SeqCError (msg, pos) -> 
    raise @@ CompilerError (Compiler msg, [pos])

let repl () = 
  let asp = ANSITerminal.sprintf in
  let style = ANSITerminal.[Bold; green] in
  let banner = String.make 78 '=' in
  eprintf "%s\n%!" @@ asp style "%s" banner;
  eprintf "%s\n%!" @@ asp style "Seq JIT";
  eprintf "%s\n%!" @@ asp style "%s" banner;

  let jit = init () in
  let start = ref true in
  let code = ref "" in
  try while true do 
    try 
      if !start then begin
        eprintf "%s%!" @@ asp style "in[%d]>\n" jit.cnt;
        start := false;
      end;
      let s = In_channel.(input_line_exn stdin) in
      code := (!code ^ s ^ "\n")
    with End_of_file ->
      begin try 
        exec jit !code
      with CompilerError (typ, pos_lst) ->
        print_error typ pos_lst ~file:!code
      end;
      if !code = "" then 
        raise Exit
      else begin
        code := "";
        start := true
      end
  done with Exit ->
    let style = ANSITerminal.[Bold; yellow] in
    eprintf "\n%s\n%!" @@ asp style "bye (%d)" jit.cnt
