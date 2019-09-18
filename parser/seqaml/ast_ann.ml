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

(** A default [tpos] placeholder for internal use. *)
let default_pos : tpos = { file = ""; line = -1; col = -1; len = 0 }

(** Each identifier is uniquely identified by its name and its location *)
type tlookup =
  (string * tpos)

(** Type annotation for an AST node. *)
type ttyp =
  { kind: ttypkind
  ; positions: tpos array
  }

and ttypkind =
  | Unknown
  | Import of string
  | Tuple of ttyp list
  | Func of tfunc
  | Class of tcls
  | TypeVar of ttypvar ref

and tfunc =
  { f_generics : (string * (int * ttyp)) list
  ; f_name : string
  ; f_cache : tlookup
  ; f_parent : (tlookup * (ttyp option))
  ; f_args : (string * ttyp) list
  ; f_ret : ttyp
  }

and tcls =
  { c_generics : (string * (int * ttyp)) list
  ; c_name : string
  ; c_cache : tlookup
  ; c_parent : (tlookup * (ttyp option))
  ; c_args : (string * ttyp) list
  ; c_type : (ttyp list) option
  }

and ttypvar =
  | Unbound of (int * int * bool) (*id, level, is_generic*)
  | Bound of ttyp
  | Generic of int

let default_typ : ttyp = { kind = Unknown; positions = [||] }

type t =
  { pos: tpos
  ; typ: ttyp
  }

let default : t = { pos = default_pos; typ = default_typ }

type 'a ann = t * 'a
