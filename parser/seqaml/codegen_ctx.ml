(* ****************************************************************************
 * Seqaml.Codegen_ctx: Context (variable table) definitions
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Util

open Ast
open Option.Monad_infix

type tenv =
  { filename : string (** The file that is being parsed. *)
  ; mdl : Llvm.Types.func_t (** A module LLVM handle. *)
  ; base : Llvm.Types.func_t (** Current base function LLVM handle. *)
  ; block : Llvm.Types.block_t (** Current block LLVM handle. *)
  ; trycatch : Llvm.Types.stmt_t
        (** Current try-catch LLVM handle ([Ctypes.null] if not present). *)
  ; flags : string Stack.t (** Currently active decorators (flags). *)
  ; annotations : Ast.Ann.t Stack.t (** The current annotation *)
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
  ; stdlib : (string, tel list) Hashtbl.t
        (** A context that holds the internal Seq objects and libraries. *)
  ; sparse : ctx:t -> ?toplevel:bool -> Stmt.t Ann.ann -> Llvm.Types.stmt_t
  }

and t = (tel, tenv, tglobal) Ctx.t

let push_ann ~(ctx : t) ann = Stack.push ctx.env.annotations ann
let pop_ann ~(ctx : t) = Stack.pop ctx.env.annotations

let ann (ctx : t) = Stack.top_exn ctx.env.annotations

let var v =
  v.Ann.typ |> Ann.var_of_typ >>| Ann.real_type

let err ?(pos : Ann.t option) ?(ctx : t option) fmt =
  let cpos = Option.value_map ctx ~f:ann ~default:(Ast.Ann.create ()) in
  let pos = Option.value pos ~default:(Ann.default) in
  let pos = if pos = Ann.default then cpos else pos in
  Core.ksprintf (fun msg -> raise @@ Err.SeqCamlError (msg, [ pos ])) fmt

let is_accessible ~(ctx : t) { base; global; _} =
  ctx.Ctx.env.base = base || global

(** [add ~ctx name var] adds a variable [name] with the handle [var] to the context [ctx]. *)
let add ~(ctx : t) ?(toplevel = false) ?(global = false) ?(internal = false) key var =
  let annot =
    { base = ctx.env.base; global; toplevel; internal; attrs = String.Hash_set.create () }
  in
  let var = var, annot in
  Ctx.add ~ctx key var

(** [init ...] returns an empty context with a toplevel block that contains internal Seq types. *)
let init_module ?(argv = true) ?(jit = false) ~filename ~mdl ~base ~block sparse =
  let ctx =
    Ctx.init
      { imported = String.Table.create (); stdlib = String.Table.create (); sparse }
      { filename; mdl; base; block; flags = Stack.create (); trycatch = Ctypes.null; annotations = Stack.create () }
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
    match Util.get_from_stdlib "scratch" with
    | Some file ->

    Hashtbl.find_exn Typecheck_ctx.imports file |> List.ignore_map ~f:(ctx.globals.sparse ~ctx ~toplevel:true)
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


let rec get_realization ~(ctx : t) ?(pos = Ann.default) typ =
  Util.A.dy ~force:true "%% realizing %s" (Ann.var_to_string typ);
  let open Ast in
  let open Ann in
  let module TC = Typecheck_ctx in
  match typ with
  | Class (gen, _) | Func (gen, _) ->
    let real_name, _ = TC.get_full_name typ in
    let ast, str2real = Hashtbl.find_exn TC.realizations gen.cache in
    (match Hashtbl.find str2real real_name, typ with
    | Some { realized_llvm; _ }, _ when realized_llvm <> Ctypes.null  ->
      (* Case 1 - already realized *)
      realized_llvm
    | Some data, Class ({ cache = p, n; _ }, { is_type }) when n = Ann.default_pos ->
      let ptr = match p, (String.prefix p 1, int_of_string_opt (String.drop_prefix p 1)) with
        | "void", _ -> Llvm.Type.void ()
        | "int", _ -> Llvm.Type.int ()
        | "float", _ -> Llvm.Type.float ()
        | "byte", _ -> Llvm.Type.byte ()
        | "bool", _ -> Llvm.Type.bool ()
        | "str", _ -> Llvm.Type.str ()
        | "seq", _ -> Llvm.Type.seq ()
        | _, ("i", Some n) -> Llvm.Type.intN n
        | _, ("u", Some n) -> Llvm.Type.uintN n
        | _, ("k", Some n) -> Llvm.Type.kmerN n
        | _ ->
          match gen.generics with
          | [ _, (_, t) ] ->
            let arg = get_realization ~ctx ~pos (real_type t) in
            (match p with
            | "array" | "generator" | "ptr" ->
              Llvm.Expr.typ @@ Llvm.Type.param ~name:p arg
            | "__array__" -> err ~ctx "cannot do __array__ yet"
            | _ -> err ~ctx ~pos "cannot instantiate internal type %s" p)
          | _ -> err ~ctx ~pos "cannot instantiate internal type %s" p
      in

      (* Llvm.Type.get_methods ptr
      |> List.iter ~f:(fun (s, t) ->
        add_internal_method n s (get_name t)); *)

      Hashtbl.set str2real ~key:real_name ~data:{ data with realized_llvm = ptr };
      ptr
    | Some ({ realized_typ; realized_ast = Some (pos, (Class cls | Type cls)); _ } as data), Class (_, { is_type }) ->
      let name = sprintf "%s:%s" cls.class_name real_name in
      let ptr = (if is_type then Llvm.Type.record [] [] else Llvm.Type.cls) name in
      let new_ctx = { ctx with map = Hashtbl.copy ctx.map; stack = Stack.create () } in
      Ctx.add_block ~ctx:new_ctx;
      gen.args
      |> List.map ~f:(function (name, typ) ->
          name, get_realization ~ctx ~pos (real_type typ))
      |> List.unzip
      |> (fun (names, types) ->
          match types, is_type with
          | [], true when is_type -> err ~ctx "type definitions must have at least one member"
          | t, true -> Llvm.Type.set_record_names ptr names types
          | t, false -> Llvm.Type.set_cls_args ptr names types);
      if not is_type then Llvm.Type.set_cls_done ptr;
      Hashtbl.set str2real ~key:real_name ~data:{ data with realized_llvm = ptr };
      ptr
    | Some ({ realized_typ; realized_ast = Some (pos, Function f); _ } as data), Func (gen, { ret; _ }) ->
      let name = sprintf "%s:%s" f.fn_name.name real_name in
      let ptr = Llvm.Func.func name in
      let block = Llvm.Block.func ptr in
      (* --> NEED THIS? *)
      (* if not toplevel then Llvm.Func.set_enclosing ptr ctx.env.base; *)
      let new_ctx =
        { ctx with
          stack = Stack.create ()
        ; map = Hashtbl.copy ctx.map
        ; env = { ctx.env with base = ptr; block }
        }
      in
      Ctx.add_block ~ctx:new_ctx;
      let names, types = List.unzip @@ List.map gen.args ~f:(fun (n, t) -> n, get_realization ~pos ~ctx t) in
      Llvm.Func.set_args ptr names types;
      Llvm.Func.set_type ptr (get_realization ~ctx ~pos ret);
      List.iter names ~f:(fun n -> add ~ctx:new_ctx n (Var (Llvm.Func.get_arg ptr n)));
      List.ignore_map f.fn_stmts ~f:(ctx.globals.sparse ~ctx:new_ctx ~toplevel:false);
      Ctx.clear_block ~ctx:new_ctx;
      Hashtbl.set str2real ~key:real_name ~data:{ data with realized_llvm = ptr };
      ptr
    | Some ({ realized_typ; realized_ast = Some (pos, Extern f); _ } as data), Func (gen, { ret; _ }) ->
      let lang, dylib, ctx_name, Stmt.{ name; _ }, _ = f in
      if lang <> "c"
      then err ~ctx "only cdef externs are currently supported";
      let name = sprintf "%s:%s" name real_name in
      let ptr = Llvm.Func.func name in
      let names, types = List.unzip @@ List.map gen.args ~f:(fun (n, t) -> n, get_realization ~pos ~ctx t) in
      Llvm.Func.set_args ptr names types;
      Llvm.Func.set_type ptr (get_realization ~pos ~ctx ret);
      Llvm.Func.set_extern ptr;
      Hashtbl.set str2real ~key:real_name ~data:{ data with realized_llvm = ptr };
      ptr
    | p, _ ->
      err ~pos ~ctx "%s:%s.%s => %s not realized by a type-checker [C/T]"
      (fst gen.cache) (Ann.pos_to_string (snd gen.cache))
      (Ann.var_to_string ~full:true typ)
      real_name
      (* (Option.value_map p ~default:"_" ~f:(fun x -> Stmt.to_string @@ Option.value_exn x.realized_ast)) *)
    )
  | _ -> err ~pos ~ctx "%s not realized by a type-checker" (Ann.var_to_string ~full:true typ)
