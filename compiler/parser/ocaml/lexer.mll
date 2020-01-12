(* *****************************************************************************
 * Seq.Lexer: Lexing module
 *
 * Author: inumanag
 * License: see LICENSE
 *
 * Heavily inspired and borrowed from https://github.com/m2ym/ocaml-pythonlib
 * *****************************************************************************)

{
  module L = Lexing
  module P = Grammar

  type offset = { mutable line: int ; mutable col: int }
  let global_offset = { line = 0; col = 0 }
  type stack = (* Used for tracking indentation levels *)
    { stack: int Stack.t
    ; fname: string
    ; mutable offset: int
    ; mutable ignore_newline: int
    }

  let char_to_string = String.make 1
  let ignore_nl t = t.ignore_newline <- t.ignore_newline + 1
  and aware_nl t = t.ignore_newline <- t.ignore_newline - 1
  let count_lines s =
    let i = ref 0 in String.iter (fun c -> if c = '\n' then incr i) s; !i
  (* given a (named) string (e.g. s'foo' or 'bar'), decide the proper token *)
  let seq_string pfx u pos =
    let fix_literals ?(is_raw=false) s =
      let buf = Buffer.create (String.length s) in
      let rec scan i =
        let l = String.length s in
        let is_octal c = Char.((code c) >= (code '0') && (code c) <= (code '7')) in
        if i >= l then ()
        else if i < (l - 1) && s.[i] = '\\' && (not is_raw) then (
          let skip, c = match s.[i + 1] with
            | '\\' -> 1, '\\'
            | '\'' -> 1, '\''
            | '"' -> 1, '"'
            | 'a' -> 1, Char.chr 7
            | 'b' -> 1, '\b'
            | 'f' -> 1, Char.chr 12
            | 'n' -> 1, '\n'
            | 'r' -> 1, '\r'
            | 't' -> 1, '\t'
            | 'v' -> 1, Char.chr 11
            | 'x' ->
              let n =
                if i < (l - 3) then int_of_string_opt ("0x" ^ (String.sub s (i + 2) 2))
                else None
              in
              (match n with
              | Some n -> 3, Char.chr n
              | None -> raise (Ast.SyntaxError ("Invalid \\x escape", pos)))
            | c when is_octal c ->
              let n =
                if i < (l - 3) && (is_octal s.[i + 2]) && (is_octal s.[i + 3]) then 3
                else if i < (l - 2) && (is_octal s.[i + 2]) then 2
                else 1
              in
              n, Char.chr (int_of_string ("0o" ^ (String.sub s (i + 1) n)))
            | _ -> 0, s.[i]
          in
          Buffer.add_char buf c;
          scan (i + 1 + skip)
        ) else (Buffer.add_char buf s.[i]; scan (i + 1))
      in
      scan 0;
      Buffer.contents buf
    in
    match String.lowercase_ascii (String.sub pfx 0 1) with
    | "r" -> P.STRING (fix_literals ~is_raw:true u)
    | ("s" | "p") as p -> P.SEQ (p, fix_literals u)
    | "k" -> P.KMER (fix_literals u)
    | "f" -> P.FSTRING (fix_literals u)
    | _ -> P.STRING (fix_literals u)
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
let stringprefix = ('s' | 'S')? ('r' | 'R')? ('k' | 'K')? ('p' | 'P')? ('f' | 'F')?
let intsuffix = ('s' | 'S' | 'z' | 'Z' | 'u' | 'U')
let ident = alpha alphanum*

(* Main handler *)
rule token state = parse
  | ""
    { (* TODO: Python-style indentation detection (i.e. more strict rules) *)
      let cur = state.offset in
      let last = Stack.top state.stack in
      if cur < last then (ignore (Stack.pop state.stack); P.DEDENT)
      else if cur > last then (Stack.push cur state.stack; P.INDENT)
      else read state lexbuf (* go ahead with parsing *) }

(* Token rules *)
and read state = parse
  | ((white* comment? newline)* white* comment?) newline
    { let lines = count_lines (L.lexeme lexbuf) in
      lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with
        pos_lnum = lexbuf.lex_curr_p.pos_lnum + lines
      ; pos_bol = lexbuf.lex_curr_p.pos_cnum
      };
      if state.ignore_newline <= 0 then (state.offset <- 0; offset state lexbuf; P.NL)
      else read state lexbuf }
  | '\\' newline white*
    { lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with
        pos_lnum = lexbuf.lex_curr_p.pos_lnum + 1
      ; pos_bol = lexbuf.lex_curr_p.pos_cnum
      };
      read state lexbuf }
  | white+ { read state lexbuf }

  | "is" white+ "not" white+ { P.ISNOT "is not" }
  | "not" white+ "in" white+ { P.NOTIN "not in" }
  | ident as id
    { match id with
      | "True"     -> P.TRUE
      | "False"    -> P.FALSE
      | "if"       -> P.IF
      | "elif"     -> P.ELIF
      | "else"     -> P.ELSE
      | "def"      -> P.DEF
      | "for"      -> P.FOR
      | "break"    -> P.BREAK
      | "continue" -> P.CONTINUE
      | "print"    -> P.PRINT
      | "return"   -> P.RETURN
      | "yield"    -> P.YIELD
      | "match"    -> P.MATCH
      | "case"     -> P.CASE
      | "as"       -> P.AS
      | "pass"     -> P.PASS
      | "while"    -> P.WHILE
      | "type"     -> P.TYPE
      | "lambda"   -> P.LAMBDA
      | "assert"   -> P.ASSERT
      | "global"   -> P.GLOBAL
      | "import"   -> P.IMPORT
      | "from"     -> P.FROM
      | "class"    -> P.CLASS
      | "typeof"   -> P.TYPEOF
      | "__ptr__"  -> P.PTR
      | "extend"   -> P.EXTEND
      | "cimport"  -> P.EXTERN "c"
      | "pyimport" -> P.EXTERN "py"
      | "pydef"    -> P.PYDEF
      | "del"      -> P.DEL
      | "None"     -> P.NONE
      | "try"      -> P.TRY
      | "except"   -> P.EXCEPT
      | "finally"  -> P.FINALLY
      | "prefetch" -> P.PREFETCH
      | "with"     -> P.WITH
      | "raise"    -> P.THROW
      | "is"       -> P.IS "is"
      | "in"       -> P.IN "in"
      | "or"       -> P.OR "||"
      | "and"      -> P.AND "&&"
      | "not"      -> P.NOT "!"
      | _          -> P.ID id }

  | stringprefix '\''     { single_string state (L.lexeme lexbuf) lexbuf }
  | stringprefix '"'      { double_string state (L.lexeme lexbuf) lexbuf }
  | stringprefix "'''"    { single_docstr state (L.lexeme lexbuf) lexbuf }
  | stringprefix "\"\"\"" { double_docstr state (L.lexeme lexbuf) lexbuf }

  (* | "$" { escaped_id state lexbuf } *)
  | "(" { ignore_nl state; P.LP }
  | ")" { aware_nl state; P.RP }
  | "[" { ignore_nl state; P.LS }
  | "]" { aware_nl state; P.RS }
  | "{" { ignore_nl state; P.LB }
  | "}" { aware_nl state; P.RB }

  | "|="  as op { P.OREQ op }
  | "&="  as op { P.ANDEQ op }
  | "^="  as op { P.XOREQ op }
  | "<<=" as op { P.LSHEQ op }
  | ">>=" as op { P.RSHEQ op }
  | "<<"  as op { P.B_LSH op }
  | ">>"  as op { P.B_RSH op }
  | "&"   as op { P.B_AND (char_to_string op) }
  | "^"   as op { P.B_XOR (char_to_string op) }
  | "~"   as op { P.B_NOT (char_to_string op) }
  | "||>" as op { P.PPIPE op }
  | ">|"  as op { P.SPIPE op }
  | "|>"  as op { P.PIPE  op }
  | "|"   as op { P.B_OR  (char_to_string op) }
  | ":="  as op { P.ASSGN_EQ  op }
  | "="   as op { P.EQ        (char_to_string op) }
  | "..." as op { P.ELLIPSIS  op }
  | "@"   as op { P.AT        (char_to_string op) }
  | "->"  { P.OF }
  | ":"   { P.COLON }
  | ";"   { P.SEMICOLON }
  | ","   { P.COMMA }
  | "."   { P.DOT }
  | "+="  as op { P.PLUSEQ op }
  | "-="  as op { P.MINEQ op }
  | "**=" as op { P.POWEQ op }
  | "*="  as op { P.MULEQ op }
  | "//=" as op { P.FDIVEQ op }
  | "/="  as op { P.DIVEQ op }
  | "%="  as op { P.MODEQ op }
  | "+"   as op { P.ADD (char_to_string op) }
  | "-"   as op { P.SUB (char_to_string op) }
  | "**"  as op { P.POW op }
  | "*"   as op { P.MUL (char_to_string op) }
  | "=="  as op { P.EEQ op }
  | "!="  as op { P.NEQ op }
  | ">="  as op { P.GEQ op }
  | ">"   as op { P.GREAT (char_to_string op) }
  | "<="  as op { P.LEQ op }
  | "<"   as op { P.LESS (char_to_string op) }
  | "//"  as op { P.FDIV op }
  | "/"   as op { P.DIV (char_to_string op) }
  | "%"   as op { P.MOD (char_to_string op) }

  | (int | hexint) as i { P.INT_S (i, "") }
  | float as f { P.FLOAT_S (float_of_string f, "") }
  | ((int | hexint) as i) (intsuffix as k) { P.INT_S (i, char_to_string k) }
  | (float as f) (intsuffix as k) { P.FLOAT_S (float_of_string f, char_to_string k) }
  | eof { P.EOF }
  | _ { raise (Ast.SyntaxError (Format.sprintf "Unknown token '%s'" (L.lexeme lexbuf), lexbuf.lex_start_p)) }

