(* 786 *)

open Core

module LLE = Llvm_executionengine

exception ParserError of string

let parse s =
  let print_position lexbuf =
    let pos = lexbuf.Lexing.lex_curr_p in
    sprintf "%s;line %d;pos %d" pos.pos_fname pos.pos_lnum (pos.pos_cnum - pos.pos_bol + 1)
  in
  let error msg = raise (ParserError msg) in
  let lexbuf = Lexing.from_string s in
  try
    let ast = Parser.program Lexer.read lexbuf in
    Ast.flatten ast
  with
  | Lexer.SyntaxError msg ->
    sprintf "%s: %s\n" (print_position lexbuf) msg |> error 
  | Parser.Error ->
    sprintf "%s: syntax error %s\n" (print_position lexbuf) (Lexing.lexeme lexbuf) |> error

let rec toplevel jit fpm = 
  printf "> ";
  Out_channel.flush stdout;
  match In_channel.input_line In_channel.stdin with
  | None -> ()
  | Some line -> begin
    try
      let ast, exec = 
        if line.[0] = '!' then 
          parse (String.sub line 1 (String.length line - 1) ^ "\n"), true
        else
          parse (line ^ "\n"), false 
      in
      Ast.prn_ast_sexp ast |> printf "%s\n";
      Ast.prn_ast ast |> printf "%s\n";
      let _r = Codegen.codegen ast in

      Utils.dump ();
      Llvm_analysis.assert_valid_function Codegen.main.fn;
      if exec then begin
        LLE.add_module Init.llm jit;
        let ct = Foreign.funptr Ctypes.(void @-> returning void) in
        let f = LLE.get_function_address "main" ct jit in
        f ();
        LLE.remove_module Init.llm jit
      end
    with 
    | Init.CompileError msg | ParserError msg -> 
      printf "error: %s\n" msg;
  end
  (* toplevel jit fpm *)

let () = 
  LLE.initialize () |> ignore;
  let jit = LLE.create Init.llm in
  let fpm = Llvm.PassManager.create_function Init.llm in
  Llvm.PassManager.initialize fpm |> ignore;

  printf "🐪 🐫 🐪 🚶🏻‍  seq « 0.1 » 🚶🏻‍ 🐫 🐪 🐫\n%!";
  toplevel jit fpm
