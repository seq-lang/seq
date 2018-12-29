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

let print_error typ pos_lst =
  let kind, msg = match typ with
    | Lexer s -> "lexer", s
    | Parser -> "parser", "Parsing error"
    | Descent s -> "descent", s
    | Compiler s -> "compiler", s 
  in
  let file_line file line =
    if String.length file > 0 && file.[0] <> '<' then
      try
        let lines = In_channel.read_lines file in 
        List.nth lines (line - 1)
      with _ -> 
        None
    else None 
  in
  let style = ANSITerminal.[Bold; red] in
  eprintf "%s%!" @@ ANSITerminal.sprintf style 
    "[ERROR] %s error: %s\n" kind msg;
  List.iteri pos_lst ~f:(fun i pos ->
    let Ast.Pos.{ file; line; col; len } = pos in
    match file_line file line with
    | Some file_line  ->
      let pre = if i = 0 then "" else "then in\n        " in 
      eprintf "%s%!" @@ ANSITerminal.sprintf style "        %s%s: %d,%d\n" 
        pre file line col;
      eprintf "%s%!" @@ ANSITerminal.sprintf style "   %3d: %s" 
        line (String.prefix file_line col);
      eprintf "%s%!" @@ ANSITerminal.sprintf 
        ANSITerminal.[Bold; white; on_red] "%s" 
        (String.sub file_line ~pos:col ~len);
      eprintf "%s%!" @@ ANSITerminal.sprintf style "%s" 
        (String.drop_prefix file_line (col + len));
      eprintf "%s%!" @@ ANSITerminal.sprintf [] "\n"
    | None -> ())

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
  let banner = String.make 60 '=' in
  eprintf "%s\n%!" @@ ANSITerminal.sprintf style "%s" banner;
  eprintf "%s\n%!" @@ ANSITerminal.sprintf style "Seq JIT";
  eprintf "%s\n%!" @@ ANSITerminal.sprintf style "%s" banner;

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
    if !start then begin
      eprintf "%s%!" @@ ANSITerminal.sprintf style "in[%d]>\n" !cnt;
      start := false;
    end else ();
    let s = In_channel.(input_line_exn stdin) in
    code := (!code ^ s ^ "\n");
    try if (String.suffix s 2) = ";;" then begin
      jit_code ctx !cnt (String.prefix !code ((String.length !code) - 3));
      code := "";
      cnt := !cnt + 1;
      start := true;
    end else ()    
    with CompilerError (typ, pos_lst) as err ->
      print_error typ pos_lst;
      code := "";
      cnt := !cnt + 1;
      start := true;
  done with End_of_file ->
    let style = ANSITerminal.[Bold; yellow] in
    eprintf "\n%s\n%!" @@ ANSITerminal.sprintf style "bye (%d)" !cnt

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
      | None -> raise 
        Caml.Not_found
    with CompilerError (typ, pos_lst) as err ->
      print_error typ pos_lst;
      exit 1
