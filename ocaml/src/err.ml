(* 786 *)

type error =
  | Lexer of string
  | Parser
  | Descent of string
  | Compiler of string

exception SeqCError of string * Ast.Pos.t
exception SeqCamlError of string * Ast.Pos.t list
exception CompilerError of error * Ast.Pos.t list

let serr ?(pos=Ast.Pos.dummy) fmt = 
  Core.ksprintf (fun msg -> raise (SeqCamlError (msg, [pos]))) fmt

let split_error msg = 
  let open Core in
  let l = Array.of_list @@ String.split ~on:'\b' msg in
  assert ((Array.length l) = 5);
  let msg = l.(0) in
  let file = l.(1) in
  let line = Int.of_string l.(2) in
  let col = Int.of_string l.(3) in
  let len = Int.of_string l.(4) in 
  raise @@ SeqCError (msg, Ast.Pos.{ file; line; col; len })
