(* *****************************************************************************
 * Seqaml.Ast_pos: AST annotations
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

(** File position annotation for an AST node. *)
type t =
  { file : string
  ; line : int
  ; col : int
  ; len : int
  ; typ : ttyp
  ; is_type_ast : bool
  }
[@@deriving fields]

(** Each identifier is uniquely identified by its name and its location *)
and tlookup = string * t

(** Type annotation for an AST node. *)
and ttyp =
  | Unknown
  | Import of string
  | TypeVar of ttypvar ref
  (* | Type of ttypvar ref *)
  | Tuple of t list
  | Func of tfunc
  | Class of tcls

and tfunc =
  { f_generics : (string * (int * t)) list
  ; f_name : string
  ; f_cache : tlookup
  ; f_parent : tlookup * t option
  ; f_args : (string * t) list
  ; f_ret : t
  }

and tcls =
  { c_generics : (string * (int * t)) list
  ; c_name : string
  ; c_cache : tlookup
  ; c_parent : tlookup * t option
  ; c_args : (string * t) list
  ; c_type : t list option
  }

and ttypvar =
  | Unbound of (int * int * bool) (*id, level, is_generic*)
  | Bound of t
  | Generic of int

let default : t =
  { file = ""; line = -1; col = -1; len = 0; typ = Unknown; is_type_ast = false }

type 'a ann = t * 'a
