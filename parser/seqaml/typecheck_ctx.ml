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
  ; statements : Stmt.t Ann.ann Stack.t Stack.t
  ; filename : string
  }

type tglobal =
  { unbound_counter : int ref
  ; temp_counter : int ref
  ; imports : (string, (string, Ast.Ann.ttyp list) Hashtbl.t) Hashtbl.t
  ; pod_types : (string, Ann.tlookup) Hashtbl.t
  ; sparse : ctx:t -> Stmt.t Ann.ann -> Stmt.t Ann.ann list
  ; stdlib : (string, Ast.Ann.ttyp list) Hashtbl.t
  }

and trealization =
  { realized_ast : Stmt.t Ann.ann option
  ; realized_typ : Ann.t
  ; realized_llvm : unit Ctypes.ptr
  }

and t = (Ast.Ann.ttyp, tenv, tglobal) Ctx.t

let imports : (string, Stmt.t Ann.ann list) Hashtbl.t
  = String.Table.create ()


let classes : (Ann.tlookup, (string, Ast.Ann.ttyp list) Hashtbl.t) Hashtbl.t
  = Hashtbl.Poly.create ()

let realizations : (Ann.tlookup, Stmt.t Ann.ann * (string, trealization) Hashtbl.t) Hashtbl.t
  = Hashtbl.Poly.create ()


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

let sannotate c node =
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
      raise @@ SeqCamlError (msg, [ Option.value_map ctx ~f:ann ~default:(Ann.create ()) ]))
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

let enter_level ~(ctx : t) = { ctx with env = { ctx.env with level = ctx.env.level + 1 } }
let exit_level ~(ctx : t) = { ctx with env = { ctx.env with level = ctx.env.level - 1 } }

let add_realization ~(ctx : t) cache real =
  let l = Option.value (Hashtbl.find ctx.env.being_realized cache) ~default:[] in
  Hashtbl.set ctx.env.being_realized ~key:cache ~data:(real :: l)

let remove_last_realization ~(ctx : t) cache =
  match Hashtbl.find ctx.env.being_realized cache with
  | Some [ _ ] -> Hashtbl.remove ctx.env.being_realized cache
  | Some (_ :: data) -> Hashtbl.set ctx.env.being_realized ~key:cache ~data
  | Some [] | None -> ierr "[remove_last_realization] cannot find realization"

let get_full_name ?being_realized (ann : Ann.tvar) =
  (* walk through parents in the linked list *)
  let rec iter_parent ann =
    match ann with
    | Ann.Class ({ parent = _, Some pt; _ }, _) | Func ({ parent = _, Some pt; _ }, _) ->
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
      Option.value
        ~default:[]
        (being_realized
        >>= fun x -> Hashtbl.find x (fst parent)
        >>= List.hd
        >>= fun x -> Ann.var_of_typ x.Ann.typ >>| Ann.real_type >>| iter_parent)
  in
  ppl ~sep:":" ~f:(Ann.var_to_string ~useds:true) (List.rev (ann :: parents)), parents

let dump_ctx (ctx : t) =
  Stack.iter ctx.stack ~f:(fun map ->
      Util.dbg "🥞 Stack  🥞";
      Hash_set.to_list map
      |> List.sort ~compare:String.compare
      |> List.iter ~f:(fun key ->
               let t = Option.value_exn (Ctx.in_scope ~ctx key) in
               match t with
               | Type t | Var t ->
                 (match Ann.real_type t with
                 | Func (g, _) | Class (g, _) ->
                   Util.dbg
                     "%10s: %s [%s:%s]"
                     key
                     (Ann.var_to_string ~full:true t)
                     (fst g.cache)
                     (Ann.pos_to_string (snd g.cache))
                 | _ -> Util.dbg "%10s: %s" key (Ann.var_to_string ~full:true t))
               | t -> Util.dbg "%10s: %s" key (Ann.typ_to_string t)))

