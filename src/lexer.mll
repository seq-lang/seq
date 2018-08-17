(* 786 *)
(* Heavily inspired and borrowed from https://github.com/m2ym/ocaml-pythonlib/ *)

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

  let seq_string pfx u =
    match pfx with
    | "r'" | "R'" -> P.REGEX u
    | "s'" | "S'" -> P.SEQ u
    | _ -> P.STRING u
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

let stringprefix = ('s' | 'S')? ('r' | 'R')?

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
      | "while" -> P.WHILE
      | "type" -> P.TYPE
      | "default" -> P.DEFAULT
      | "lambda" -> P.LAMBDA
      | "assert" -> P.ASSERT
      | "global" -> P.GLOBAL
      | "import" -> P.IMPORT
      | "from" -> P.FROM
      | _ -> P.ID(id)
  }

  | stringprefix '\''     { single_string state (L.lexeme lexbuf) lexbuf }
  | stringprefix '"'      { double_string state (L.lexeme lexbuf) lexbuf }
  | stringprefix "'''"    { single_docstr state (L.lexeme lexbuf) lexbuf }
  | stringprefix "\"\"\"" { double_docstr state (L.lexeme lexbuf) lexbuf }
  | '`' (ident as r) ' ' { 
    let s = read_extern state (Buffer.create 17) lexbuf in
    P.EXTERN(r, s)
  }

  | "+=" as op { P.PLUSEQ op }
  | "-=" as op { P.MINEQ op }
  | "**=" as op { P.POWEQ op }
  | "*=" as op { P.MULEQ op }
  | "//=" as op { P.FDIVEQ op }
  | "/=" as op { P.DIVEQ op }
  | "%=" as op { P.MODEQ op }

  | "->" { P.OF }
  | "+" as op { P.ADD (Char.to_string op) }
  | "-" as op { P.SUB (Char.to_string op) }
  | "**" as op { P.POW op }
  | "*" as op { P.MUL (Char.to_string op) }
  | "==" as op { P.EEQ op }
  | "!=" as op { P.NEQ op }
  | ">=" as op { P.GEQ op }
  | ">" as op { P.GREAT (Char.to_string op) }
  | "<=" as op { P.LEQ op }
  | "<" as op { P.LESS (Char.to_string op) }
  | "//" as op { P.FDIV op }
  | "/" as op { P.DIV (Char.to_string op) }
  | "%" as op { P.MOD (Char.to_string op) }
  | "|>" as op { P.PIPE op }

  | "(" { ignore_nl state; P.LP }
  | ")" { aware_nl state;  P.RP }
  | "[" { ignore_nl state; P.LS }
  | "]" { aware_nl state;  P.RS }
  | "{" { ignore_nl state; P.LB }
  | "}" { aware_nl state;  P.RB }
  | "=" { P.EQ }
  | ":" { P.COLON }
  | ";" { P.SEMICOLON }
  | "@" { P.AT }
  | "," { P.COMMA }
  | "..." { P.ELLIPSIS}
  | "." { P.DOT }
  
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

(* parse strings *)
and single_string state prefix = parse
  | (([^ '\\' '\r' '\n' '\''] | escape)* as s) '\'' { seq_string prefix s }
and single_docstr state prefix = shortest
  | (([^ '\\'] | escape)* as s) "'''" { 
    let lines = count_lines s in  
    lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with pos_lnum = lexbuf.lex_curr_p.pos_lnum + lines };
    seq_string prefix s
  }
and double_string state prefix = parse
  | (([^ '\\' '\r' '\n' '\"'] | escape)* as s) '"' { seq_string prefix s }
and double_docstr state prefix = shortest
  | (([^ '\\'] | escape)* as s) "\"\"\"" { 
    let lines = count_lines s in
    lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with pos_lnum = lexbuf.lex_curr_p.pos_lnum + lines };
    seq_string prefix s
  }

(* parse backquoted string (extern language spec) *)
and read_extern state buf = parse
  | '`'      { B.contents buf }
  | [^ '`']+ { B.add_string buf @@ L.lexeme lexbuf; read_extern state buf lexbuf }
  | _        { SyntaxError ("Illegal extern character: " ^ L.lexeme lexbuf) |> raise }
  | eof      { SyntaxError "Extern is not terminated" |> raise }

(* end of lexer specification *)
