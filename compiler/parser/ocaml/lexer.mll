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
  module P = Seqgrammar
  module B = Buffer

  type offset = { mutable line: int ; mutable col: int }
  type stack = (* Used for tracking indentation levels *)
    { stack: int Stack.t
    ; fname: string
    ; mutable offset: int
    ; mutable ignore_newline: int
    }
  type pystate =
    { start: int
    ; mutable p_offset: int
    ; mutable trail: int
    }

  let is_extern = ref 0

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
                if i < (l - 3) then
                try Some (int_of_string ("0x" ^ (String.sub s (i + 2) 2)))
                with Failure _ -> None
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
    match pfx with
    | "r" | "R" -> P.STRING ("", fix_literals ~is_raw:true u)
    | _ -> P.STRING (String.lowercase_ascii pfx, fix_literals u)
}

(* Lexer regex expressions *)
let newline = '\n' | "\r\n"
let white = [' ' '\t']
let comment = '#' [^ '\n' '\r']*

let digit = ['0'-'9']
let bindigit = ['0' '1']
let hexdigit = ['0'-'9' 'a'-'f' 'A'-'F']
let int = digit | digit (digit | '_')* digit
let binint = '0' ['b' 'B'] (bindigit | bindigit (bindigit | '_')* bindigit)
let hexint = '0' ['x' 'X'] (hexdigit | hexdigit (hexdigit | '_')* hexdigit)
let fraction = '.' (digit | digit (digit | '_')* digit)
let danglingfloat = int '.'
let pointfloat = int? fraction
let exponent = ['e' 'E'] ['+' '-']? (digit | digit (digit | '_')* digit)
let expfloat = (int | pointfloat | danglingfloat) exponent
let float = pointfloat | expfloat

let escape = '\\' _
let alpha = ['a'-'z' 'A'-'Z' '_']
let alphanum = ['A'-'Z' 'a'-'z' '0'-'9' '_']
(* let stringprefix = ('s' | 'S')? ('r' | 'R')? ('k' | 'K')? ('p' | 'P')? ('f' | 'F')? *)
(* let intsuffix = ('s' | 'S' | 'z' | 'Z' | 'u' | 'U') *)
let intsuffix = (alpha alphanum*)?
let stringprefix = (alpha alphanum*)?
let ident = alpha alphanum*

(* Main handler *)
rule token state = parse
  | ""
    { (* TODO: Python-style indentation detection (i.e. more strict rules) *)
      let cur = state.offset in
      let last = Stack.top state.stack in
      if cur < last then (ignore (Stack.pop state.stack); [P.DEDENT])
      else if cur > last then (Stack.push cur state.stack; [P.INDENT])
      else read state lexbuf }

