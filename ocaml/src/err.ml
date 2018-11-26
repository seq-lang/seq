(* 786 *)

type error =
  | Lexer of string
  | Parser
  | Descent of string
  | Compiler of string

exception SeqCError of string * Ast.Pos.t
exception SeqCamlError of string * Ast.Pos.t list
exception CompilerError of error * Ast.Pos.t list

let seq_error msg pos =
  raise (SeqCamlError (msg, pos))

let serr ?(pos=Ast.Pos.dummy) fmt = 
  Core.ksprintf (fun msg -> raise (SeqCamlError (msg, [pos]))) fmt