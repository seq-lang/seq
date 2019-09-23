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
  | Int of string
      (** [Int]s are by default stored as strings because the conversion is done during the parsing. *)
  | IntS of (string * string)
      (** Second member denotes the suffix (e.g. [10u] -> [IntS("10", "u")]). *)
  | Float of float
  | FloatS of (float * string)
  | String of string
  | Kmer of string
  | Seq of string
  | Id of string
  | Unpack of string (** [Unpack(x)] corresponds to [**x] in Seq. *)
  | Tuple of t ann list
  | List of t ann list
  | Set of t ann list
  | Dict of (t ann * t ann) list
  | Generator of (t ann * tcomprehension)
  | ListGenerator of (t ann * tcomprehension)
  | SetGenerator of (t ann * tcomprehension)
  | DictGenerator of ((t ann * t ann) * tcomprehension)
  | IfExpr of (t ann * t ann * t ann)
  | Unary of (string * t ann)
  | Binary of (t ann * string * t ann)
  | Pipe of (string * t ann) list
      (** Pipelines are represented as a list of pipe members and their preceding pipeline operators
      (e.g. [a |> b ||> c] is [("", a), ("|>", b), ("||>", c)]). *)
  | Index of (t ann * t ann list)
  | Call of (t ann * tcall list)
      (** Each call arguments is of type [tcall] to account for argument names. *)
  | Slice of (t ann option * t ann option * t ann option)
  | Dot of (t ann * string)
  | Ellipsis of unit
      (** [Ellipsis] is currently used only for partial function definitions. *)
  | TypeOf of t ann
  | Ptr of t ann (** [Ptr(x)] corresponds to [__ptr__[x]]. *)
  | Lambda of (string list * t ann)
      (** {b NOTE} : Lambdas are currently not supported. *)

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
  ; next : tcomprehension option
  }
