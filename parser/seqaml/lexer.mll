(* *****************************************************************************
 * Seqaml.Lexer: Lexing module
 *
 * Author: inumanag
 * License: see LICENSE
 *
 * Heavily inspired and borrowed from https://github.com/m2ym/ocaml-pythonlib
 * *****************************************************************************)

{
  module B = Buffer
  module L = Lexing
  module P = Grammar

  open Core

  (* Used for tracking indentation levels *)
  type stack =
    { stack: int Stack.t;
      fname: string;
      mutable offset: int;
      mutable ignore_newline: int; }

  let stack_create fname =
    let stack = Stack.create () in
    Stack.push stack 0;
    { stack = stack; offset = 0; ignore_newline = 0; fname }

  (* flags to set should we ignore newlines (e.g. within parentheses) or not *)
  let ignore_nl t =
    t.ignore_newline <- t.ignore_newline + 1
  and aware_nl t =
    t.ignore_newline <- t.ignore_newline - 1

  (* get current file position from lexer *)
  let cur_pos state ?(len=1) (lexbuf: Lexing.lexbuf) =
    Ast.Ann.
      { file = state.fname;
        line = lexbuf.lex_start_p.pos_lnum;
        col = lexbuf.lex_start_p.pos_cnum - lexbuf.lex_start_p.pos_bol;
        len }

  let count_lines s =
    String.fold ~init:0 ~f:(fun acc c -> if c = '\n' then acc + 1 else acc) s

  (* given a (named) string (e.g. s'foo' or 'bar'), decide the proper token *)
  let seq_string pfx u st =
    let fix_literals ?(is_raw=false) s =
      let buf = Buffer.create (String.length s) in
      let rec scan i =
        let l = String.length s in
        let is_octal c =
          (Char.to_int c) >= (Char.to_int '0') && (Char.to_int c) <= (Char.to_int '7')
        in
        if i >= l then ()
        else if i < (l - 1) && s.[i] = '\\' && (not is_raw) then begin
          let skip, c = match s.[i + 1] with
            | '\\' -> 1, '\\'
            | '\'' -> 1, '\''
            | '"' -> 1, '"'
            | 'a' -> 1, Char.of_int_exn 7
            | 'b' -> 1, '\b'
            | 'f' -> 1, Char.of_int_exn 12
            | 'n' -> 1, '\n'
            | 'r' -> 1, '\r'
            | 't' -> 1, '\t'
            | 'v' -> 1, Char.of_int_exn 11
            | 'x' ->
              let n =
                if i < (l - 3) then
                  int_of_string_opt ("0x" ^ (String.sub s ~pos:(i + 2) ~len:2))
                else
                  None
              in
              begin match n with
              | Some n -> 3, Char.of_int_exn n
              | None -> Err.SyntaxError ("Invalid \\x escape", st) |> raise
              end
            | c when is_octal c ->
              let n =
                if i < (l - 3) && (is_octal s.[i + 2]) && (is_octal s.[i + 3]) then 3
                else if i < (l - 2) && (is_octal s.[i + 2]) then 2
                else 1
              in
              n, Char.of_int_exn
                 @@ int_of_string ("0o" ^ (String.sub s ~pos:(i + 1) ~len:n))
            | _ -> 0, s.[i]
          in
          Buffer.add_char buf c;
          scan (i + 1 + skip)
        end else begin
          Buffer.add_char buf s.[i];
          scan (i + 1)
        end
      in
      scan 0;
      Buffer.contents buf
    in
    match String.lowercase (String.prefix pfx 1) with
    | "r" -> P.STRING (st, fix_literals ~is_raw:true u)
    | ("s" | "p") as p -> P.SEQ (st, p, fix_literals u)
    | "k" -> P.KMER (st, fix_literals u)
    | _ -> P.STRING (st, fix_literals u)
}

(* Lexer regex expressions *)

let newline = '\n' | "\r\n"
let white = [' ' '\t']
let comment = '#' [^ '\n' '\r']*

let digit = ['0'-'9']
let hexdigit = ['0'-'9' 'a'-'f' 'A'-'F']

