(* ****************************************************************************
 * Seqaml.Ctx: Context (variable table) definitions
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core

(** Main parsing context. *)
type t =
  { filename : string (** The file that is being parsed. *)
  ; mdl : Llvm.Types.func_t (** A module LLVM handle. *)
  ; base : Llvm.Types.func_t (** Current base function LLVM handle. *)
  ; block : Llvm.Types.block_t (** Current block LLVM handle. *)
  ; trycatch : Llvm.Types.stmt_t
        (** Current try-catch LLVM handle ([Ctypes.null] if not present). *)
  ; flags : string Stack.t (** Currently active decorators (flags). *)
  ; stack : string Hash_set.t Stack.t
        (** A stack of currently active code blocks.
        Each block holds a [Hash_set] of the block-defined identifiers.
        The most recent block is located at the top of the stack. *)
  ; map : Ctx_namespace.t (** Current context namespace [Ctx_namespace.t]. *)
  ; imported : (string, Ctx_namespace.t) Hashtbl.t
        (** A hash table that maps an import name to a corresponding namespace [Ctx_namespace.t]. *)
  ; parser : t -> ?file:string -> string -> unit
        (** A callback that is used to parse a Seq code string to AST (via Menhir).
        Needed for parsing [import] statements.
        Usage: [parses ctx ?file code], where [file] is an optional file name that is used
        to populate [Ast.Ann] annotations. *)
  }

(** A context that holds the internal Seq objects and libraries. *)
let stdlib : Ctx_namespace.t = String.Table.create ()

(** [add_block context] adds a new block to the context stack. *)
let add_block ctx = Stack.push ctx.stack (String.Hash_set.create ())

(** [clear_block ctx] removes the most recent block
    and removes all corresponding variables from the namespace. *)
let clear_block ctx =
  Hash_set.iter (Stack.pop_exn ctx.stack) ~f:(fun key ->
      match Hashtbl.find ctx.map key with
      | Some [ _ ] -> Hashtbl.remove ctx.map key
      | Some (_ :: items) -> Hashtbl.set ctx.map ~key ~data:items
      | Some [] | None -> Err.ierr "cannot find variable %s (clear_block)" key)

(** [add ~ctx name var] adds a variable [name] with the handle [var] to the context [ctx]. *)
let add ~(ctx : t) ?(toplevel = false) ?(global = false) ?(internal = false) key var =
  let annot =
    Ctx_namespace.
      { base = ctx.base; global; toplevel; internal; attrs = String.Hash_set.create () }
  in
  let var = var, annot in
  (match Hashtbl.find ctx.map key with
  | None -> Hashtbl.set ctx.map ~key ~data:[ var ]
  | Some lst -> Hashtbl.set ctx.map ~key ~data:(var :: lst));
  Hash_set.add (Stack.top_exn ctx.stack) key

(** [remove ~ctx name] removes the most recent variable [name] from the namespace. *)
let remove ctx key =
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

(** [parse_file ~ctx file] parses a file [file] within the context [ctx]. *)
let parse_file ~ctx file =
  Util.dbg "parsing %s" file;
  let lines = In_channel.read_lines file in
  let code = String.concat ~sep:"\n" lines ^ "\n" in
  ctx.parser ~file:(Filename.realpath file) ctx code

(** [init ...] returns an empty context with a toplevel block that contains internal Seq types. *)
let init_module ?(argv = true) ?(jit = false) ~filename ~mdl ~base ~block parser =
  let ctx =
    { filename
    ; mdl
    ; base
    ; block
    ; parser
    ; stack = Stack.create ()
    ; flags = Stack.create ()
    ; map = String.Table.create ()
    ; imported = String.Table.create ()
    ; trycatch = Ctypes.null
    }
  in
  add_block ctx;
  (* default flags for internal arguments *)
  let toplevel = true in
  let global = true in
  let internal = true in
  if Hashtbl.length stdlib = 0
  then (
    let ctx = { ctx with map = stdlib } in
    (* initialize POD types *)
    let pod_types =
      Llvm.Type.
        [ "void", void
        ; "int", int
        ; "str", str
        ; "seq", seq
        ; "bool", bool
        ; "float", float
        ; "byte", byte
        ]
    in
    List.iter pod_types ~f:(fun (key, fn) ->
        add ~ctx ~toplevel ~global ~internal key @@ Ctx_namespace.Type (fn ()));
    (* set __argv__ params *)
    if argv
    then (
      let args = Llvm.Module.get_args mdl in
      add ~ctx ~internal ~global ~toplevel "__argv__" (Ctx_namespace.Var args));
    match Util.get_from_stdlib "stdlib" with
    | Some file -> parse_file ~ctx file
    | None -> Err.ierr "cannot locate stdlib.seq");
  Hashtbl.iteri stdlib ~f:(fun ~key ~data ->
      add ~ctx ~internal ~global ~toplevel key (fst @@ List.hd_exn data));
  ctx

(** [init_empty ctx] returns an empty context with a toplevel block that contains internal Seq types.
    Unlike [init], this function reuses all handles from [ctx]. *)
let init_empty ctx =
  let ctx = { ctx with stack = Stack.create (); map = String.Table.create () } in
  add_block ctx;
  Hashtbl.iteri stdlib ~f:(fun ~key ~data ->
      add ~ctx ~internal:true ~global:true ~toplevel:true key (fst @@ List.hd_exn data));
  ctx

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

(** [to_dbg_output ctx] outputs the current [ctx] to the debug output. *)
let to_dbg_output ctx =
  let open Util in
  let dump_map ?(depth = 1) ?(internal = false) map =
    let sortf (xa, (xb, _)) (ya, (yb, _)) = compare (xb, xa) (yb, ya) in
    let ind x = String.make (x * 3) ' ' in
    let rec prn ~depth ctx =
      let prn_assignable (ass, (ant : Ctx_namespace.tann)) =
        let ib b = if b then 1 else 0 in
        let att =
          sprintf "<%s>" @@ String.concat ~sep:", " @@ Hash_set.to_list ant.attrs
        in
        let ant =
          sprintf
            "%c%c%c"
            (if ant.toplevel then 't' else ' ')
            (if ant.global then 'g' else ' ')
            (if ant.internal then 'i' else ' ')
        in
        match ass with
        | Ctx_namespace.Var _ -> sprintf "(*var/%s*)" ant, ""
        | Func _ -> sprintf "(*fun/%s*)" ant, sprintf "%s" att
        | Type _ -> sprintf "(*typ/%s*)" ant, ""
        | Import ctx -> sprintf "(*imp/%s*)" ant, " ->\n" ^ prn ctx ~depth:(depth + 1)
      in
      let sorted =
        Hashtbl.to_alist ctx
        |> List.map ~f:(fun (a, b) -> a, List.hd_exn b)
        |> List.sort ~compare:sortf
      in
      String.concat ~sep:"\n"
      @@ List.filter_map sorted ~f:(fun (key, data) ->
             if (not internal) && (snd data).internal
             then None
             else (
               let pre, pos = prn_assignable data in
               Some (sprintf "%s%s %s %s" (ind depth) pre key pos)))
    in
    dbg "%s" (prn ~depth map)
  in
  dbg "=== == - CONTEXT DUMP - == ===";
  dbg "-> Filename: %s" ctx.filename;
  dbg "-> Keys:";
  dump_map ctx.map;
  dbg "-> Imports:";
  Hashtbl.iteri ctx.imported ~f:(fun ~key ~data ->
      dbg "   %s:" key;
      dump_map ~depth:2 data)
