(******************************************************************************
 *
 * Seq OCaml 
 * main.ml: Entry point module
 *
 * Author: inumanag
 *
 ******************************************************************************)

open Core
open Err

let asp = ANSITerminal.sprintf

let jit_code (ctx: Ctx.t) cnt code = 
  let anon_fn = Llvm.Func.func (sprintf "<anon_%d>" cnt) in
  let anon_ctx = 
    { ctx with 
      base = anon_fn; 
      block = Llvm.Block.func anon_fn;
      map = Hashtbl.copy ctx.map } 
  in 
  Ctx.add_block anon_ctx;
  Parser.parse_string ~file:"<jit>" ~jit:true anon_ctx code;

  Hash_set.iter (Stack.pop_exn anon_ctx.stack) ~f:(fun key ->
    match Hashtbl.find anon_ctx.map key with
    | Some ((v, ann) :: items) -> 
      if ann.toplevel && ann.global && (not ann.internal) then 
        Ctx.add ctx ~toplevel:true ~global:true key v;
    | _ -> ());
  Llvm.JIT.func ctx.mdl anon_fn

let jit_repl () = 
  let style = ANSITerminal.[Bold; green] in
  let banner = String.make 78 '=' in
  eprintf "%s\n%!" @@ asp style "%s" banner;
  eprintf "%s\n%!" @@ asp style "Seq JIT";
  eprintf "%s\n%!" @@ asp style "%s" banner;

  let anon_fn = Llvm.Func.func "<anon_init>" in
  let ctx = Ctx.init "<jit>"
    ~argv:false
    (Llvm.JIT.init ())
    anon_fn (Llvm.Block.func anon_fn)
    Parser.parse_file 
  in
  Llvm.JIT.func ctx.mdl anon_fn;
  let code = ref "" in
  let cnt = ref 1 in 
  let start = ref true in
  try while true do 
    try 
      if !start then begin
        eprintf "%s%!" @@ asp style "in[%d]>\n" !cnt;
        start := false;
      end;
      let s = In_channel.(input_line_exn stdin) in
      code := (!code ^ s ^ "\n")
    with End_of_file ->
      begin try 
        jit_code ctx !cnt !code
      with CompilerError (typ, pos_lst) ->
        print_error typ pos_lst ~file:!code
      end;
      if !code = "" then 
        raise Exit
      else begin
        code := "";
        cnt := !cnt + 1;
        start := true
      end
  done with Exit ->
    let style = ANSITerminal.[Bold; yellow] in
    eprintf "\n%s\n%!" @@ asp style "bye (%d)" !cnt

(** Entry point *)
let () =
  let _ = Callback.register "parse_c" Parser.parse_c in

  if Array.length Sys.argv < 2 then
    jit_repl ()
  else if (Array.length Sys.argv = 2) && (Sys.argv.(1) = "--parse") then
    () 
  else
    try
      let err_handler = fun a b -> raise (CompilerError (a, b)) in
      let m = Parser.init Sys.argv.(1) err_handler in
      match m with
      | Some m -> 
        begin try
          Llvm.Module.exec m (Array.to_list Sys.argv) false
        with SeqCError (msg, pos) -> 
          raise @@ CompilerError (Compiler msg, [pos])
        end
      | None -> 
        raise Caml.Not_found
    with CompilerError (typ, pos_lst) as err ->
      print_error typ pos_lst;
      exit 1
