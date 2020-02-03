(* *****************************************************************************
 * Seq.Main: Entry point module
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

external raise_exception: string -> string -> int -> int -> unit = "seq_ocaml_exception"

let print_token t =
  let open Grammar in
  let open Printf in
  match t with
  | YIELD -> "YIELD"
  | XOREQ s -> sprintf "XOREQ(%s)" s
  | WITH -> "WITH"
  | WHILE -> "WHILE"
  | TYPEOF -> "TYPEOF"
  | TYPE -> "TYPE"
  | TRY -> "TRY"
  | TRUE -> "TRUE"
  | THROW -> "THROW"
  | SUB s -> sprintf "SUB(%s)" s
  | STRING s -> sprintf "STRING(%s)" s
  | SPIPE s -> sprintf "SPIPE(%s)" s
  | SEQ(a,b)-> sprintf "SEQ(%s,%s)" a b
  | SEMICOLON -> "SEMICOLON"
  | RSHEQ s -> sprintf "RSHEQ(%s)" s
  | RS -> "RS"
  | RP -> "RP"
  | RETURN -> "RETURN"
  | RB -> "RB"
  | PYDEF -> "PYDEF"
  | PTR -> "PTR"
  | PRINT -> "PRINT"
  | PREFETCH -> "PREFETCH"
  | PPIPE s -> sprintf "PPIPE(%s)" s
  | POWEQ s -> sprintf "POWEQ(%s)" s
  | POW s -> sprintf "POW(%s)" s
  | PLUSEQ s -> sprintf "PLUSEQ(%s)" s
  | PIPE s -> sprintf "PIPE(%s)" s
  | PASS -> "PASS"
  | OREQ s -> sprintf "OREQ(%s)" s
  | OR s -> sprintf "OR(%s)" s
  | OF -> "OF"
  | NOTIN s -> sprintf "NOTIN(%s)" s
  | NOT s -> sprintf "NOT(%s)" s
  | NONE -> "NONE"
  | NL -> "NL"
  | NEQ s -> sprintf "NEQ(%s)" s
  | MULEQ s -> sprintf "MULEQ(%s)" s
  | MUL s -> sprintf "MUL(%s)" s
  | MODEQ s -> sprintf "MODEQ(%s)" s
  | MOD s -> sprintf "MOD(%s)" s
  | MINEQ s -> sprintf "MINEQ(%s)" s
  | MATCH -> "MATCH"
  | LSHEQ s -> sprintf "LSHEQ(%s)" s
  | LS -> "LS"
  | LP -> "LP"
  | LESS s -> sprintf "LESS(%s)" s
  | LEQ s -> sprintf "LEQ(%s)" s
  | LB -> "LB"
  | LAMBDA -> "LAMBDA"
  | KMER s -> sprintf "KMER(%s)" s
  | ISNOT s -> sprintf "ISNOT(%s)" s
  | IS s -> sprintf "IS(%s)" s
  | INT_S(a,b) -> sprintf "INT_S(%s,%s)" a b
  | INDENT -> "INDENT"
  | IN s -> sprintf "IN(%s)" s
  | IMPORT -> "IMPORT"
  | IF -> "IF"
  | ID s -> sprintf "ID(%s)" s
  | GREAT s -> sprintf "GREAT(%s)" s
  | GLOBAL -> "GLOBAL"
  | GEQ s -> sprintf "GEQ(%s)" s
  | FSTRING s -> sprintf "FSTRING(%s)" s
  | FROM -> "FROM"
  | FOR -> "FOR"
  | FLOAT_S(f,s) -> sprintf "FLOAT_S(%f,%s)" f s
  | FINALLY -> "FINALLY"
  | FDIVEQ s -> sprintf "FDIVEQ(%s)" s
  | FDIV s -> sprintf "FDIV(%s)" s
  | FALSE -> "FALSE"
  | EXTERN s -> sprintf "EXTERN(%s)" s
  | EXTEND -> "EXTEND"
  | EXCEPT -> "EXCEPT"
  | EQ s -> sprintf "EQ(%s)" s
  | EOF -> "EOF"
  | ELSE -> "ELSE"
  | ELLIPSIS s -> sprintf "ELLIPSIS(%s)" s
  | ELIF -> "ELIF"
  | EEQ s -> sprintf "EEQ(%s)" s
  | DOT -> "DOT"
  | DIVEQ s -> sprintf "DIVEQ(%s)" s
  | DIV s -> sprintf "DIV(%s)" s
  | DEL -> "DEL"
  | DEF -> "DEF"
  | DEDENT -> "DEDENT"
  | CONTINUE -> "CONTINUE"
  | COMMA -> "COMMA"
  | COLON -> "COLON"
  | CLASS -> "CLASS"
  | CASE -> "CASE"
  | B_XOR s -> sprintf "B_XOR(%s)" s
  | B_RSH s -> sprintf "B_RSH(%s)" s
  | B_OR s -> sprintf "B_OR(%s)" s
  | B_NOT s -> sprintf "B_NOT(%s)" s
  | B_LSH s -> sprintf "B_LSH(%s)" s
  | B_AND s -> sprintf "B_AND(%s)" s
  | BREAK -> "BREAK"
  | AT s -> sprintf "AT(%s)" s
  | ASSERT -> "ASSERT"
  | AS -> "AS"
  | ANDEQ s -> sprintf "ANDEQ(%s)" s
  | AND s -> sprintf "AND(%s)" s
  | ADD s -> sprintf "ADD(%s)" s



let test code =
  let lexbuf = Lexing.from_string (code ^ "\n") in
  let stack = Stack.create () in
  Stack.push 0 stack;
  let state = Lexer.{ stack; offset = 0; ignore_newline = 0; fname = "test" } in
  while true do
    let t = Lexer.token state lexbuf in
    match t with
    | EOF -> Printf.eprintf "EOF\n%!"; exit 0
    | NL | INDENT | DEDENT -> Printf.eprintf "%s\n%!" @@ print_token t
    | t -> Printf.eprintf "%s %!" @@ print_token t
  done

let parse file code line_offset col_offset =
  let lexbuf = Lexing.from_string (code ^ "\n") in
  lexbuf.lex_curr_p
  <- { pos_fname = file
     ; pos_lnum = line_offset
     ; pos_cnum = -col_offset
     ; pos_bol = -col_offset
     };
  try
    (* test code ; *)
    let stack = Stack.create () in
    Stack.push 0 stack;
    let state = Lexer.{ stack; offset = 0; ignore_newline = 0; fname = file } in
    let ast = Grammar.program (Lexer.token state) lexbuf in
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
  Callback.register "menhir_parse" parse
