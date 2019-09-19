(* *****************************************************************************
 * Seqaml.Ast_stmt: Statement AST node definitions
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Ast_ann

type texpr = Ast_expr.t

(** Defines statement AST variant for the current Seq syntax.
    The main type [t] is a tuple that holds:
    + node annotation (defined via [tann]), and
    + AST node itself (defined as [tn]).
    The generalization of this tuple is specified in [tt].
*)
type t =
  | Pass of unit
  | Break of unit
  | Continue of unit
  | Expr of texpr ann
  | Assign of (texpr ann * texpr ann * tassign * texpr ann option)
      (** Assignment statement. [lhs : typ assign_op rhs] parses to [(lhs, rsh, assign_op, typ)].
      [assign_op] is of type [tassign]. Type specifier is optional. *)
  | Del of texpr ann
  | Print of (texpr ann list * string)
      (** [Print] statement consists of expression list and the final print character (defaults to newline). *)
  | Return of texpr ann option
  | Yield of texpr ann option
  | Assert of texpr ann
  | TypeAlias of (string * texpr ann)
      (** [TypeAlias] handles Seq type aliases (e.g. [type ty = some_type]) *)
  | While of (texpr ann * t ann list)
  | For of (string list * texpr ann * t ann list)
  | If of if_case list
  | Match of (texpr ann * match_case list)
  | Extend of (texpr ann * t ann list)
  | Extern of (string * string option * string * param * param list)
      (** [Extern] consists of:
      - external language name (right now, only ["c"] is supported via [cdef]),
      - external library name (not supported; defaults to [None]),
      - desired function name (either same as the external name, or modified through [as] operator in Seq),
      - return function type (includes the external function name) , and
      - list of function arguments. *)
  | Import of import list
  | ImportPaste of string
      (** [ImportPaste] is currently Seq's [import!]. _Deprecated_. *)
  | Function of fn_t
  | Class of class_t
  | Type of class_t
  | Declare of param
  | Try of (t ann list * catch list * t ann list option)
      (** [Try(stmts, catch, finally_stmts)] corresponds to
      [try: stmts ... ; (catch1: ... ; catch2: ... ;) finally: finally_stmts ] *)
  | Global of string
  | Throw of texpr ann
  | Prefetch of texpr ann list
  | Special of (string * t ann list * string list)
      (** [Special] encodes custom syntax extensions used for developing new ideas. Currently not supported. *)

and if_case =
  { cond : texpr ann option
  ; cond_stmts : t ann list
  }

and match_case =
  { pattern : pattern
  ; case_stmts : t ann list
  }

and param =
  { name : string
  ; typ : texpr ann option
  }

and catch =
  { exc : string option
  ; var : string option
  ; stmts : t ann list
  }

(** [class_t] encodes classes and types in Seq.
    [args] represents for class/type arguments (object members),
    while [members] represents object methods. *)
and class_t =
  { class_name : string
  ; generics : string list
  ; args : param list option
  ; members : t ann list
  }

(** [fn_t] encodes classes and types in Seq.
    [fn_attrs] reprensets function decorators (attributes). *)
and fn_t =
  { fn_name : param
  ; fn_generics : string list
  ; fn_args : param list
  ; fn_stmts : t ann list
  ; fn_attrs : string list
  }

(** [Import] encodes Seq's import statement. [from] specifies the import source,
    [what] is the list of identifiers and their local aliases that are to be imported,
    and optional [import_as] is the local alias for the particular import.
    Examples:
    - [import from]: [{from = "from", what = [], import_as = None}]
    - [import from as import_as]: [{from = "from", what = [], import_as = "import_as"}]
    - [from from import what]: [{from = "from", what = [("what", None)], import_as = None}]
    - [from from import what1, what2 as name]:
      [{from = "from", what = [("what", None), ("what2", "name")], import_as = None}] *)
and import =
  { from : string
  ; what : (string * string option) list option
  ; import_as : string option
  }

and pattern =
  | StarPattern
  | IntPattern of int64
  | BoolPattern of bool
  | StrPattern of string
  | SeqPattern of string
  | RangePattern of (int64 * int64)
  | TuplePattern of pattern list
  | ListPattern of pattern list
  | OrPattern of pattern list
  | WildcardPattern of string option
  | GuardedPattern of (pattern * texpr ann)
  | BoundPattern of (string * pattern)

(** Specifies the kind of assign statement:
    - [Normal] is the default [=] assignment.
    - [Shadow] is the [:=] assignment operator.
    - TODO *)
and tassign =
  | Normal
  | Shadow
  | Update
