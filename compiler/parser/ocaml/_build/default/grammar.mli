
(* The type of tokens. *)

type token = 
  | YIELD
  | XOREQ of (string)
  | WITH
  | WHILE
  | TYPEOF
  | TYPE
  | TRY
  | TRUE
  | THROW
  | SUB of (string)
  | STRING of (string)
  | SPIPE of (string)
  | SEQ of (string * string)
  | SEMICOLON
  | RSHEQ of (string)
  | RS
  | RP
  | RETURN
  | RB
  | PYDEF_RAW of (string)
  | PYDEF
  | PTR
  | PRINT
  | PPIPE of (string)
  | POWEQ of (string)
  | POW of (string)
  | PLUSEQ of (string)
  | PIPE of (string)
  | PASS
  | OREQ of (string)
  | OR of (string)
  | OF
  | NOTIN of (string)
  | NOT of (string)
  | NONE
  | NL
  | NEQ of (string)
  | MULEQ of (string)
  | MUL of (string)
  | MODEQ of (string)
  | MOD of (string)
  | MINEQ of (string)
  | MATCH
  | LSHEQ of (string)
  | LS
  | LP
  | LESS of (string)
  | LEQ of (string)
  | LB
  | LAMBDA
  | KMER of (string)
  | ISNOT of (string)
  | IS of (string)
  | INT_S of (string * string)
  | INDENT
  | IN of (string)
  | IMPORT
  | IF
  | ID of (string)
  | GREAT of (string)
  | GLOBAL
  | GEQ of (string)
  | FSTRING of (string)
  | FROM
  | FOR
  | FLOAT_S of (float * string)
  | FINALLY
  | FDIVEQ of (string)
  | FDIV of (string)
  | FALSE
  | EXTERN of (string)
  | EXTEND
  | EXCEPT
  | EQ of (string)
  | EOF
  | ELSE
  | ELLIPSIS of (string)
  | ELIF
  | EEQ of (string)
  | DOT
  | DIVEQ of (string)
  | DIV of (string)
  | DEL
  | DEF
  | DEDENT
  | CONTINUE
  | COMMA
  | COLON
  | CLASS
  | CASE
  | B_XOR of (string)
  | B_RSH of (string)
  | B_OR of (string)
  | B_NOT of (string)
  | B_LSH of (string)
  | B_AND of (string)
  | BREAK
  | AT of (string)
  | ASSERT
  | AS
  | ANDEQ of (string)
  | AND of (string)
  | ADD of (string)

(* This exception is raised by the monolithic API functions. *)

exception Error

(* The monolithic API. *)

val program: (Lexing.lexbuf -> token) -> Lexing.lexbuf -> (Ast.tstmt Ast.ann list)

module MenhirInterpreter : sig
  
  (* The incremental API. *)
  
  include MenhirLib.IncrementalEngine.INCREMENTAL_ENGINE
    with type token = token
  
end

(* The entry point(s) to the incremental API. *)

module Incremental : sig
  
  val program: Lexing.position -> (Ast.tstmt Ast.ann list) MenhirInterpreter.checkpoint
  
end
