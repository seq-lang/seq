(* 786 *)
(* Heavily inspired and borrowed from https://github.com/m2ym/ocaml-pythonlib/ *)

{
  module B = Buffer
  module L = Lexing
  module P = Parser

  open Core
  
  exception SyntaxError of string * Lexing.position

  type stack = {
    stack: int Stack.t;
    fname: string;
    mutable offset: int;
    mutable ignore_newline: int;
  }
  
  let stack_create fname =
    let stack = Stack.create () in
    Stack.push stack 0;
    {stack = stack; offset = 0; ignore_newline = 0; fname}
  
  let ignore_nl t =
    t.ignore_newline <- t.ignore_newline + 1
  and aware_nl t =
    t.ignore_newline <- t.ignore_newline - 1

  let cur_pos state (lexbuf: Lexing.lexbuf) = 
    {lexbuf.lex_start_p with pos_fname = state.fname}

  let count_lines s =
    String.fold ~init:0 ~f:(fun acc c -> if c = '\n' then acc + 1 else acc) s

  let seq_string pfx u st =
    let escape = Scanf.unescaped in
    match pfx with
    | "r'" | "R'" -> P.REGEX  (escape u, st)
    | "s'" | "S'" -> P.SEQ    (escape u, st)
    | _           -> P.STRING (escape u, st)
}

(* lexer regex expressions *)

let newline = '\n' | "\r\n"
let white = [' ' '\t']
let comment = '#' [^ '\n' '\r']*

let digit = ['0'-'9']

let int = digit+
let fraction = '.' digit+
let pointfloat = int? fraction | int '.'
let exponent = ['e' 'E'] ['+' '-']? digit+
let expfloat = (int | pointfloat) exponent
let float = pointfloat | expfloat

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
      P.DEDENT (cur_pos state lexbuf)
    end
    else if cur > last then begin
      Stack.push state.stack cur;
      P.INDENT (cur_pos state lexbuf)
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
    if state.ignore_newline <= 0 then begin
      state.offset <- 0;
      offset state lexbuf;
      P.NL (cur_pos state lexbuf)
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
  
  | "is" white+ "not" { P.ISNOT ("is not", cur_pos state lexbuf) }
  | "not" white+ "in" { P.NOTIN ("not in", cur_pos state lexbuf) }
  | ident as id {
    match id with
      | "True"     -> P.TRUE       (cur_pos state lexbuf)
      | "False"    -> P.FALSE      (cur_pos state lexbuf)
      | "if"       -> P.IF         (cur_pos state lexbuf)
      | "elif"     -> P.ELIF       (cur_pos state lexbuf)
      | "else"     -> P.ELSE       (cur_pos state lexbuf)
      | "def"      -> P.DEF        (cur_pos state lexbuf)
      | "for"      -> P.FOR        (cur_pos state lexbuf)
      | "break"    -> P.BREAK      (cur_pos state lexbuf)
      | "continue" -> P.CONTINUE   (cur_pos state lexbuf)
      | "in"       -> P.IN         ("in", cur_pos state lexbuf)
      | "or"       -> P.OR         ("||", cur_pos state lexbuf)
      | "and"      -> P.AND        ("&&", cur_pos state lexbuf)
      | "not"      -> P.NOT        ("!",  cur_pos state lexbuf)
      | "print"    -> P.PRINT      (cur_pos state lexbuf)
      | "return"   -> P.RETURN     (cur_pos state lexbuf)
      | "yield"    -> P.YIELD      (cur_pos state lexbuf)
      | "match"    -> P.MATCH      (cur_pos state lexbuf)
      | "case"     -> P.CASE       (cur_pos state lexbuf)
      | "as"       -> P.AS         (cur_pos state lexbuf)
      | "pass"     -> P.PASS       (cur_pos state lexbuf)
      | "while"    -> P.WHILE      (cur_pos state lexbuf)
      | "type"     -> P.TYPE       (cur_pos state lexbuf)
      | "default"  -> P.DEFAULT    (cur_pos state lexbuf)
      (*| "lambda"   -> P.LAMBDA     (cur_pos state lexbuf)*)
      | "assert"   -> P.ASSERT     (cur_pos state lexbuf)
      | "global"   -> P.GLOBAL     (cur_pos state lexbuf)
      | "import"   -> P.IMPORT     (cur_pos state lexbuf)
      | "from"     -> P.FROM       (cur_pos state lexbuf)
      | "class"    -> P.CLASS      (cur_pos state lexbuf)
      | "typeof"   -> P.TYPEOF     (cur_pos state lexbuf)
      | "extend"   -> P.EXTEND     (cur_pos state lexbuf)
      | "extern"   -> P.EXTERN     (cur_pos state lexbuf)
      | "None"     -> P.NONE       (cur_pos state lexbuf)
      | "is"       -> P.IS         ("is", cur_pos state lexbuf)
      | _          -> P.ID         (id, cur_pos state lexbuf)
  }

  | stringprefix '\''     { single_string state (L.lexeme lexbuf) lexbuf }
  | stringprefix '"'      { double_string state (L.lexeme lexbuf) lexbuf }
  | stringprefix "'''"    { single_docstr state (L.lexeme lexbuf) lexbuf }
  | stringprefix "\"\"\"" { double_docstr state (L.lexeme lexbuf) lexbuf }

  | '`' (ident as gen) { P.GENERIC ("`" ^ gen, cur_pos state lexbuf) }
(*  | (ident as r) '`' { 
    let s = read_extern state (Buffer.create 17) lexbuf in
    P.EXTERN(r, s, cur_pos state lexbuf)
  }*)

  | "(" { ignore_nl state; P.LP (cur_pos state lexbuf) }
  | ")" { aware_nl state;  P.RP (cur_pos state lexbuf) }
  | "[" { ignore_nl state; P.LS (cur_pos state lexbuf) }
  | "]" { aware_nl state;  P.RS (cur_pos state lexbuf) }
  | "{" { ignore_nl state; P.LB (cur_pos state lexbuf) }
  | "}" { aware_nl state;  P.RB (cur_pos state lexbuf) }

  | "<<"  as op { P.B_LSH (op, cur_pos state lexbuf) }
  | ">>"  as op { P.B_RSH (op, cur_pos state lexbuf) }
  | "&"   as op { P.B_AND (Char.to_string op, cur_pos state lexbuf) }
  | "^"   as op { P.B_XOR (Char.to_string op, cur_pos state lexbuf) }
  | "~"   as op { P.B_NOT (Char.to_string op, cur_pos state lexbuf) }
  | "|>"  as op { P.PIPE  (op, cur_pos state lexbuf) }
  | "|"   as op { P.B_OR  (Char.to_string op, cur_pos state lexbuf) }
  
  | ":="  { P.ASSGN_EQ  (cur_pos state lexbuf) }
  | "="   { P.EQ        (cur_pos state lexbuf) }
  | "->"  { P.OF        (cur_pos state lexbuf) }
  | ":"   { P.COLON     (cur_pos state lexbuf) }
  | ";"   { P.SEMICOLON (cur_pos state lexbuf) }
  | "@"   { P.AT        (cur_pos state lexbuf) }
  | ","   { P.COMMA     (cur_pos state lexbuf) }
  | "..." { P.ELLIPSIS  (cur_pos state lexbuf) }
  | "."   { P.DOT       (cur_pos state lexbuf) }

  | "+="  as op { P.PLUSEQ (op, cur_pos state lexbuf) }
  | "-="  as op { P.MINEQ  (op, cur_pos state lexbuf) }
  | "**=" as op { P.POWEQ  (op, cur_pos state lexbuf) }
  | "*="  as op { P.MULEQ  (op, cur_pos state lexbuf) }
  | "//=" as op { P.FDIVEQ (op, cur_pos state lexbuf) }
  | "/="  as op { P.DIVEQ  (op, cur_pos state lexbuf) }
  | "%="  as op { P.MODEQ  (op, cur_pos state lexbuf) }
  | "+"   as op { P.ADD    (Char.to_string op, cur_pos state lexbuf) }
  | "-"   as op { P.SUB    (Char.to_string op, cur_pos state lexbuf) }
  | "**"  as op { P.POW    (op, cur_pos state lexbuf) }
  | "*"   as op { P.MUL    (Char.to_string op, cur_pos state lexbuf) }
  | "=="  as op { P.EEQ    (op, cur_pos state lexbuf) }
  | "!="  as op { P.NEQ    (op, cur_pos state lexbuf) }
  | ">="  as op { P.GEQ    (op, cur_pos state lexbuf) }
  | ">"   as op { P.GREAT  (Char.to_string op, cur_pos state lexbuf) }
  | "<="  as op { P.LEQ    (op, cur_pos state lexbuf) }
  | "<"   as op { P.LESS   (Char.to_string op, cur_pos state lexbuf) }
  | "//"  as op { P.FDIV   (op, cur_pos state lexbuf) }
  | "/"   as op { P.DIV    (Char.to_string op, cur_pos state lexbuf) }
  | "%"   as op { P.MOD    (Char.to_string op, cur_pos state lexbuf) }

  | int as i   { P.INT   (int_of_string i, cur_pos state lexbuf) }   
  | float as f { P.FLOAT (float_of_string f, cur_pos state lexbuf) }
  
  | eof { P.EOF (cur_pos state lexbuf) }
  | _ {
    let tok = L.lexeme lexbuf in
    let pos = cur_pos state lexbuf in
    SyntaxError (Format.sprintf "Unknown token '%s'" tok, pos) |> raise
  }

(* parse indentations *)
and offset state = parse
  | ""   { }
  | ' '  { state.offset <- state.offset + 1; offset state lexbuf }
  | '\t' { state.offset <- state.offset + 8; offset state lexbuf }

(* parse strings *)
and single_string state prefix = parse
  | (([^ '\\' '\r' '\n' '\''] | escape)* as s) '\'' { seq_string prefix s (cur_pos state lexbuf) }
and single_docstr state prefix = shortest
  | (([^ '\\'] | escape)* as s) "'''" { 
    let lines = count_lines s in  
    lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with pos_lnum = lexbuf.lex_curr_p.pos_lnum + lines };
    seq_string prefix s (cur_pos state lexbuf)
  }
and double_string state prefix = parse
  | (([^ '\\' '\r' '\n' '\"'] | escape)* as s) '"' { seq_string prefix s (cur_pos state lexbuf) }
and double_docstr state prefix = shortest
  | (([^ '\\'] | escape)* as s) "\"\"\"" { 
    let lines = count_lines s in
    lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with pos_lnum = lexbuf.lex_curr_p.pos_lnum + lines };
    seq_string prefix s (cur_pos state lexbuf)
  }

(* parse backquoted string (extern language spec) *)
and read_extern state buf = parse
  | '`'      { B.contents buf }
  | [^ '`']+ { B.add_string buf @@ L.lexeme lexbuf; read_extern state buf lexbuf }
  | _        { SyntaxError ("Illegal extern character: " ^ L.lexeme lexbuf, cur_pos state lexbuf) |> raise }
  | eof      { SyntaxError ("Extern is not terminated", cur_pos state lexbuf) |> raise }

(* end of lexer specification *)
