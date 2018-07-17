(* 786 *)

open Core
open Sexplib.Std

type expr =
  | Bool of bool
  | Int of int
  | Float of float
  | String of string
  | Ident of string
  | Binary of expr * string * expr
  | Pipe of expr list  
  | Index of string * expr
  | Call of string * expr list
  | List of expr list
  | Extern of string * string
  (* | IfExpr of expr * expr * expr *)
  [@@deriving sexp]

type statement =
  | Pass
  | Expr of expr
  | Print of expr list
  | Assign of string * expr (* x = sth *)
  | For of string * expr * statement list (* var, array, for-st *)
    | Break 
    | Continue
  | IfElse of expr * statement list * statement list (* cond, if-st, else-st *)
  | Match of expr * case list
  | Function of string * string list * statement list (* name, args, fun-st *)
    | Return of expr
    | Yield of expr
  | Eof
and case =
  | Case of expr * string option * statement list
  [@@deriving sexp]

type ast = 
  | Module of statement list
  [@@deriving sexp]

let rec flatten = function 
  | Binary(e1, op, e2) when op = "|>" -> 
    begin
      let f1 = flatten e1 in
      let f2 = flatten e2 in
      match f1, f2 with 
      (* TODO optimize calls below, @ is O(n) *)
      | Pipe f1, Pipe f2 -> Pipe (f1 @ f2) 
      | Pipe f1, f2 -> Pipe (f1 @ [f2])
      | f1, Pipe f2 -> Pipe (f1 :: f2)
      | f1, f2 -> Pipe [f1; f2]
    end
  | Binary(e1, op, e2) -> Binary(flatten e1, op, flatten e2)
  | Index(s, e) -> Index(s, flatten e)
  | Call(c, l) -> Call(c, List.map l ~f:flatten)
  | List(l) -> List(List.map l ~f:flatten)
  | _ as e -> e

let prn_ast_sexp a =
  sexp_of_ast a |> Sexp.to_string 
