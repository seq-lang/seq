(* ****************************************************************************
 * Seqaml.Typecheck_ctx: Context (variable table) definitions
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Util
open Err
open Ast
open Option.Monad_infix

(** Type checking environment  *)
type tenv =
  { level : int (** Type checking level *)
  ; unbounds : Ast.Ann.t Stack.t
        (** List of all unbound variables within the environment *)
  ; enclosing_name : Ast.Ann.tlookup
        (** The name of the enclosing function or class.
      [__main__] is used for the outermost block within a module. *)
  ; enclosing_type : Ast.Ann.tlookup option
        (** The type of the enclosing class. [None] if there is no such class.
      Used for dynamically detecting class members. *)
  ; enclosing_return : Ast.Ann.tvar option ref
        (** The return type of the current function. [None] if not within a function.
      Mutable as it can change during the parsing. *)
  ; realizing : bool
        (** Are we realizing the current block? By default, [true] unless scanning the
      class members (those are realized during the explicit realization stage). *)
  ; being_realized : (Ast.Ann.tlookup, Ast.Ann.t list) Hashtbl.t
        (** A hashtable containing the names of functions/classes that are currently being realized.
      Used to avoid infinite recursion during the realization of recursive or
      self-referencing types or functions. *)
  ; annotations : Ann.t Stack.t (** The current annotation *)
  ; statements : Stmt.t Ann.ann Stack.t
  ; filename : string
  }

(* type tel =
  | Member of Ast.Ann.t
  | Var of Ast.Ann.t
  | Type of Ast.Ann.t
  | Import of string *)

(* | Import of Ast.Ann.t  --> type Import etc *)

type tglobal =
  { unbound_counter : int ref
  ; temp_counter : int ref
  ; realizations :
      (Ann.tlookup, Stmt.t Ann.ann * (string, trealization) Hashtbl.t) Hashtbl.t
  ; classes : (Ann.tlookup, (string, Ast.Ann.ttyp list) Hashtbl.t) Hashtbl.t
  ; imports : (string, (string, Ast.Ann.ttyp list) Hashtbl.t) Hashtbl.t
  ; pod_types : (string, Ann.tlookup) Hashtbl.t
  ; parser : ctx:t -> ?file:string -> string -> unit
  }

and trealization =
  { realized_ast : Stmt.t Ann.ann option
  ; realized_typ : Ann.t
  ; realized_llvm : unit Ctypes.ptr
  }

and t = (Ast.Ann.ttyp, tenv, tglobal) Ctx.t

(* ****************** Utilities ****************** *)

let next_counter c =
  incr c;
  !c

let get_cache_name (name, ann) = sprintf "%s:%s:%d" name ann.Ann.file ann.line
let push_ann ~(ctx : t) ann = Stack.push ctx.env.annotations ann
let pop_ann ~(ctx : t) = Stack.pop ctx.env.annotations

let ann ?typ (ctx : t) =
  let ann = Stack.top_exn ctx.env.annotations in
  { ann with typ }

let sannotate ~(ctx : t) node =
  let c = ann ctx in
  let patch (a, n) = Ann.{ a with pos = c.pos }, n in
  Stmt.walk ~fe:patch ~f:patch node

let eannotate ~(ctx : t) node =
  let c = ann ctx in
  let patch (a, n) = Ann.{ a with pos = c.pos }, n in
  Expr.walk ~f:patch node

(* let make_link ~ctx (t : Ann.t) = Bound (ref t)
  match t with
  | Var t -> Bound (ref t))
  | Type t -> Bound (ref t))
  | t -> t *)

let err ?(ctx : t option) fmt =
  (* let ann = Option.value ann ~default:(ann ctx) in *)
  Core.ksprintf
    (fun msg ->
      raise
      @@ SeqCamlError (msg, [ Option.value_map ctx ~f:ann ~default:(Ann.create ()) ]))
    fmt

let make_unbound ?id ?(is_generic = false) ?level (ctx : t) =
  let id = Option.value id ~default:(next_counter ctx.globals.unbound_counter) in
  let level = Option.value level ~default:ctx.env.level in
  let typ =
    let u = Ann.Unbound (id, level, is_generic) in
    Ann.Link (ref u)
  in
  Stack.push ctx.env.unbounds (ann ~typ:(Var typ) ctx);
  typ

let make_temp ?(prefix = "") (ctx : t) =
  let id = next_counter ctx.globals.temp_counter in
  sprintf "$%s_%d" prefix id

let enter_level ~(ctx : t) =
  { ctx with env = { ctx.env with level = ctx.env.level + 1 } }