(* Token rules *)
and read state = parse
  | ((white* comment? newline)* white* comment?) newline
    { let lines = count_lines (L.lexeme lexbuf) in
      lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with
        pos_lnum = lexbuf.lex_curr_p.pos_lnum + lines
      ; pos_bol = lexbuf.lex_curr_p.pos_cnum
      };
      if state.ignore_newline <= 0 then (
        let pydef_start = if !is_extern > 10 then state.offset else 0 in
        state.offset <- 0;
        if !is_extern > 10 then (
          let buf = B.create 100 in
          let pstate = { start = pydef_start; p_offset = 0; trail = 0 } in
          pydef_offset buf pstate lexbuf;
          is_extern := 0;
          lexbuf.lex_curr_pos <- lexbuf.lex_curr_pos - state.offset - 1;
          let code = Buffer.contents buf in
          let code_lines = Seq.fold_left (fun c s -> match s with '\n' -> c + 1 | _ -> c) 0 (String.to_seq code) in
          lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with pos_lnum = lexbuf.lex_curr_p.pos_lnum + code_lines + 1 };
          (* Printf.eprintf "[pd] %d %d %d \n%!" pstate.trail (Stack.top state.stack) state.offset ; *)
          match pstate.trail with
          | 0 ->
            state.offset <- 0;
            (P.EXTERN code) :: P.NL :: token state lexbuf
          | _ ->
            state.offset <- pstate.trail;
            (P.EXTERN code) :: P.NL :: token state lexbuf
        ) else (
          is_extern := 0;
          offset state lexbuf;
          [P.NL]
        )
      )
      else read state lexbuf }
  | '\\' newline white*
    { lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with
        pos_lnum = lexbuf.lex_curr_p.pos_lnum + 1
      ; pos_bol = lexbuf.lex_curr_p.pos_cnum
      };
      read state lexbuf }
  | white+ { read state lexbuf }

  | "is" white+ "not" white+ { [P.ISNOT "is not"] }
  | "not" white+ "in" white+ { [P.NOTIN "not in"] }
  | "@" (("python" | "llvm") as l) white* newline { is_extern := 1; [P.AT("@"); P.ID(l); P.NL] }
  | "print(" { [P.PRINTLP] }
  | ident as id
    { match id with
      | "True"     -> [P.TRUE]
      | "False"    -> [P.FALSE]
      | "if"       -> [P.IF]
      | "elif"     -> [P.ELIF]
      | "else"     -> [P.ELSE]
      | "def"      -> (if !is_extern > 0 then is_extern := !is_extern + 10); [P.DEF]
      | "for"      -> [P.FOR]
      | "break"    -> [P.BREAK]
      | "continue" -> [P.CONTINUE]
      | "print"    -> [P.PRINT]
      | "return"   -> [P.RETURN]
      | "yield"    -> [P.YIELD]
      | "match"    -> [P.MATCH]
      | "case"     -> [P.CASE]
      | "as"       -> [P.AS]
      | "pass"     -> [P.PASS]
      | "while"    -> [P.WHILE]
      | "lambda"   -> [P.LAMBDA]
      | "assert"   -> [P.ASSERT]
      | "global"   -> [P.GLOBAL]
      | "import"   -> [P.IMPORT]
      | "from"     -> [P.FROM]
      | "class"    -> [P.CLASS]
      | "typeof"   -> [P.TYPEOF]
      | "del"      -> [P.DEL]
      | "None"     -> [P.NONE]
      | "try"      -> [P.TRY]
      | "except"   -> [P.EXCEPT]
      | "finally"  -> [P.FINALLY]
      | "with"     -> [P.WITH]
      | "raise"    -> [P.THROW]
      | "is"       -> [P.IS "is"]
      | "in"       -> [P.IN "in"]
      | "or"       -> [P.OR "||"]
      | "and"      -> [P.AND "&&"]
      | "not"      -> [P.NOT "!"]
      | _          -> [P.ID id]
    }

  | (stringprefix as p) '\''     { [single_string state p lexbuf] }
  | (stringprefix as p) '"'      { [double_string state p lexbuf] }
  | (stringprefix as p) "'''"    { [single_docstr state p lexbuf] }
  | (stringprefix as p) "\"\"\"" { [double_docstr state p lexbuf] }

  (* | "$" { escaped_id state lexbuf } *)
  | "(" { ignore_nl state; [P.LP] }
  | ")" { aware_nl state; [P.RP] }
  | "[" { ignore_nl state; [P.LS] }
  | "]" { aware_nl state; [P.RS] }
  | "{" { ignore_nl state; [P.LB] }
  | "}" { aware_nl state; [P.RB] }

  | "|="  as op { [P.OREQ op] }
  | "&="  as op { [P.ANDEQ op] }
  | "^="  as op { [P.XOREQ op] }
  | "<<=" as op { [P.LSHEQ op] }
  | ">>=" as op { [P.RSHEQ op] }
  | "<<"  as op { [P.B_LSH op] }
  | ">>"  as op { [P.B_RSH op] }
  | "&"   as op { [P.B_AND (char_to_string op)] }
  | "^"   as op { [P.B_XOR (char_to_string op)] }
  | "~"   as op { [P.B_NOT (char_to_string op)] }
  | "||>" as op { [P.PPIPE op] }
  | ">|"  as op { [P.SPIPE op] }
  | "|>"  as op { [P.PIPE  op] }
  | "|"   as op { [P.B_OR (char_to_string op)] }
  | "="   as op { [P.EQ (char_to_string op)] }
  | "..." as op { [P.ELLIPSIS op] }
  | "@"   as op { [P.AT (char_to_string op)] }
  | ":="  as op { [P.WALRUS op] }
  (* | "=>"  { [P.ARROW] } *)
  | "->"  { [P.OF] }
  | ":"   { [P.COLON] }
  | ";"   { [P.SEMICOLON] }
  | ","   { [P.COMMA] }
  | "."   { [P.DOT] }
  | "+="  as op { [P.PLUSEQ op] }
  | "-="  as op { [P.MINEQ op] }
  | "**=" as op { [P.POWEQ op] }
  | "*="  as op { [P.MULEQ op] }
  | "//=" as op { [P.FDIVEQ op] }
  | "/="  as op { [P.DIVEQ op] }
  | "%="  as op { [P.MODEQ op] }
  | "+"   as op { [P.ADD (char_to_string op)] }
  | "-"   as op { [P.SUB (char_to_string op)] }
  | "**"  as op { [P.POW op] }
  | "*"   as op { [P.MUL (char_to_string op)] }
  | "=="  as op { [P.EEQ op] }
  | "!="  as op { [P.NEQ op] }
  | ">="  as op { [P.GEQ op] }
  | ">"   as op { [P.GREAT (char_to_string op)] }
  | "<="  as op { [P.LEQ op] }
  | "<"   as op { [P.LESS (char_to_string op)] }
  | "//"  as op { [P.FDIV op] }
  | "/"   as op { [P.DIV (char_to_string op)] }
  | "%"   as op { [P.MOD (char_to_string op)] }

  | (int | binint | hexint) as i { [P.INT (i, "")] }
  | ((int | binint | hexint) as i) (intsuffix as k) {
    (* Handle exponents (1e2) *)
    match k.[0], float_of_string_opt (i ^ k) with
    | 'e', Some fp -> [P.FLOAT (fp, "")]
    | _ -> [P.INT (i, String.lowercase_ascii k)]
  }
  | danglingfloat as f { [P.FLOAT (float_of_string f, "")] }
  | float as f { [P.FLOAT (float_of_string f, "")] }
  | (float as f) (intsuffix as k) {
    (* Handle exponents (1e2) *)
    match float_of_string_opt (f ^ k) with
    | Some fp -> [P.FLOAT (fp, "")]
    | None -> [P.FLOAT (float_of_string f, String.lowercase_ascii k)]
  }
  | eof { [P.EOF] }
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

(* PyDef lexing *)
(* TODO: handle newlines in pydef block... *)
and pydef buf state = parse
  | eof {}
  | white* newline
    { B.add_string buf (L.lexeme lexbuf);
      state.p_offset <- 0;
      pydef_offset buf state lexbuf }
  | _
    { B.add_string buf (L.lexeme lexbuf);
      pydef buf state lexbuf }

and pydef_offset buf state = parse
  | (' ' | '\t') as t
    { if state.p_offset >= state.start then B.add_string buf (char_to_string t);
      state.p_offset <- state.p_offset + (if t = ' ' then 1 else 8);
      pydef_offset buf state lexbuf }
  | _
    { if state.p_offset <= state.start
      then state.trail <- state.p_offset
      else (B.add_string buf (L.lexeme lexbuf); pydef buf state lexbuf) }
