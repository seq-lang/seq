(* 786 *)

{
  open Core
  open Parser
  exception SyntaxError of string
}

let white = [' ' '\t']+
let newline = ['\n' '\r']
let digit = ['0'-'9']
let int = '-'? digit+
let frac = '.' digit*
let float =  int? frac
let alpha = ['a'-'z' 'A'-'Z']
let alphanum = ['A'-'z' 'a'-'z' '0'-'9']
let ident = alpha alphanum*
let comment = "#" _*? "\n" 
let string = "\"" _*? "\""

rule read = parse
  | white
  | newline
  | comment        { read lexbuf }
  | '"'            { read_string (Buffer.create 17) lexbuf }
  | "("            { LPAREN }
  | ")"            { RPAREN }
  | "="            { EQ }
  | "+" as op      { ADD (Char.to_string op) }
  | "-" as op      { SUB (Char.to_string op) }
  | "*" as op      { MUL (Char.to_string op) }
  | "|" as op      { PIPE (Char.to_string op) }
  | "," as op      { BRANCH (Char.to_string op) }
  | "let"          { LET }
  | int as i       { INT (int_of_string i) }   
  | float as f     { FLOAT (float_of_string f) }
  | ident as id    { ID id }
  | eof            { EOF }
  | _ {
    let tok = Lexing.lexeme lexbuf in
    let pos = Lexing.lexeme_start_p lexbuf in
    let pos_fmt = Format.sprintf "file: %s, line: %d, col: %d" pos.pos_fname pos.pos_lnum pos.pos_cnum in
    SyntaxError (Format.sprintf "unknown token: '%s' at (%s)" tok pos_fmt) |> raise
  }
and read_string buf =
  parse
  | '"'           { STRING (Buffer.contents buf) }
  | '\\' '/'      { Buffer.add_char buf '/'; read_string buf lexbuf }
  | '\\' '\\'     { Buffer.add_char buf '\\'; read_string buf lexbuf }
  | '\\' 'b'      { Buffer.add_char buf '\b'; read_string buf lexbuf }
  | '\\' 'f'      { Buffer.add_char buf '\012'; read_string buf lexbuf }
  | '\\' 'n'      { Buffer.add_char buf '\n'; read_string buf lexbuf }
  | '\\' 'r'      { Buffer.add_char buf '\r'; read_string buf lexbuf }
  | '\\' 't'      { Buffer.add_char buf '\t'; read_string buf lexbuf }
  | [^ '"' '\\']+ { Buffer.add_string buf @@ Lexing.lexeme lexbuf; read_string buf lexbuf }
  | _             { SyntaxError ("Illegal string character: " ^ Lexing.lexeme lexbuf) |> raise }
  | eof           { SyntaxError "String is not terminated" |> raise }

