(* *****************************************************************************
 * Seq.Main: Entry point module
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

external raise_exception: string -> string -> int -> int -> unit = "seq_ocaml_exception"

open Printf

let print_token t =
  let open Seqgrammar in
  match t with
  | YIELD -> "YIELD"
  | XOREQ s -> sprintf "XOREQ(%s)" s
  | WITH -> "WITH"
  | WHILE -> "WHILE"
  | TYPEOF -> "TYPEOF"
  | TRY -> "TRY"
  | TRUE -> "TRUE"
  | THROW -> "THROW"
  | SUB s -> sprintf "SUB(%s)" s
  | STRING(p,s) -> sprintf "STRING(%s,%s)" p s
  | SPIPE s -> sprintf "SPIPE(%s)" s
  | SEMICOLON -> "SEMICOLON"
  | RSHEQ s -> sprintf "RSHEQ(%s)" s
  | RS -> "RS"
  | RP -> "RP"
  | RETURN -> "RETURN"
  | RB -> "RB"
  | PRINT -> "PRINT"
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
  | ISNOT s -> sprintf "ISNOT(%s)" s
  | IS s -> sprintf "IS(%s)" s
  | INT(a,b) -> sprintf "INT(%s,%s)" a b
  | INDENT -> "INDENT"
  | IN s -> sprintf "IN(%s)" s
  | IMPORT -> "IMPORT"
  | IF -> "IF"
  | ID s -> sprintf "ID(%s)" s
  | GREAT s -> sprintf "GREAT(%s)" s
  | GLOBAL -> "GLOBAL"
  | GEQ s -> sprintf "GEQ(%s)" s
  | FROM -> "FROM"
  | FOR -> "FOR"
  | FLOAT(f,s) -> sprintf "FLOAT(%f,%s)" f s
  | FINALLY -> "FINALLY"
  | FDIVEQ s -> sprintf "FDIVEQ(%s)" s
  | FDIV s -> sprintf "FDIV(%s)" s
  | FALSE -> "FALSE"
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
  | EXTERN s -> sprintf "EXTERN(\n%s)" s
  | WALRUS s -> ":="
  | PRINTLP -> "print("
  (* | ARROW -> "=>" *)


module I = Seqgrammar.MenhirInterpreter

let find_map f t =
  let rec loop = function
    | [] -> None
    | x :: l -> match f x with None -> loop l | Some _ as r -> r
  in
  loop t

let rec loop lexbuf state cache checkpoint =
  match checkpoint with
  | I.Accepted v -> Some v
  | I.InputNeeded _env ->
    ( match cache with
      | [] ->
        loop lexbuf state (Lexer.token state lexbuf) checkpoint
      | t :: ts ->
        let ch = I.offer checkpoint (t, lexbuf.lex_start_p, lexbuf.lex_curr_p) in
        loop lexbuf state ts ch )
  | I.HandlingError _env ->
    (*
    let state = I.current_state_number _env in
    let msg =
      try
        let msg = String.trim @@ Seqgrammar_messages.message state in
        if msg.[0] = '!'
        then sprintf ": %s ('%s')" (String.sub msg 1 (String.length msg - 1)) (Lexing.lexeme lexbuf)
        else sprintf ": %s" msg
      with Not_found -> ""
    in *)
    let msg = sprintf ": '%s'" (Lexing.lexeme lexbuf) in
    let msg, pos =  (sprintf "parsing error%s" msg), lexbuf.lex_start_p in
    raise_exception msg pos.pos_fname (pos.pos_lnum + 1) (pos.pos_cnum - pos.pos_bol + 1);
    None
  | I.Shifting _ | I.AboutToReduce _ -> loop lexbuf state cache (I.resume checkpoint)
  | I.Rejected -> assert false

let test file code =
  let open Seqgrammar in
  printf "[lex] file: %s\n[lex]   %!" file;
  let lexbuf = Lexing.from_string (code ^ "\n") in
  let stack = Stack.create () in
  Stack.push 0 stack;
  let state = Lexer.{ stack; offset = 0; ignore_newline = 0; fname = "test" } in
  let finished = ref false in
  while not !finished do
    Lexer.token state lexbuf |> List.iter (function
      | EOF -> printf "EOF\n%!"; finished := true
      | (NL | INDENT | DEDENT) as t -> printf "%s\n[lex]   %!" @@ print_token t
      | t -> printf "%s %!" @@ print_token t
    )
  done

let parse file code line_offset col_offset debug =
  let lexbuf = Lexing.from_string (code ^ "\n") in
  lexbuf.lex_curr_p
  <- { pos_fname = file
     ; pos_lnum = line_offset
     ; pos_cnum = -col_offset
     ; pos_bol = -col_offset
     };
  try
    if debug > 0 then test file code;
    let stack = Stack.create () in
    Stack.push 0 stack;
    let state = Lexer.{ stack; offset = 0; ignore_newline = 0; fname = file } in
    loop lexbuf state [] (Seqgrammar.Incremental.program lexbuf.lex_curr_p)
  with Ast.SyntaxError (s, pos) ->
    let s = sprintf "lexing error: %s" s in
    raise_exception s file (pos.pos_lnum + 1) (pos.pos_cnum - pos.pos_bol + 1);
    None

let () =
  Callback.register "menhir_parse" parse
