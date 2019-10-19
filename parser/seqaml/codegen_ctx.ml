(* ****************************************************************************
 * Seqaml.Codegen_ctx: Context (variable table) definitions
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core

type tenv =
  { filename : string (** The file that is being parsed. *)
  ; mdl : Llvm.Types.func_t (** A module LLVM handle. *)
  ; base : Llvm.Types.func_t (** Current base function LLVM handle. *)
  ; block : Llvm.Types.block_t (** Current block LLVM handle. *)
  ; trycatch : Llvm.Types.stmt_t
        (** Current try-catch LLVM handle ([Ctypes.null] if not present). *)
  ; flags : string Stack.t (** Currently active decorators (flags). *)
  }

(** This module defines a data structure that maps
    identifiers to the corresponding Seq LLVM handles. *)

(** A namespace object can be either:
    - a variable (LLVM handle)
    - a type / class (LLVM handle)
    - a function (LLVM handle and the list of attributes), or
    - an import context (that maps to another namespace [t]) *)
type tkind =
  | Var of Llvm.Types.var_t
  | Type of Llvm.Types.typ_t
  | Func of (Llvm.Types.func_t * string list)
  | Import of string

(** Each namespace object is linked to:
    - a [base] function (LLVM handle),
    - [toplevel] flag,
    - [global] flag,
    - [internal] flag (for stock Seq objects), and
    - the list of attributes. *)
type tann =
  { base : Llvm.Types.func_t
  ; toplevel : bool
  ; global : bool
  ; internal : bool
  ; attrs : string Hash_set.t
  }

type tel = tkind * tann

type tglobal =
  { imported : (string, t) Hashtbl.t
  ; parser : ctx:t -> ?file:string -> string -> unit
        (** A callback that is used to parse a Seq code string to AST (via Menhir).
    Needed for parsing [import] statements.
    Usage: [parses ctx ?file code], where [file] is an optional file name that is used
    to populate [Ast.Ann] annotations. *)
  ; stdlib : (string, tel list) Hashtbl.t
        (** A context that holds the internal Seq objects and libraries. *)
  }

and t = (tel, tenv, tglobal) Ctx.t

(** [add ~ctx name var] adds a variable [name] with the handle [var] to the context [ctx]. *)
let add ~(ctx : t) ?(toplevel = false) ?(global = false) ?(internal = false) key var =
  let annot =
    { base = ctx.env.base; global; toplevel; internal; attrs = String.Hash_set.create () }
  in
  let var = var, annot in
  Ctx.add ~ctx key var

(** [parse_file ~ctx file] parses a file [file] within the context [ctx]. *)
let parse_file ~(ctx : t) file =
  Util.dbg "parsing %s" file;
  let lines = In_channel.read_lines file in
  let code = String.concat ~sep:"\n" lines ^ "\n" in
  ctx.globals.parser ~ctx ~file:(Filename.realpath file) code

(** [init ...] returns an empty context with a toplevel block that contains internal Seq types. *)
let init_module ?(argv = true) ?(jit = false) ~filename ~mdl ~base ~block parser =
  let ctx =
    Ctx.init
      { imported = String.Table.create (); stdlib = String.Table.create (); parser }
      { filename; mdl; base; block; flags = Stack.create (); trycatch = Ctypes.null }
  in
  Ctx.add_block ~ctx;
  (* default flags for internal arguments *)
  let toplevel = true in
  let global = true in
  let internal = true in
  if Hashtbl.length ctx.globals.stdlib = 0
  then (
    let ctx = { ctx with map = ctx.globals.stdlib } in
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
        add ~ctx ~toplevel ~global ~internal key @@ Type (fn ()));
    (* set __argv__ params *)
    if argv
    then (
      let args = Llvm.Module.get_args mdl in
      add ~ctx ~internal ~global ~toplevel "__argv__" (Var args));
    match Util.get_from_stdlib "stdlib" with
    | Some file -> parse_file ~ctx file
    | None -> Err.ierr "cannot locate stdlib.seq");
  Hashtbl.iteri ctx.globals.stdlib ~f:(fun ~key ~data ->
      add ~ctx ~internal ~global ~toplevel key (fst @@ List.hd_exn data));
  ctx

(** [init_empty ctx] returns an empty context with a toplevel block that contains internal Seq types.
    Unlike [init], this function reuses all handles from [ctx]. *)
let init_empty ~(ctx : t) =
  let ctx = Ctx.init ctx.globals ctx.env in
  Ctx.add_block ~ctx;
  Hashtbl.iteri ctx.globals.stdlib ~f:(fun ~key ~data ->
      add ~ctx ~internal:true ~global:true ~toplevel:true key (fst @@ List.hd_exn data));
  ctx

(** [to_dbg_output ctx] outputs the current [ctx] to the debug output. *)
let to_dbg_output ~(ctx : t) =
  let open Util in
  let dump_map ?(depth = 1) ?(internal = false) map =
    let sortf (xa, (xb, _)) (ya, (yb, _)) = compare (xb, xa) (yb, ya) in
    let ind x = String.make (x * 3) ' ' in
    let rec prn ~depth (ctx : t) =
      let prn_assignable (ass, (ant : tann)) =
        let ib b = if b then 1 else 0 in
        let att = sprintf "<%s>" @@ String.concat ~sep:", " @@ Hash_set.to_list ant.attrs in
        let ant =
          sprintf
            "%c%c%c"
            (if ant.toplevel then 't' else ' ')
            (if ant.global then 'g' else ' ')
            (if ant.internal then 'i' else ' ')
        in
        match ass with
        | Var _ -> sprintf "(*var/%s*)" ant, ""
        | Func _ -> sprintf "(*fun/%s*)" ant, sprintf "%s" att
        | Type _ -> sprintf "(*typ/%s*)" ant, ""
        | Import s ->
          let ctx = Hashtbl.find_exn ctx.globals.imported s in
          sprintf "(*imp/%s*)" ant, " ->\n" ^ prn ctx ~depth:(depth + 1)
      in
      let sorted =
        Hashtbl.to_alist ctx.map
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
  dbg "-> Filename: %s" ctx.env.filename;
  dbg "-> Keys:";
  dump_map ctx;
  dbg "-> Imports:";
  Hashtbl.iteri ctx.globals.imported ~f:(fun ~key ~data ->
      dbg "   %s:" key;
      dump_map ~depth:2 data)
