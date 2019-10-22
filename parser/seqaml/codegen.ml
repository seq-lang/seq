(* *****************************************************************************
 * Seqaml.Parser: Parses strings to AST
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core

(** [parse ?f ?file str] parses a string [str] and optionally applies [f] to the resulting AST.
    [file] is used only to populate corresponding [Ast.Ann] annotations. *)
let parse ?f ?(file = "") code =
  let lexbuf = Lexing.from_string (code ^ "\n") in
  let file = if file = "" then "\t" ^ code else file in
  try
    let state = Lexer.stack_create file in
    let ast = Grammar.program (Lexer.token state) lexbuf in
    Option.call ~f ast;
    ast
  with
  | Err.SyntaxError (msg, pos) -> raise @@ Err.CompilerError (Lexer msg, [ pos ])
  | Grammar.Error ->
    let pos =
      Ast.Ann.create
        ~file
        ~line:lexbuf.lex_start_p.pos_lnum
        ~col:(lexbuf.lex_start_p.pos_cnum - lexbuf.lex_start_p.pos_bol)
        ~len:1
        ()
    in
    raise @@ Err.CompilerError (Parser, [ pos ])
  | Err.SeqCamlError (msg, pos) ->
    Printexc.print_backtrace stderr;
    raise @@ Err.CompilerError (Descent msg, pos)
  | Err.SeqCError (msg, pos) -> raise @@ Err.CompilerError (Compiler msg, [ pos ])
  | Err.InternalError (msg, pos) -> raise @@ Err.CompilerError (Internal msg, [ pos ])

let parse_file ?f file =
  let ast =
    In_channel.read_lines file
      |> String.concat ~sep:"\n"
      |> parse ~file:(Filename.realpath file)
  in
  match f with Some f -> List.map ~f ast | None -> ast
