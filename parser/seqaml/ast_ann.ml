(* *****************************************************************************
 * Seqaml.Ast_pos: AST annotations
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

(** File position annotation for an AST node. *)
type tpos =
  { file : string
  ; line : int
  ; col : int
  ; len : int
  }
[@@deriving fields]

(** Each identifier is uniquely identified by its name and its location *)
type tlookup = string * tpos

(** Type annotation for an AST node. *)
type ttyp =
  | Import of string
  | Var of tvar
  | Type of tvar

and tvar =
  | Tuple of tvar list
  | Func of (tgeneric * tfun) (* f_ret *)
  | Class of (tgeneric * tclass) (* c_type *)
  | Link of tvar' ref

and tvar' =
  | Unbound of (int * int * bool) (*id, level, is_generic*)
  | Generic of int
  | Bound of tvar

and tgeneric =
  { generics : (string * (int * tvar)) list
  ; name : string (**probably not needed *)
  ; cache : tlookup
  ; parent : tlookup * tvar option
  ; args : (string * tvar) list
  }

and tfun =
  { ret: tvar
  ; used: string Core.Hash_set.t
  }

and tclass =
  { types: tvar list option } (** unify uses this: needed at all? *)

type t =
  { pos: tpos
  ; typ: ttyp option
  }

let default_pos : tpos =
  { file = ""
  ; line = -1
  ; col = -1
  ; len = 0
  }

let default : t =
  { pos = default_pos
  ; typ = None
  }

type 'a ann = t * 'a
