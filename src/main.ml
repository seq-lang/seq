(* 786 *)

open Core

let print_position lexbuf =
  let pos = lexbuf.Lexing.lex_curr_p in
  sprintf "%s;line %d;pos %d" pos.pos_fname pos.pos_lnum (pos.pos_cnum - pos.pos_bol + 1)

external c_compile: Ast.ast -> unit = "caml_compile"

let () = 
  let ic = In_channel.stdin in
  let lines = In_channel.input_lines ic in
  let lexbuf = Lexing.from_string @@ (String.concat ~sep:"\n" lines) ^ "\n" in
  let state = Lexer.stack_create () in
  try
    let ast = Parser.program (Lexer.token state) lexbuf in  
    fprintf stderr "|> AST::Caml ==> \n%s\n" @@ Ast.prn_ast ast;
    c_compile ast
  with 
  | Lexer.SyntaxError msg ->
    fprintf stderr "!! Lexer error: %s\n" msg
  | Parser.Error ->
    fprintf stderr "!! Menhir error %s: %s\n" (print_position lexbuf) (Lexing.lexeme lexbuf)
  | Failure msg ->
    fprintf stderr "!! C++/JIT error: %s\n" msg
