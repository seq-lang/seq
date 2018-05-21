(* 786 *)

open Core

type expr =
  | Int of int
  | Float of float
  | Identifier of string * expr list
  | Binary of expr * string * expr
and proto =
  | Prototype of string * string list

type ast =
  | Definition of proto * expr list
  | Expr of expr
  | Eof

let rec prn_expr = function
  | Int i -> sprintf "%d" i
  | Float f -> sprintf "%.1f" f
  | Identifier(v, a) -> 
    sprintf "%s%s" v (if List.length a > 0
      then "(" ^ (String.concat ~sep:"," (List.map a ~f:prn_expr)) ^ ")" 
      else "")
  | Binary(e1, op, e2) -> 
    sprintf "{%s%s%s}" (prn_expr e1) op (prn_expr e2)
and prn_proto = function
  | Prototype(p, sa) -> 
    sprintf "def::%s(%s)" p 
      (String.concat ~sep:" " (List.map sa ~f:(sprintf "%s")))
and prn_ast = function
  | Definition(p, e) -> 
    sprintf "%s->{ %s }" (prn_proto p) 
      (String.concat ~sep:";" (List.map e ~f:prn_expr))
  | Expr e -> 
    prn_expr e
  | Eof ->
    sprintf "EOF"