(* Indentation rules *)
and offset state = parse
  | ""   { }
  | ' '  { state.offset <- state.offset + 1; offset state lexbuf }
  | '\t' { state.offset <- state.offset + 8; offset state lexbuf }

(* String rules *)
and single_string state prefix = parse
  | (([^ '\\' '\r' '\n' '\''] | escape)* as s) '\'' { seq_string prefix s lexbuf.lex_start_p }
  | ([^ '\\' '\r' '\n' '\''] | escape)* (newline | eof) { raise (Ast.SyntaxError ("string not closed", lexbuf.lex_start_p)) }
and single_docstr state prefix = shortest
  | (([^ '\\'] | escape)* as s) "'''"
    { lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with pos_lnum = lexbuf.lex_curr_p.pos_lnum + (count_lines s) };
      seq_string prefix s lexbuf.lex_start_p }
  | ([^ '\\'] | escape)* eof { raise (Ast.SyntaxError ("string not closed", lexbuf.lex_start_p)) }
and double_string state prefix = parse
  | (([^ '\\' '\r' '\n' '\"'] | escape)* as s) '"' { seq_string prefix s lexbuf.lex_start_p }
  | ([^ '\\' '\r' '\n' '\"'] | escape)* (newline | eof) { raise (Ast.SyntaxError ("string not closed", lexbuf.lex_start_p)) }
and double_docstr state prefix = shortest
  | (([^ '\\'] | escape)* as s) "\"\"\""
    { lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with pos_lnum = lexbuf.lex_curr_p.pos_lnum + (count_lines s) };
      seq_string prefix s lexbuf.lex_start_p }
  | ([^ '\\'] | escape)* eof { raise (Ast.SyntaxError ("string not closed", lexbuf.lex_start_p)) }

(* and escaped_id state = parse | (([^ '\r' '\n' '$'])* as s) '$' { P.ID s } *)
