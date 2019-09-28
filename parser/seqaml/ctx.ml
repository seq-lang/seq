(* ****************************************************************************
 * Seqaml.Ctx: Context (variable table) definitions
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core

(** Alias for a [Hashtbl] that maps identifiers ([string]) to a list of namespace object [tel].
    The head of such list is the most recent (and the currently active) Seq object. *)
type ('tel, 'tenv, 'tglobal) t =
  { stack : string Hash_set.t Stack.t
        (** A stack of currently active code blocks.
        Each block holds a [Hash_set] of the block-defined identifiers.
        The most recent block is located at the top of the stack. *)
  ; map : (string, 'tel list) Hashtbl.t
        (** A hash table that maps an import name to a corresponding namespace [Codegen_ctx.t]. *)
  ; env : 'tenv
  ; globals : 'tglobal
  }

let init globals env =
  { stack = Stack.create (); map = String.Table.create (); globals; env }

(** [add_block context] adds a new block to the context stack. *)
let add_block ~ctx = Stack.push ctx.stack (String.Hash_set.create ())

(** [clear_block ctx] removes the most recent block
    and removes all corresponding variables from the namespace. *)
let clear_block ~ctx =
  Hash_set.iter (Stack.pop_exn ctx.stack) ~f:(fun key ->
      match Hashtbl.find ctx.map key with
      | Some [ _ ] -> Hashtbl.remove ctx.map key
      | Some (_ :: items) -> Hashtbl.set ctx.map ~key ~data:items
      | Some [] | None -> Err.ierr "cannot find variable %s (clear_block)" key)

(** [add ~ctx name var] adds a variable [name] with the handle [var] to the context [ctx]. *)
let add ~ctx key var =
  Hashtbl.find_and_call
    ctx.map
    key
    ~if_found:(fun lst -> Hashtbl.set ctx.map ~key ~data:(var :: lst))
    ~if_not_found:(fun key -> Hashtbl.set ctx.map ~key ~data:[ var ]);
  Hash_set.add (Stack.top_exn ctx.stack) key

(** [remove ~ctx name] removes the most recent variable [name] from the namespace. *)
let remove ~ctx key =
  match Hashtbl.find ctx.map key with
  | Some (hd :: tl) ->
    (match tl with
    | [] -> Hashtbl.remove ctx.map key
    | tl -> Hashtbl.set ctx.map ~key ~data:tl);
    ignore
    @@ Stack.find ctx.stack ~f:(fun set ->
           match Hash_set.find set ~f:(( = ) key) with
           | Some _ ->
             Hash_set.remove set key;
             true
           | None -> false)
  | _ -> ()

(** [in_block ~ctx name] returns the most recent variable handle
    if a variable [name] is present in the most recent block [ctx] *)
let in_block ~ctx key =
  if Stack.length ctx.stack = 0
  then None
  else if Hash_set.exists (Stack.top_exn ctx.stack) ~f:(( = ) key)
  then Some (List.hd_exn @@ Hashtbl.find_exn ctx.map key)
  else None

(** [in_scope ~ctx name] returns the most recent variable handle
    if a variable [name] is present in the context [ctx]. *)
let in_scope ~ctx key =
  match Hashtbl.find ctx.map key with
  | Some (hd :: _) -> Some hd
  | _ -> None
