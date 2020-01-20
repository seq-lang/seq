(* *****************************************************************************
 * Seq.Main: Entry point module
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

external raise_exception: string -> string -> int -> int -> unit = "seq_ocaml_exception"

(* let raise_exception a b c d =
  Printf.eprintf "[error] %s %s %d %d" a b c d;
  ignore @@ exit 1;
  () *)

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
    Printf.eprintf "[ocaml] done!\n%!";
    Some ast
  with
  | Grammar.Error ->
    raise_exception "parser error" file
      (lexbuf.lex_start_p.pos_lnum)
      (lexbuf.lex_start_p.pos_cnum - lexbuf.lex_start_p.pos_bol);
    None
  | Ast.GrammarError (s, pos) | Ast.SyntaxError (s, pos) ->
    raise_exception s file
      pos.pos_lnum
      (pos.pos_cnum - lexbuf.lex_start_p.pos_bol);
    None

let () =
  Callback.register "menhir_parse" parse;
  Printf.eprintf "[ocaml] initialized!\n%!";
  (* let lines = ref [] in
  let chan = open_in Sys.argv.(1) in
  ( try while true; do lines := input_line chan :: !lines done
    with End_of_file -> close_in chan );
  ignore @@ parse Sys.argv.(1) (String.concat "\n" @@ List.rev !lines) 0 0 *)
