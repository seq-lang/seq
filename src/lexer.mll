(* 786 *)

{
  module B = Buffer
  module L = Lexing
  module P = Parser

  open Core
  
  exception SyntaxError of string

  type stack = {
    stack: int Stack.t;
    mutable offset: int;
    mutable ignore_newline: bool;
  }
  
  let stack_create () =
    let stack = Stack.create () in
    Stack.push stack 0;
    {stack = stack; offset = 0; ignore_newline = false}
  
  let ignore_nl t =
    t.ignore_newline <- true
  and aware_nl t =
    t.ignore_newline <- false

  let count_lines s =
    let n = ref 0 in 
    String.iter ~f:(fun c -> if c = '\n' then incr n) s;
    !n
}

(* lexer regex expressions *)

let newline = '\n' | "\r\n"
let white = [' ' '\t']
let comment = '#' [^ '\n' '\r']*

let digit = ['0'-'9']
let int = '-'? digit+
let frac = '.' digit*
let pfloat = int? frac
let exp = ['e' 'E'] ['+' '-']? digit+
let efloat = pfloat? exp
let float = pfloat | efloat

let escape = '\\' _

let alpha = ['a'-'z' 'A'-'Z' '_']
let alphanum = ['A'-'Z' 'a'-'z' '0'-'9' '_']

let ident = alpha alphanum*

(* rules *)

rule token state = parse 
  | "" {
    let cur = state.offset in
    let last = Stack.top_exn state.stack in
    if cur < last then begin
      Stack.pop state.stack |> ignore;
      P.DEDENT
    end
    else if cur > last then begin
      Stack.push state.stack cur;
      P.INDENT
    end
    else read state lexbuf (* go ahead with parsing *)
  }

and read state = parse
  | ((white* comment? newline)* white* comment?) newline {
    let lines = count_lines (L.lexeme lexbuf) in
    lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with 
      pos_lnum = lexbuf.lex_curr_p.pos_lnum + lines;
      pos_bol = lexbuf.lex_curr_p.pos_cnum
    };
    if not state.ignore_newline then begin
      state.offset <- 0;
      offset state lexbuf;
      P.NL
    end 
    else read state lexbuf 
  }
  | '\\' newline white* {
    lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with 
      pos_lnum = lexbuf.lex_curr_p.pos_lnum + 1;
      pos_bol = lexbuf.lex_curr_p.pos_cnum
    };
    read state lexbuf
  }
  | white+ { read state lexbuf }
  
  | ident as id {
    match id with
      | "if" -> P.IF
      | "elif" -> P.ELIF
      | "else" -> P.ELSE
      | "def" -> P.DEF
      | "for" -> P.FOR
      | "break" -> P.BREAK
      | "continue" -> P.CONTINUE
      | "in" -> P.IN
      | "or" -> P.OR("or")
      | "and" -> P.AND("and")
      | "not" -> P.NOT("not")
      | "print" -> P.PRINT
      | "return" -> P.RETURN
      | "yield" -> P.YIELD
      | "match" -> P.MATCH
      | "case" -> P.CASE
      | "as" -> P.AS
      | "pass" -> P.PASS
      | "of" -> P.OF
      | "while" -> P.WHILE
      | "type" -> P.TYPE
      | "default" -> P.DEFAULT
      | _ -> P.ID(id)
  }

  | '"' { read_string state (Buffer.create 17) lexbuf }
  | '`' (ident as r) ' ' { 
    let s = read_extern state (Buffer.create 17) lexbuf in
    P.EXTERN(r, s)
  }

  | "+" as op { P.ADD (Char.to_string op) }
  | "-" as op { P.SUB (Char.to_string op) }
  | "*" as op { P.MUL (Char.to_string op) }
  | "/" as op { P.DIV (Char.to_string op) }
  | "==" as op { P.EEQ op }
  | "!=" as op { P.NEQ op }
  | ">=" as op { P.GEQ op }
  | ">" as op { P.GREAT (Char.to_string op) }
  | "<=" as op { P.LEQ op }
  | "<" as op { P.LESS (Char.to_string op) }
  | "/" as op { P.DIV (Char.to_string op) }
  | "|>" as op { P.PIPE op }

  | "(" { ignore_nl state; P.LP }
  | ")" { aware_nl state;  P.RP }
  | "[" { ignore_nl state; P.LS }
  | "]" { aware_nl state;  P.RS }
  | "{" { ignore_nl state; P.LB }
  | "}" { aware_nl state;  P.RB }
  | "=" { P.EQ }
  | ":" { P.COLON }
  | "." { P.DOT }
  | "," { P.COMMA }
  
  | int as i   { P.INT (int_of_string i) }   
  | float as f { P.FLOAT (float_of_string f) }
  
  | eof { P.EOF }
  | _ {
    let tok = L.lexeme lexbuf in
    let pos = L.lexeme_start_p lexbuf in
    let pos_fmt = Format.sprintf "file: %s, line: %d, col: %d" pos.pos_fname pos.pos_lnum pos.pos_cnum in
    SyntaxError (Format.sprintf "unknown token: '%s' at (%s)" tok pos_fmt) |> raise
  }

