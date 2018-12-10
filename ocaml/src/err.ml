(******************************************************************************
 *
 * Seq OCaml 
 * err.ml: Error exceptions and helpers
 *
 * Author: inumanag
 *
 ******************************************************************************)

(** Error kind description *)
type t =
  | Lexer of string
  | Parser
  | Descent of string
  | Compiler of string

(** Unified exception that groups all other exceptions based on their source *)
exception CompilerError of t * Ast.Pos.t list
(** LLVM/C++ exception *)
exception SeqCError of string * Ast.Pos.t
(** AST-parsing exception  *)
exception SeqCamlError of string * Ast.Pos.t list
(** Lexing exception *)
exception SyntaxError of string * Ast.Pos.t


(** [serr ~pos format_string ...] throws an AST-parsing exception 
    with message formatted via sprintf-style [format_string]
    that indicates file position [pos] as a culprit. *)
let serr ?(pos=Ast.Pos.dummy) fmt = 
  Core.ksprintf (fun msg -> raise (SeqCamlError (msg, [pos]))) fmt

(** Helper to parse string exception messages passed from C++ library to 
    OCaml and to extract [Ast.Pos] information from them. 
    Currently done in a very primitive way by using '\b' as field separator.

    TODO: pass and parse Sexp-style strings *)
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
