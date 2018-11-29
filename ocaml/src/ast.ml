(* 786 *)

open Core
open Sexplib.Std

module Pos = 
struct
  type t = 
    { file: string;
      line: int;
      col: int;
      len: int }
  [@@deriving sexp]
  
  let dummy =
    { file = ""; line = -1; col = -1; len = 0 }
end

module ExprNode = 
struct 
  (** Each node is a 2-tuple that stores 
      (1) position within a file and
      (2) node data *)
  type 'a tt = 
    Pos.t * 'a
  and t = 
    node tt
  and node =
    | Empty          of unit
    | Bool           of bool
    | Int            of int
    | Float          of float
    | String         of string
    | Seq            of string
    | Generic        of generic
    | Id             of string
    | Tuple          of t list
    | List           of t list
    | Set            of t list
    | Dict           of (t * t) list
    | Generator      of (t * comprehension tt)
    | ListGenerator  of (t * comprehension tt)
    | SetGenerator   of (t * comprehension tt)
    | DictGenerator  of ((t * t) * comprehension tt)
    | IfExpr         of (t * t * t)
    | Unary          of (string * t)
    | Binary         of (t * string * t)
    | Pipe           of t list
    | Index          of (t * t list)
    | Call           of (t * t list)
    | Slice          of (t option * t option * t option)
    | Dot            of (t * string)
    | Ellipsis       of unit
    | TypeOf         of t
    | Lambda         of (string tt list * t)
  and generic = 
    string
  and comprehension =
    { var: string list; 
      gen: t; 
      cond: t option; 
      next: (comprehension tt) option }
  [@@deriving sexp]
end 

module StmtNode = 
struct
  (** Each node is a 2-tuple that stores 
      (1) position within a file and
      (2) node data *)
  type 'a tt = 
    Pos.t * 'a
  and t =
    node tt
  and node = 
    | Pass     of unit
    | Break    of unit
    | Continue of unit
    | Expr     of ExprNode.t
    | Assign   of (ExprNode.t list * ExprNode.t list * bool)
    | Del      of ExprNode.t list
    | Print    of ExprNode.t list
    | Return   of ExprNode.t option
    | Yield    of ExprNode.t option
    | Assert   of ExprNode.t list
    | Type     of (string * param tt list)
    | While    of (ExprNode.t * t list)
    | For      of (string list * ExprNode.t * t list)
    | If       of (if_case tt) list
    | Match    of (ExprNode.t * (match_case tt) list)
    | Extend   of (string * generic tt list)
    | Extern   of (string * string option * param tt * param tt list)
    | Import   of (string tt * string option) list 
    | Generic  of generic 
    | Try      of (t list * catch tt list * t list option)
    | Global   of string tt list
    | Throw    of ExprNode.t
  and generic =
    | Function of 
        (param tt * (ExprNode.generic tt) list * param tt list * t list)
    | Class of 
        (string * (ExprNode.generic tt) list * param tt list * generic tt list)
  and if_case = 
    { cond: ExprNode.t option; 
      stmts: t list }
  and match_case = 
    { pattern: pattern;
      stmts: t list }
  and param = 
    { name: string; 
      typ: ExprNode.t option; }
  and catch = 
    { exc: string option;
      var: string option;
      stmts: t list }
  and pattern = 
    | StarPattern
    | IntPattern      of int
    | BoolPattern     of bool
    | StrPattern      of string
    | SeqPattern      of string
    | RangePattern    of (int * int)
    | TuplePattern    of pattern list
    | ListPattern     of pattern list
    | OrPattern       of pattern list
    | WildcardPattern of string option
    | GuardedPattern  of (pattern * ExprNode.t)
    | BoundPattern    of (string * pattern)
  [@@deriving sexp]
end

type t = Module of StmtNode.t list
[@@deriving sexp]