let init_module ?(argv = false) ~filename sparse =
  let internal_cnt = ref 0 in
  let next_pos () =
    incr internal_cnt;
    Ann.create ~file:"/internal" ~line:(!internal_cnt - 1) ~col:0 ()
  in
  let main_realization = "", Ann.default_pos in
  let ctx =
    Ctx.init
      { unbound_counter = ref 0
      ; temp_counter = ref 0
      ; pod_types = String.Table.create ()
      ; imports = String.Table.create ()
      ; sparse
      ; stdlib = String.Table.create ()
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
  (let ctx = { ctx with map = ctx.globals.stdlib } in
   Ctx.add_block ~ctx;
   let create_class ?(generic = 0) ?(is_type = false) class_name =
     let cls =
       Stmt.(
         Class
           { class_name
           ; generics = List.init generic ~f:(fun i -> sprintf "T%d" i)
           ; members = []
           ; args = None
           })
     in
     let cache = class_name, Ann.default_pos in
     let generics =
       List.init generic ~f:(fun i ->
           let id = !(ctx.globals.unbound_counter) in
           incr ctx.globals.unbound_counter;
           sprintf "T%d" i, Ann.(id, Link (ref (Generic id))))
     in
     let typ =
       Ann.Type
         (Ann.Class
            ( { generics; name = class_name; parent = main_realization, None; cache; args = [] }
            , { is_type } ))
     in
     Ctx.add ~ctx class_name typ;
     Hashtbl.set classes ~key:cache ~data:(String.Table.create ());
     Hashtbl.set
       realizations
       ~key:cache
       ~data:((Ann.create ~typ (), cls), String.Table.create ())
   in
   let add_internal_method class_name name signature ptr =
     (* let signature = Llvm.Type.get_name ptr in *)
     (* Util.A.dy "%s" signature; *)
     assert (String.prefix signature 9 = "function[");
     assert (String.suffix signature 1 = "]");
     try
       let signature = String.drop_prefix (String.drop_suffix signature 1) 9 in
       (* Util.A.dr "%s.%s ->> %s" class_name name signature; *)
       let args =
         List.map (String.split ~on:',' signature) ~f:(fun expr ->
             Codegen.parse expr
             |> List.map ~f:(ctx.globals.sparse ~ctx)
             |> List.concat
             |> (function
                | [ _, Stmt.Expr ({ typ; _ }, _) ] -> typ
                | _ -> ierr "parse expected single expression"))
       in
       let ret, args =
         match args with
         | ret :: args -> ret, args
         | _ -> failwith "bad type"
       in
       let full_name = sprintf "%s.%s" class_name name in
       let f_cache = full_name, Ann.default_pos in
       let typ =
         Ann.Var
           (Ann.Func
              ( { generics = []
                ; name = full_name
                ; cache = f_cache
                ; parent = (class_name, Ann.default_pos), None
                ; args = List.mapi args ~f:(fun i a -> sprintf "a%d" i, Ann.var_of_typ_exn a)
                }
              , { ret = Ann.var_of_typ_exn ret; used = String.Hash_set.create () } ))
       in
       Ctx.add ~ctx full_name typ;
       let ast = Stmt.Pass () in
       let realization =
        { realized_llvm = ptr; realized_ast = None; realized_typ = Ann.create ~typ () }
       in
       let realization_name = sprintf "%s:%s" class_name (Ann.typ_to_string typ) in
       (* Util.A.db "add realization %s:%s.%s => %s := %nx"
        (fst f_cache) (Ann.pos_to_string (snd f_cache))
        (Ann.typ_to_string ~full:true typ)
        realization_name
        (Ctypes.raw_address_of_ptr ptr)
        ; *)
       Hashtbl.set
         realizations
         ~key:f_cache
         ~data:((Ann.create ~typ (), ast), String.Table.of_alist_exn [ realization_name, realization ]);
       Hashtbl.find_and_call
         classes
         (class_name, Ann.default_pos)
         ~if_found:(Hashtbl.add_multi ~key:name ~data:typ)
         ~if_not_found:(fun _ -> Util.A.dy "can't find class %s" class_name)
     with
     | SeqCamlError _ -> ()
   in
   (* Initialize C POD types *)
   let pod_types =
     List.concat
       Llvm.Type.
         [ [ "void", void (); "int", int (); "bool", bool (); "float", float (); "byte", byte () ]
         ; (*List.init 2048*)
           List.map [ 1; 2; 4; 8; 16; 32; 64; 128; 256 ] ~f:(fun n -> sprintf "u%d" n, uintN n)
         ; (*List.init 2048*)
           List.map [ 1; 2; 4; 8; 16; 32; 64; 128; 256 ] ~f:(fun n -> sprintf "i%d" n, intN n)
         ; (*List.init 1024*)
           List.map [ 1; 2; 4; 8; 16; 32; 64; 128; 256 ] ~f:(fun n -> sprintf "k%d" n, kmerN n)
         ]
   in
   (* create_class ~generic:1 "Kmer"; *)
   List.iter pod_types ~f:(fun (name, _) -> create_class name ~is_type:true);
   Codegen.parse
     (Util.unindent
      {|
      class ptr[T]:
        cdef __init__(self: ptr[T])
        cdef __init__(self: ptr[T], i: int)
        cdef __init__(self: ptr[T], other: T)
        cdef __init__(self: ptr[T], other: ptr[T])
        cdef __copy__(self: ptr[T]) -> ptr[T]
        cdef __bool__(self: ptr[T]) -> bool
        cdef __getitem__(self: ptr[T], i: int) -> T
        cdef __setitem__(self: ptr[T], i: int, j: T) -> void
        cdef __add__(self: ptr[T], i: int) -> ptr[T]
        cdef __sub__(self: ptr[T], x: ptr[T]) -> int
        cdef __eq__(self: ptr[T], x: ptr[T]) -> bool
        cdef __ne__(self: ptr[T], x: ptr[T]) -> bool
        cdef __lt__(self: ptr[T], x: ptr[T]) -> bool
        cdef __gt__(self: ptr[T], x: ptr[T]) -> bool
        cdef __le__(self: ptr[T], x: ptr[T]) -> bool
        cdef __ge__(self: ptr[T], x: ptr[T]) -> bool
        # TODO: prefetchs
      class generator[T]:
        cdef __iter__(self: generator[T]) -> generator[T]
      type array[T](ptr: ptr[T], len: int):
        cdef __elemsize__() -> int
        cdef __init__(self: array[T], p: ptr[T], i: int) -> array[T]
        cdef __init__(self: array[T], i: int) -> array[T]
        cdef __copy__(self: array[T]) -> array[T]
        cdef __len__(self: array[T]) -> int
        cdef __bool__(self: array[T]) -> bool
        cdef __getitem__(self: array[T], i: int) -> T
        cdef __slice__(self: array[T], i: int, j: int) -> array[T]
        cdef __slice_left__(self: array[T], i: int) -> array[T]
        cdef __slice_right__(self: array[T], i: int) -> array[T]
        cdef __setitem__(self: array[T], i: int, j: T) -> void
      type __array__[T](ptr: ptr[T], len: int):
        cdef __getitem__(self: __array__[T], i: int) -> T
        cdef __setitem__(self: __array__[T], i: int, j: T) -> void
      type str(ptr: ptr[byte], len: int)
      type seq(ptr: ptr[byte], len: int)
      |})
   |> List.map ~f:(sannotate Ann.default)
   |> List.ignore_map ~f:(ctx.globals.sparse ~ctx);
   List.iter [ "str" ; "seq" ] ~f:(fun s ->
     let h = Hashtbl.find_exn classes (s, Ann.default_pos) in
     Hashtbl.remove h "__init__");
   List.iter
     (pod_types @ Llvm.Type.[ "str", str (); "seq", seq () ])
     ~f:(fun (n, ll) ->
       let open Llvm.Type in
       get_methods ll
       |> List.iter ~f:(fun (s, g, t) -> add_internal_method n s g t));
   (* dump_ctx ctx; *)

   (* argv! *)
   if true
   then (
     match Util.get_from_stdlib "stdlib" with
     | Some file ->
        let statements =
          Codegen.parse_file file
          |> List.map ~f:(ctx.globals.sparse ~ctx)
          |> List.concat
        in
        Hashtbl.set imports ~key:file ~data:statements
     | None -> Err.ierr "cannot locate stdlib.seq"));
  Hashtbl.iteri ctx.globals.stdlib ~f:(fun ~key ~data -> Ctx.add ~ctx key (List.hd_exn data));
  ctx

let init_empty ~(ctx : t) =
  let ctx = Ctx.init ctx.globals ctx.env in
  Ctx.add_block ~ctx;
  Hashtbl.iteri ctx.globals.stdlib ~f:(fun ~key ~data -> Ctx.add ~ctx key (List.hd_exn data));
  ctx

let patch ~ctx s =
  let ann = ann ctx in
  let patch a = Ann.{ a with pos = ann.pos } in
  Stmt.walk s ~fe:(fun (a, e) -> patch a, e) ~f:(fun (a, s) -> patch a, s)

let epatch ~ctx s =
  let ann = ann ctx in
  let patch a = Ann.{ a with pos = ann.pos } in
  Expr.walk s ~f:(fun (a, s) -> patch a, s)

let make_internal_magic ~ctx name ret_typ arg_types =
  sannotate (ann ctx) @@ s_extern ~lang:"llvm" name ret_typ arg_types

let magic_call ~ctx ?(args = []) ~magic parse e =
  (* let e = parse ~ctx e in
  match Ann.var_of_typ (fst e).Ann.typ >>| Ann.real_type with
  | Some (Class ({ cache = (p, m); _ }, _)) when magic = (sprintf "__%s__" p) && m = Ann.default_pos ->
    e
  | _ -> *)
    let y = parse ~ctx @@ eannotate ~ctx (e_call (e_dot e magic) args) in
    match y with
    | Expr.(ac, Call ((ad, Dot (i, _)) as d, _)) as c ->
      (* Util.A.dy "%s [%s] . %s [%s]"
      (Expr.to_string d)
      (Ann.t_to_string ad.Ann.typ)
      (Expr.to_string c)
      (Ann.t_to_string ac.Ann.typ); *)
      y
    | _ -> err ~ctx "invalid magic %s" magic