let exit_level ~(ctx : t) =
  { ctx with env = { ctx.env with level = ctx.env.level - 1 } }

let add_realization ~(ctx : t) cache real =
  let l = Option.value (Hashtbl.find ctx.env.being_realized cache) ~default:[] in
  Hashtbl.set ctx.env.being_realized ~key:cache ~data:(real :: l)

let remove_last_realization ~(ctx : t) cache =
  match Hashtbl.find ctx.env.being_realized cache with
  | Some [ _ ] -> Hashtbl.remove ctx.env.being_realized cache
  | Some (_ :: data) -> Hashtbl.set ctx.env.being_realized ~key:cache ~data
  | Some [] | None -> ierr "[remove_last_realization] cannot find realization"

let get_full_name ?ctx (ann : Ann.tvar) =
  (* walk through parents in the linked list *)
  let rec iter_parent ann =
    match ann with
    | Ann.Class ({ parent = _, Some pt; _ }, _)
    | Func ({ parent = _, Some pt; _ }, _) ->
      ann :: iter_parent pt
    | _ -> [ ann ]
  in
  let parent =
    match Ann.real_type ann with
    | Func (g, _) | Class (g, _) -> g.parent
    | _ -> ierr "[get_full_name] invalid type for get_full_name"
  in
  (* Check whether the enclosing type is being currently realized! *)
  let parents =
    match parent with
    | _, Some p -> iter_parent p
    | ("", _), None -> []
    | _, None ->
      (* If parent was not set by DotExpr, check if it is being realized *)
      Option.value ~default:[] (
        ctx
        >>= fun x -> Hashtbl.find x.Ctx.env.being_realized (fst parent)
        >>= List.hd
        >>= fun x -> Ann.var_of_typ x.typ
        >>| Ann.real_type
        >>| iter_parent
      )
  in
  ppl ~sep:":" ~f:Ann.var_to_string (List.rev (ann :: parents)), parents

let parse_file ~(ctx : t) file =
  Util.dbg "parsing %s" file;
  let lines = In_channel.read_lines file in
  let code = String.concat ~sep:"\n" lines ^ "\n" in
  ctx.globals.parser ~ctx ~file:(Filename.realpath file) code

