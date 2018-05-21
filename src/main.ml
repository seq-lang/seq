(* 786 *)

open Core

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
    ast
  with
  | Lexer.SyntaxError msg ->
    sprintf "%s: %s\n" (print_position lexbuf) msg |> error 
  | Parser.Error ->
    sprintf "%s: syntax error %s\n" (print_position lexbuf) (Lexing.lexeme lexbuf) |> error

let rec toplevel () = 
  printf "> ";
  Out_channel.flush stdout;
  match In_channel.input_line In_channel.stdin with
  | None -> ()
  | Some line ->
    try
      let ast = parse (line ^ "\n") in
      Ast.prn_ast ast |> printf "%s\n";
      let (t, l) = Codegen.codegen ast in
      Codegen.dump Codegen.fmain;
      toplevel ()
    with 
    | Codegen.CompileError msg | ParserError msg -> 
      printf "error: %s\n" msg;
      toplevel ()

let () = 
  printf "🐪  ~ seq (0.0.1)\n";
  toplevel ()
