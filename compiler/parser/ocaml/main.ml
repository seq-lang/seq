(* *****************************************************************************
 * Seq.Main: Entry point module
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

let parse file code line_offset col_offset =
  let lexbuf = Lexing.from_string (code ^ "\n") in
  lexbuf.lex_curr_p
  <- { pos_fname = file
     ; pos_lnum = line_offset
     ; pos_cnum = -col_offset
     ; pos_bol = -col_offset
     };
  try
    let stack = Stack.create () in
    Stack.push 0 stack;
    let state = Lexer.{ stack; offset = 0; ignore_newline = 0; fname = file } in
    let ast = Grammar.program (Lexer.token state) lexbuf in
    ast
  with
  | Grammar.Error ->
    let line = lexbuf.lex_start_p.pos_lnum + Lexer.global_offset.line in
    let col =
      lexbuf.lex_start_p.pos_cnum - lexbuf.lex_start_p.pos_bol + Lexer.global_offset.col
    in
    Printf.eprintf "[ocaml] parser error: %s:%d:%d\n%!" file line col;
    exit 1
  | Ast.GrammarError (s, pos) | Ast.SyntaxError (s, pos) ->
    let line = pos.pos_lnum in
    let col = pos.pos_cnum - lexbuf.lex_start_p.pos_bol in
    Printf.eprintf "[ocaml] parser error: %s:%d:%d -- %s\n%!" file line col s;
    exit 1

let () =
  Callback.register "menhir_parse" parse;
  Printf.eprintf "[ocaml] initialized!\n"