let init_module ~filename parser =
  let internal_cnt = ref 0 in
  let next_pos () =
    incr internal_cnt;
    Ann.create ~file:"<internal>" ~line:(!internal_cnt - 1) ~col:0 ()
  in
  let main_realization = "", (Ann.create ()).pos in
  let ctx =
    Ctx.init
      { unbound_counter = ref 0
      ; temp_counter = ref 0
      ; realizations = Hashtbl.Poly.create ()
      ; classes = Hashtbl.Poly.create ()
      ; pod_types = String.Table.create ()
      ; imports = String.Table.create ()
      ; parser
      }
      { level = 0
      ; unbounds = Stack.create ()
      ; enclosing_name = main_realization
      ; enclosing_return = ref None
      ; enclosing_type = None
      ; realizing = true
      ; being_realized = Hashtbl.Poly.create ()
      ; annotations = Stack.create ()
      ; statements = Stack.create ()
      ; filename
      }
  in
  Ctx.add_block ~ctx;
  (* Import stdlib *)
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
  List.iter pod_types ~f:(fun (class_name, ll) ->
      let cls =
        Stmt.(Class { class_name; generics = []; members = []; args = None })
      in
      let cache = class_name, (Ann.create ()).pos in
      let typ =
        Ann.Type (Ann.Class (
                 { generics = []
                 ; name = class_name
                 ; parent = main_realization, None
                 ; cache = cache
                 ; args = []
                 },
                 { types = None }))
      in
      Ctx.add ~ctx class_name typ;
      Hashtbl.set ctx.globals.classes ~key:cache ~data:(String.Table.create ());
      Hashtbl.set
        ctx.globals.realizations
        ~key:cache
        ~data:((Ann.create ~typ (), cls), String.Table.create ()));
  List.iter pod_types ~f:(fun (class_name, ll) ->
      (* Util.A.dr ">>>> %s" class_name; *)
      let cache = class_name, (Ann.create ()).pos in
      let methods = Llvm.Type.get_methods (ll ()) in
      List.iter methods
        ~f:(fun (s, t) ->
          let sg = Llvm.Type.get_name t in
          assert (String.prefix sg 9 = "function[");
          assert (String.suffix sg 1 = "]");
          let sg = String.drop_prefix (String.drop_suffix sg 1) 9 in
          let args = List.map (String.split ~on:',' sg) ~f:(Ctx.in_scope ~ctx) in
          if List.for_all args ~f:is_some
          then (
            let ret, args =
              match List.filter_map args ~f:Fn.id with
              | ret :: args -> ret, args
              | _ -> failwith "bad type"
            in
            (* Util.A.dr "%s.%s ->> %s" class_name s sg; *)
            let f_cache = s, (Ann.create ()).pos in
            let typ =
              Ann.Var (Ann.Func (
                { generics = []
                ; name = sprintf "%s.%s" class_name s
                ; cache = f_cache
                ; parent = cache, None
                ; args = List.map args ~f:(fun a -> "", Ann.var_of_typ_exn (Some a))
                },
                { ret = Ann.var_of_typ_exn (Some ret)
                ; used = String.Hash_set.create () }))
            in
            Ctx.add ~ctx (sprintf "%s.%s" class_name s) typ;
            let ast = Stmt.Pass () in
            Hashtbl.set
              ctx.globals.realizations
              ~key:f_cache
              ~data:((Ann.create ~typ (), ast), String.Table.create ());
            Hashtbl.find ctx.globals.classes cache
            >>| (fun h -> Hashtbl.add_multi h ~key:s ~data:typ)
            |> ignore)
          else ()));

    let str_arg = Ctx.in_scope ~ctx "str" in
    let ret, args = str_arg, [str_arg] in
    let f_cache = "__str__", (Ann.create ()).pos in
    let typ =
      Ann.Var (Ann.Func (
        { generics = []
        ; name = "str.__str__"
        ; cache = f_cache
        ; parent = ("str", (Ann.create ()).pos), None
        ; args = List.map args ~f:(fun a -> "", Ann.var_of_typ_exn a)
        },
        { ret = Ann.var_of_typ_exn ret
        ; used = String.Hash_set.create () }))
    in
    Ctx.add ~ctx "str.__str__" typ;
    let ast = Stmt.Pass () in
    Hashtbl.set
      ctx.globals.realizations
      ~key:f_cache
      ~data:((Ann.create ~typ (), ast), String.Table.create ());
    Hashtbl.find ctx.globals.classes ("str", (Ann.create ()).pos)
    >>| (fun h -> Hashtbl.add_multi h ~key:"__str__" ~data:typ)
    |> ignore;

  (* let stdlib_stmts = List.iter
    [ "__init__"
    ; "stdlib"
    ; "range"
    ; "int"
    ; "str"
    ; "list"
    ; "dict"
    ; "set"
    ]
    ~f:(fun res ->
        let res = sprintf "internal/%s" res in
        match Util.get_from_stdlib res with
        | Some file ->
          parse_file ~ctx file
        | None ->
          serr "cannot locate internal module %s" res)
  in *)
  (* Hashtbl.set imported
    ~key:"" ~data:(stdlib_stmts, String.Table.create ()); *)
  ctx

let init_empty ~(ctx : t) =
  let ctx = Ctx.init ctx.globals ctx.env in
  Ctx.add_block ~ctx;
  (* TODO
  Hashtbl.iteri ctx.globals.stdlib ~f:(fun ~key ~data ->
      add ~ctx ~internal:true ~global:true ~toplevel:true key (fst @@ List.hd_exn data)); *)
  ctx

let dump_ctx (ctx : t) =
  Stack.iter ctx.stack ~f:(fun map ->
      Hash_set.to_list map
      |> List.sort ~compare:String.compare
      |> List.iter ~f:(fun key ->
             if not (String.is_substring key ~substring:"__")
             then (
               let t = Option.value_exn (Ctx.in_scope ~ctx key) in
               Util.dbg "%10s: %s" key (Ann.typ_to_string t))))

let patch ~ctx s =
  let ann = ann ctx in
  let patch a = Ann.{ a with pos = ann.pos } in
  Stmt.walk s ~fe:(fun (a, e) -> patch a, e) ~f:(fun (a, s) -> patch a, s)

let epatch ~ctx s =
  let ann = ann ctx in
  let patch a = Ann.{ a with pos = ann.pos } in
  Expr.walk s ~f:(fun (a, s) -> patch a, s)

let make_internal_magic ~ctx name ret_typ arg_typ =
  sannotate ~ctx @@ s_extern ~lang:"llvm" name ret_typ [ "self", arg_typ ]

let magic_call ~ctx ?(args=[]) ~magic parse e =
  let e = parse ~ctx e in
  let m = Ann.create () in
  parse ~ctx @@ eannotate ~ctx (e_call (e_dot e magic) args)

  (* match Ann.var_of_typ (fst e).Ann.typ >>| Ann.real_type with
  | Some (Class ({ cache = (p, m); _ }, _)) when magic = (sprintf "__%s__" p) -> e
  | _ ->  *)
