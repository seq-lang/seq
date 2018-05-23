(* 786 *)

open Core
open Sexplib.Std

type expr =
  | Int of int
  | Float of float
  | Identifier of string * expr list
  | Binary of expr * string * expr
  | Pipe of expr list
and proto =
  | Prototype of string * string list
  [@@deriving sexp]

type ast =
  | Definition of proto * expr list
  | Expr of expr
  | Eof
  [@@deriving sexp]

let rec flatten_exp = function 
  | Binary(e1, op, e2) when op = "|>" -> 
    begin
      let f1 = flatten_exp e1 in
      let f2 = flatten_exp e2 in
      match f1, f2 with 
      (* TODO optimize calls below, @ is O(n) *)
      | Pipe f1, Pipe f2 -> Pipe (f1 @ f2) 
      | Pipe f1, f2 -> Pipe (f1 @ [f2])
      | f1, Pipe f2 -> Pipe (f1 :: f2)
      | f1, f2 -> Pipe [f1; f2]
    end
  | Binary(e1, op, e2) -> Binary(flatten_exp e1, op, flatten_exp e2)
  | Identifier(i, l) -> Identifier(i, List.map l ~f:flatten_exp)
  | _ as e -> e
and flatten = function 
  | Expr e -> Expr (flatten_exp e)
  | Definition(p, l) -> Definition(p, List.map l ~f:flatten_exp)
  | Eof -> Eof

(* let prn_ast a =
  sexp_of_ast a |> Sexp.to_string *)

let rec prn_expr = function
  | Int i -> sprintf "%d" i
  | Float f -> sprintf "%.1f" f
  | Identifier(v, a) -> 
    sprintf "%s%s" v 
      (if List.length a > 0
      then sprintf "(%s)" (List.map a ~f:prn_expr |> String.concat ~sep:", ")
      else "")
  | Binary(e1, op, e2) -> 
    sprintf "(%s %s %s)" (prn_expr e1) op (prn_expr e2)
  | Pipe l ->
    let str = List.map l ~f:(fun x -> prn_expr x |> sprintf "%s") |> String.concat ~sep:" | " in
    sprintf "[%s]" str
and prn_proto = function
  | Prototype(p, sa) -> 
    sprintf "def %s(%s)" p 
      (String.concat ~sep:" " (List.map sa ~f:(sprintf "%s")))
and prn_ast = function
  | Definition(p, e) -> 
    sprintf "%s { %s }" (prn_proto p) (List.map e ~f:prn_expr |> String.concat ~sep:";")
  | Expr e -> prn_expr e
  | Eof -> sprintf "EOF"
