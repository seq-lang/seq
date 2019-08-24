(* *****************************************************************************
 * Seqaml.Ast_expr: Expression AST node definitions
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Ast_ann

(** Defines expression AST variant for the current Seq syntax.
    The main type [t] is a tuple that holds:
    + node annotation (defined via [tann]), and
    + AST node itself (defined as [tn]).
    The generalization of this tuple is specified in [tt].
*)
type t =
  | Empty of unit
  | Bool of bool
  (** [Int]s are by default stored as strings because the conversion is done during the parsing. *)
  | Int of string
  (** Second member denotes the suffix (e.g. [10u] -> [IntS("10", "u")]). *)
  | IntS of (string * string)
  | Float of float
  | FloatS of (float * string)
  | String of string
  | Seq of string
  | Id of string
  (** [Unpack(x)] corresponds to [**x] in Seq. *)
  | Unpack of string
  | Tuple of (t ann) list
  | List of (t ann) list
  | Set of (t ann) list
  | Dict of ((t ann) * (t ann)) list
  | Generator of ((t ann) * (tcomprehension ann))
  | ListGenerator of ((t ann) * (tcomprehension ann))
  | SetGenerator of ((t ann) * (tcomprehension ann))
  | DictGenerator of (((t ann) * (t ann)) * (tcomprehension ann))
  | IfExpr of ((t ann) * (t ann) * (t ann))
  | Unary of (string * (t ann))
  | Binary of ((t ann) * string * (t ann))
  (** Pipelines are represented as a list of pipe members and their preceding pipeline operators
      (e.g. [a |> b ||> c] is [("", a), ("|>", b), ("||>", c)]). *)
  | Pipe of (string * (t ann)) list
  | Index of ((t ann) * (t ann))
  (** Each call arguments is of type [call] to account for argument names. *)
  | Call of ((t ann) * (tcall ann) list)
  | Slice of ((t ann) option * (t ann) option * (t ann) option)
  | Dot of ((t ann) * string)
  (** [Ellipsis] is currently used only for partial function definitions. *)
  | Ellipsis of unit
  | TypeOf of (t ann)
  (** [Ptr(x)] corresponds to [__ptr__[x]]. *)
  | Ptr of (t ann)
  (** _NOTE_ : Lambdas are currently not supported. *)
  | Lambda of ((string ann) list * (t ann))

and tcall =
  { name : string option
  ; value : t ann
  }

(** Comprehension types store variable names ([var]), generator definition ([gen]), 
    conditions ([cond]) and the pointer to an optional next comprehension. 
    For example, [i, k for i in foo() for j in bar() if i + j == 2] is represented as
    [{var = [i, k]; 
      gen = Expr("foo()"); 
      cond = None; 
      next = {var = [j]; 
              gen = Expr("bar()"); 
              cond = Expr("i + j == 2"); 
              next = None} 
     }] *)
and tcomprehension =
  { var : string list
  ; gen : t ann
  ; cond : t ann option
  ; next : tcomprehension ann option
  }
