(* ****************************************************************************
 * Seqaml.Ctx_namespace: Context (variable table) definitions
 *
 * Author: inumanag
 * License: see LICENSE
 * ****************************************************************************)

open Core

(** This module defines a data structure that maps
    identifiers to the corresponding Seq LLVM handles. *)

(** Alias for a [Hashtbl] that maps identifiers ([string]) to a list of namespace object [tel].
    The head of such list is the most recent (and the currently active) Seq object. *)
type t = (string, (tel * tann) list) Hashtbl.t

(** A namespace object can be either:
    - a variable (LLVM handle)
    - a type / class (LLVM handle)
    - a function (LLVM handle and the list of attributes), or
    - an import context (that maps to another namespace [t]) *)
and tel =
  | Var of Llvm.Types.var_t
  | Type of Llvm.Types.typ_t
  | Func of (Llvm.Types.func_t * string list)
  | Import of t

(** Each namespace object is linked to:
    - a [base] function (LLVM handle),
    - [toplevel] flag,
    - [global] flag,
    - [internal] flag (for stock Seq objects), and
    - the list of attributes. *)
and tann =
  { base : Llvm.Types.func_t
  ; toplevel : bool
  ; global : bool
  ; internal : bool
  ; attrs : string Hash_set.t
  }
