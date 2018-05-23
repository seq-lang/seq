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

rule read = parse
  | white 
  | newline
  | comment        { read lexbuf }
  | "("            { LPAREN }
  | ")"            { RPAREN }
  | "="            { EQ }
  | "+" as op      { ADD (Char.to_string op) }
  | "-" as op      { SUB (Char.to_string op) }
  | "*" as op      { MUL (Char.to_string op) }
  | "|>" as op     { PIPE (op) }
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
    raise (SyntaxError (Format.sprintf "unknown token: '%s' at (%s)" tok pos_fmt))
  }
