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
  }

(** Annotated type specification. *)
type 'a ann = t * 'a 

(** A default [t] placeholder for internal use. *)
let default : t = { file = ""; line = -1; col = -1; len = 0 }