(* parse indentations *)
and offset state = parse
  | ""   { }
  | ' '  { state.offset <- state.offset + 1; offset state lexbuf }
  | '\t' { state.offset <- state.offset + 8; offset state lexbuf }

(* parse quoted string *)
and read_string state buf = parse
  | '"'           { P.STRING (B.contents buf) }
  | '\\' '/'      { B.add_char buf '/'; read_string state buf lexbuf }
  | '\\' '\\'     { B.add_char buf '\\'; read_string state buf lexbuf }
  | '\\' 'b'      { B.add_char buf '\b'; read_string state buf lexbuf }
  | '\\' 'f'      { B.add_char buf '\012'; read_string state buf lexbuf }
  | '\\' 'n'      { B.add_char buf '\n'; read_string state buf lexbuf }
  | '\\' 'r'      { B.add_char buf '\r'; read_string state buf lexbuf }
  | '\\' 't'      { B.add_char buf '\t'; read_string state buf lexbuf }
  | [^ '"' '\\']+ { B.add_string buf @@ L.lexeme lexbuf; read_string state buf lexbuf }
  | _             { SyntaxError ("Illegal string character: " ^ L.lexeme lexbuf) |> raise }
  | eof           { SyntaxError "String is not terminated" |> raise }

(* parse backquoted string (extern language spec) *)
and read_extern state buf = parse
  | '`'      { B.contents buf }
  | [^ '`']+ { B.add_string buf @@ L.lexeme lexbuf; read_extern state buf lexbuf }
  | _        { SyntaxError ("Illegal extern character: " ^ L.lexeme lexbuf) |> raise }
  | eof      { SyntaxError "Extern is not terminated" |> raise }

(* end of lexer specification *)

{
  let to_string = function
    | P.AND(s) -> "AND"
    | P.AS -> "AS"
    | P.BREAK -> "BREAK"
    | P.COLON -> "COLON"
    | P.COMMA -> "COMMA"
    | P.CONTINUE -> "CONTINUE"
    | P.DEDENT -> "DEDENT"
    | P.DEF -> "DEF"
    | P.DOT -> "DOT"
    | P.ELIF -> "ELIF"
    | P.ELSE -> "ELSE"
    | P.EOF -> "EOF"
    | P.EQ -> "EQ"
    | P.EXTERN(r, s) -> sprintf "EXTERN(%s, `%s`)" r s
    | P.FOR -> "FOR"
    | P.ID(s) -> sprintf "ID(%s)" s 
    | P.IF -> "IF"
    | P.IN -> "IN"
    | P.INDENT -> "INDENT"
    | P.LB -> "LB"
    | P.LP -> "LP"
    | P.LS -> "LS"
    | P.NL -> "NL"
    | P.NOT(s)-> "NOT"
    | P.OR(s) -> "OR"
    | P.PRINT -> "PRINT"
    | P.RB -> "RB"
    | P.RETURN -> "RETURN"
    | P.RP -> "RP"
    | P.RS -> "RS"
    | P.STRING(s) -> sprintf "STRING(%s)" s
    | P.YIELD -> "YIELD"
    | P.MATCH -> "MATCH"
    | P.CASE -> "CASE"
    | P.ADD(s) -> "ADD" 
    | P.SUB(s) -> "SUB" 
    | P.MUL(s) -> "MUL" 
    | P.DIV(s) -> "DIV" 
    | P.GEQ(s) -> "GEQ" 
    | P.GREAT(s) -> "GREAT"
    | P.LEQ(s) -> "LEQ"
    | P.LESS(s) -> "LESS"
    | P.PIPE(s) -> "PIPE"
    | P.INT(s) -> sprintf "INT(%d)" s
    | P.FLOAT(s) -> sprintf "FLOAT(%f)" s
    | P.EEQ(s) -> "EEQ" 
    | P.NEQ(s) -> "NEQ"
    | P.OF -> "OF"
    | P.PASS -> "PASS"
    | P.WHILE -> "WHILE"
    | P.TYPE -> "TYPE"
    | P.DEFAULT -> "DEFAULT"
    | _ -> SyntaxError "unknown token" |> raise
  
  let lexmain () =
    let lexbuf = L.from_channel stdin in
    let state = stack_create () in
    let rec loop level = function
      | P.INDENT as x -> 
        printf "%s\n%s" (to_string x) (String.make (level+1) ' ');
        loop (level + 1) @@ token state lexbuf
      | P.DEDENT as x -> 
        printf "%s\n%s" (to_string x) (String.make (level-1) ' ');
        loop (level - 1) @@ token state lexbuf
      | P.NL as x ->
        printf "%s\n%s" (to_string x) (String.make level ' ');
        loop level @@ token state lexbuf
      | P.EOF -> 
        ()
      | x ->  
        printf "%s " (to_string x);
        loop level @@ token state lexbuf
    in 
    loop 0 @@ token state lexbuf
    
  (* let () = lexmain () *)
}