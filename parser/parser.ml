(******************************************************************************
 *
 * Seq OCaml 
 * parser.ml: Main parsing module
 *
 * Author: inumanag
 *
 ******************************************************************************)

open Core
open Err

let parse_ast ?fn ?(file="") code = 
  let lexbuf = Lexing.from_string (code ^ "\n") in
  let file = if file = "" then "\t" ^ code else file in
  try
    let state = Lexer.stack_create file in
    let ast = Grammar.program (Lexer.token state) lexbuf in
    Option.call ~f:fn ast;
    ast
  with
  | SyntaxError (msg, pos) ->
    raise @@ CompilerError (Lexer msg, [pos])
  | Grammar.Error ->
    let pos = Ast.Pos.
      { file;
        line = lexbuf.lex_start_p.pos_lnum;
        col = lexbuf.lex_start_p.pos_cnum - lexbuf.lex_start_p.pos_bol;
        len = 1 } 
    in
    (* Printexc.print_backtrace stderr; *)
    raise @@ CompilerError (Parser, [pos])
  | SeqCamlError (msg, pos) ->
    raise @@ CompilerError (Descent msg, pos)
  | SeqCError (msg, pos) ->
    raise @@ CompilerError (Compiler msg, [pos])
  | InternalError (msg, pos) ->
    raise @@ CompilerError (Internal msg, [pos])