let int = digit+
let hexint = '0' ['x' 'X'] hexdigit+
let fraction = '.' digit+
let pointfloat = int? fraction | int '.'
let exponent = ['e' 'E'] ['+' '-']? digit+
let expfloat = (int | pointfloat) exponent
let float = pointfloat | expfloat

let escape = '\\' _

let alpha = ['a'-'z' 'A'-'Z' '_']
let alphanum = ['A'-'Z' 'a'-'z' '0'-'9' '_']

let stringprefix = ('s' | 'S')? ('r' | 'R')? ('k' | 'K')? ('p' | 'P')?
let intsuffix = ('s' | 'S' | 'z' | 'Z' | 'u' | 'U')

let ident = alpha alphanum*

(* Rules *)

(* Main handler *)
rule token state = parse
  | "" {
    (* TODO: Python-style indentation detection (i.e. more strict rules) *)
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

(* Token rules *)
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

  | "is" white+ "not" white+
    { P.ISNOT (cur_pos state lexbuf ~len:6, "is not") }
  | "not" white+ "in" white+
    { P.NOTIN (cur_pos state lexbuf ~len:6, "not in") }
  | ("import!" as id) white+
    { P.IMPORT_CONTEXT (cur_pos state lexbuf ~len:(String.length id)) }
  | ident as id {
    let len = String.length id in
    match id with
      | "True"     -> P.TRUE       (cur_pos state lexbuf ~len)
      | "False"    -> P.FALSE      (cur_pos state lexbuf ~len)
      | "if"       -> P.IF         (cur_pos state lexbuf ~len)
      | "elif"     -> P.ELIF       (cur_pos state lexbuf ~len)
      | "else"     -> P.ELSE       (cur_pos state lexbuf ~len)
      | "def"      -> P.DEF        (cur_pos state lexbuf ~len)
      | "for"      -> P.FOR        (cur_pos state lexbuf ~len)
      | "break"    -> P.BREAK      (cur_pos state lexbuf ~len)
      | "continue" -> P.CONTINUE   (cur_pos state lexbuf ~len)
      | "print"    -> P.PRINT      (cur_pos state lexbuf ~len)
      | "return"   -> P.RETURN     (cur_pos state lexbuf ~len)
      | "yield"    -> P.YIELD      (cur_pos state lexbuf ~len)
      | "match"    -> P.MATCH      (cur_pos state lexbuf ~len)
      | "case"     -> P.CASE       (cur_pos state lexbuf ~len)
      | "as"       -> P.AS         (cur_pos state lexbuf ~len)
      | "pass"     -> P.PASS       (cur_pos state lexbuf ~len)
      | "while"    -> P.WHILE      (cur_pos state lexbuf ~len)
      | "type"     -> P.TYPE       (cur_pos state lexbuf ~len)
      | "default"  -> P.DEFAULT    (cur_pos state lexbuf ~len)
      | "lambda"   -> P.LAMBDA     (cur_pos state lexbuf ~len)
      | "assert"   -> P.ASSERT     (cur_pos state lexbuf ~len)
      | "global"   -> P.GLOBAL     (cur_pos state lexbuf ~len)
      | "import"   -> P.IMPORT     (cur_pos state lexbuf ~len)
      | "from"     -> P.FROM       (cur_pos state lexbuf ~len)
      | "class"    -> P.CLASS      (cur_pos state lexbuf ~len)
      | "typeof"   -> P.TYPEOF     (cur_pos state lexbuf ~len)
      | "__ptr__"  -> P.PTR        (cur_pos state lexbuf ~len)
      | "extend"   -> P.EXTEND     (cur_pos state lexbuf ~len)
      | "cdef"     -> P.EXTERN     (cur_pos state lexbuf ~len)
      | "del"      -> P.DEL        (cur_pos state lexbuf ~len)
      | "None"     -> P.NONE       (cur_pos state lexbuf ~len)
      | "try"      -> P.TRY        (cur_pos state lexbuf ~len)
      | "except"   -> P.EXCEPT     (cur_pos state lexbuf ~len)
      | "finally"  -> P.FINALLY    (cur_pos state lexbuf ~len)
      | "prefetch" -> P.PREFETCH   (cur_pos state lexbuf ~len)
      | "with"     -> P.WITH       (cur_pos state lexbuf ~len)
      | "raise"    -> P.THROW      (cur_pos state lexbuf ~len)
      | "is"       -> P.IS         (cur_pos state lexbuf ~len, "is")
      | "in"       -> P.IN         (cur_pos state lexbuf ~len, "in")
      | "or"       -> P.OR         (cur_pos state lexbuf ~len, "||")
      | "and"      -> P.AND        (cur_pos state lexbuf ~len, "&&")
      | "not"      -> P.NOT        (cur_pos state lexbuf ~len, "!")
      | _          -> P.ID         (cur_pos state lexbuf ~len, id)
  }

  | stringprefix '\''     { single_string state (L.lexeme lexbuf) lexbuf }
  | stringprefix '"'      { double_string state (L.lexeme lexbuf) lexbuf }
  | stringprefix "'''"    { single_docstr state (L.lexeme lexbuf) lexbuf }
  | stringprefix "\"\"\"" { double_docstr state (L.lexeme lexbuf) lexbuf }

  | "$" { escaped_id state lexbuf }
  | "(" { ignore_nl state; P.LP (cur_pos state lexbuf) }
  | ")" { aware_nl state;  P.RP (cur_pos state lexbuf) }
  | "[" { ignore_nl state; P.LS (cur_pos state lexbuf) }
  | "]" { aware_nl state;  P.RS (cur_pos state lexbuf) }
  | "{" { ignore_nl state; P.LB (cur_pos state lexbuf) }
  | "}" { aware_nl state;  P.RB (cur_pos state lexbuf) }

  | "|="  as op { P.OREQ   (cur_pos state lexbuf ~len:2, op) }
  | "&="  as op { P.ANDEQ  (cur_pos state lexbuf ~len:2, op) }
  | "^="  as op { P.XOREQ  (cur_pos state lexbuf ~len:2, op) }
  | "<<=" as op { P.LSHEQ (cur_pos state lexbuf ~len:3, op) }
  | ">>=" as op { P.RSHEQ (cur_pos state lexbuf ~len:3, op) }
  | "<<"  as op { P.B_LSH (cur_pos state lexbuf ~len:2, op) }
  | ">>"  as op { P.B_RSH (cur_pos state lexbuf ~len:2, op) }
  | "&"   as op { P.B_AND (cur_pos state lexbuf, Char.to_string op) }
  | "^"   as op { P.B_XOR (cur_pos state lexbuf, Char.to_string op) }
  | "~"   as op { P.B_NOT (cur_pos state lexbuf, Char.to_string op) }
  | "||>" as op { P.PPIPE (cur_pos state lexbuf ~len:2, op) }
  | ">|"  as op { P.SPIPE (cur_pos state lexbuf ~len:2, op) }
  | "|>"  as op { P.PIPE  (cur_pos state lexbuf ~len:2, op) }
  | "|"   as op { P.B_OR  (cur_pos state lexbuf, Char.to_string op) }
  | ":="  as op { P.ASSGN_EQ  (cur_pos state lexbuf ~len:2, op) }
  | "="   as op { P.EQ        (cur_pos state lexbuf, Char.to_string op) }
  | "..." as op { P.ELLIPSIS  (cur_pos state lexbuf ~len:3, op) }
  | "@"   as op { P.AT        (cur_pos state lexbuf, Char.to_string op) }

  | "->"  { P.OF        (cur_pos state lexbuf ~len:2) }
  | ":"   { P.COLON     (cur_pos state lexbuf) }
  | ";"   { P.SEMICOLON (cur_pos state lexbuf) }
  | ","   { P.COMMA     (cur_pos state lexbuf) }
  | "."   { P.DOT       (cur_pos state lexbuf) }

  | "+="  as op { P.PLUSEQ (cur_pos state lexbuf ~len:2, op) }
  | "-="  as op { P.MINEQ  (cur_pos state lexbuf ~len:2, op) }
  | "**=" as op { P.POWEQ  (cur_pos state lexbuf ~len:3, op) }
  | "*="  as op { P.MULEQ  (cur_pos state lexbuf ~len:2, op) }
  | "//=" as op { P.FDIVEQ (cur_pos state lexbuf ~len:3, op) }
  | "/="  as op { P.DIVEQ  (cur_pos state lexbuf ~len:2, op) }
  | "%="  as op { P.MODEQ  (cur_pos state lexbuf ~len:2, op) }
  | "+"   as op { P.ADD    (cur_pos state lexbuf, Char.to_string op) }
  | "-"   as op { P.SUB    (cur_pos state lexbuf, Char.to_string op) }
  | "**"  as op { P.POW    (cur_pos state lexbuf ~len:2, op) }
  | "*"   as op { P.MUL    (cur_pos state lexbuf, Char.to_string op) }
  | "=="  as op { P.EEQ    (cur_pos state lexbuf ~len:2, op) }
  | "!="  as op { P.NEQ    (cur_pos state lexbuf ~len:2, op) }
  | ">="  as op { P.GEQ    (cur_pos state lexbuf ~len:2, op) }
  | ">"   as op { P.GREAT  (cur_pos state lexbuf, Char.to_string op) }
  | "<="  as op { P.LEQ    (cur_pos state lexbuf ~len:2, op) }
  | "<"   as op { P.LESS   (cur_pos state lexbuf, Char.to_string op) }
  | "//"  as op { P.FDIV   (cur_pos state lexbuf ~len:2, op) }
  | "/"   as op { P.DIV    (cur_pos state lexbuf, Char.to_string op) }
  | "%"   as op { P.MOD    (cur_pos state lexbuf, Char.to_string op) }

  | (int | hexint) as i
    { P.INT (cur_pos state lexbuf ~len:(String.length i), i) }
  | float as f
    { P.FLOAT (cur_pos state lexbuf ~len:(String.length f),
        float_of_string f) }

  | ((int | hexint) as i) (intsuffix as k)
    { P.INT_S (cur_pos state lexbuf ~len:(String.length i),
        (i, Char.to_string k)) }
  | (float as f) (intsuffix as k)
    { P.FLOAT_S (cur_pos state lexbuf ~len:(String.length f),
        (float_of_string f, Char.to_string k)) }

  | eof { P.EOF (cur_pos state lexbuf) }
  | _ {
    let tok = L.lexeme lexbuf in
    let pos = cur_pos state lexbuf in
    Err.SyntaxError (Format.sprintf "Unknown token '%s'" tok, pos) |> raise
  }

(* Indentation rules *)
and offset state = parse
  | ""   { }
  | ' '  { state.offset <- state.offset + 1; offset state lexbuf }
  | '\t' { state.offset <- state.offset + 8; offset state lexbuf }

(* String rules *)
and single_string state prefix = parse
  | (([^ '\\' '\r' '\n' '\''] | escape)* as s) '\'' {
    let len = (String.length s) + (String.length prefix) in
    seq_string prefix s (cur_pos state lexbuf ~len)
  }
and single_docstr state prefix = shortest
  | (([^ '\\'] | escape)* as s) "'''" {
    let lines = count_lines s in
    lexbuf.lex_curr_p <-
      { lexbuf.lex_curr_p with pos_lnum = lexbuf.lex_curr_p.pos_lnum + lines };
    let len = (String.length s) + (String.length prefix) in
    seq_string prefix s (cur_pos state lexbuf ~len)
  }
and double_string state prefix = parse
  | (([^ '\\' '\r' '\n' '\"'] | escape)* as s) '"' {
    let len = (String.length s) + (String.length prefix) in
    seq_string prefix s (cur_pos state lexbuf ~len)
  }
and double_docstr state prefix = shortest
  | (([^ '\\'] | escape)* as s) "\"\"\"" {
    let lines = count_lines s in
    lexbuf.lex_curr_p <-
      { lexbuf.lex_curr_p with pos_lnum = lexbuf.lex_curr_p.pos_lnum + lines };
    let len = (String.length s) + (String.length prefix) in
    seq_string prefix s (cur_pos state lexbuf ~len)
  }

and escaped_id state = parse
  | (([^ '\r' '\n' '$' ])* as s) '$' {
    let len = (String.length s) in
    P.ID (cur_pos state lexbuf ~len, s)
  }

