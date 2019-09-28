(* ****************************************************************************
 * Seqaml.Typecheck_stmt: Typecheck Seq code from expression AST
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Err
open Option.Monad_infix

(** This module implements [Typecheck_intf.Stmt].
    Parametrized by [Typecheck_intf.Expr] for parsing generators *)
module Typecheck (E : Typecheck_intf.Expr) (R : Typecheck_intf.Real) :
  Typecheck_intf.Stmt = struct
  open Ast
  open Ast.Stmt
  module C = Typecheck_ctx
  module T = Typecheck_infer

  let rec parse ~(ctx : C.t) (s : Stmt.t Ann.ann) : Stmt.t Ann.ann list =
    let ann, node = s in
    C.push_ann ~ctx ann;
    let (node : Stmt.t) =
      match node with
      | Expr p -> Expr (parse_expr ctx p)
      | Assert p -> Assert (parse_expr ctx p)
      | Throw p -> Throw (parse_expr ctx p)
      | Assign p -> parse_assign ctx p
      | Declare p -> parse_declare ctx p
      | Del p -> parse_del ctx p
      | Print p -> parse_print ctx p
      | Return p -> parse_return ctx p
      | Yield p -> parse_yield ctx p
      | If p -> parse_if ctx p
      | While p -> parse_while ctx p
      | For p -> parse_for ctx p
      | Match p -> parse_match ctx p
      | Import p -> parse_import ctx p
      | Try p -> parse_try ctx p
      | Extern p -> parse_extern ctx p
      | Function p -> parse_function ctx p
      | Extend p -> parse_extend ctx p
      | Class p -> parse_class ctx p
      | Type p -> parse_class ctx p ~is_type:true
      | Special p -> parse_special ctx p
      | Pass _ | Break _ | Continue _ | Global _ -> node
      | TypeAlias _ | Prefetch _ | ImportPaste _ -> C.err ~ctx "not supported :/"
    in
    let stmts = (ann, node) :: Stack.to_list ctx.env.statements in
    Stack.clear ctx.env.statements;
    C.pop_ann ~ctx |> ignore;
    (* Util.A.dr "%s -> %s" (Stmt.to_string s) (Stmt.to_string (ann, node)); *)
    List.rev stmts

  and parse_realized ~(ctx : C.t) stmts =
    let stmts = List.concat @@ List.map stmts ~f:(parse ~ctx) in
    Stack.iter ctx.env.unbounds ~f:(fun u ->
        if Ann.has_unbound u
        then Util.A.dy "unbound %s is not realized" (Ann.to_string u));
    stmts

  and parse_block ctx stmts =
    Ctx.add_block ~ctx;
    let stmts = List.concat @@ List.map stmts ~f:(parse ~ctx) in
    Ctx.clear_block ~ctx;
    stmts

  (* ***************************** *)
  and parse_expr ctx e = E.parse ~ctx e

  and parse_assign ctx ?(prefix = "") (lhs, rhs, shadow, dectyp) =
    let rhs = E.parse ~ctx:(C.enter_level ~ctx) rhs in
    match snd lhs, dectyp with
    | Id var, _ ->
      let t = fst rhs in
      dectyp >>| E.parse ~ctx >>| fst >>| T.unify_inplace ~ctx t |> ignore;
      (match Ctx.in_scope ~ctx var with
      | Some (Var t | Type t) when T.unify t t <> -1 -> T.unify_inplace ~ctx t t
      | Some (Var t | Type t) ->
        Util.dbg
          "shadowing %s : %s with %s"
          var
          (Ann.typ_to_string t)
          (Ann.typ_to_string (fst rhs));
        Ctx.add ~ctx (prefix ^ var) (if t.is_type_ast then Type t else Var t)
      | None -> Ctx.add ~ctx (prefix ^ var) (if t.is_type_ast then Type t else Var t)
      | _ -> ierr ~ctx "[parse_assign] invalid assigment");
      let ann = { (fst lhs) with typ = t.typ } in
      Assign (e_id ~ann var, rhs, shadow, None)
    | Dot d, None ->
      (* a.x = b *)
      let lhs = E.parse ~ctx lhs in
      T.unify_inplace ~ctx (fst lhs) (fst rhs);
      Assign (lhs, rhs, shadow, None)
    | Index (var_expr, [ idx ]), None ->
      (* a[x] = b *)
      Expr (C.magic_call E.parse ~ctx ~magic:"__setitem__" var_expr ~args:[ idx; rhs ])
    | _ -> C.err ~ctx "invalid assign statement"

  and parse_declare ?(prefix = "") ctx { name; typ } =
    let e =
      match typ with
      | Some typ -> E.parse ~ctx typ
      | None -> C.err ~ctx "invalid declare"
    in
    if is_some @@ Ctx.in_scope ~ctx name then Util.dbg "shadowing %s" name;
    let t = { (fst e) with is_type_ast = false } in
    Ctx.add ~ctx (prefix ^ name) (Var t);
    Declare { name; typ = Some (t, snd e) }

  and parse_del ctx expr =
    match snd expr with
    | Expr.Index (lhs, [ index ]) ->
      Expr (C.magic_call E.parse ~ctx ~magic:"__delitem__" lhs ~args:[ index ])
    | Id var ->
      Ctx.remove ~ctx var;
      Del (E.parse ~ctx expr)
    | _ -> C.err ~ctx "invalid del statement"

  and parse_print ctx (exprs, term) =
    let exprs = List.map exprs ~f:(C.magic_call E.parse ~ctx ~magic:"__str__") in
    Print (exprs, term)

  and parse_return ctx expr =
    let t, expr =
      match expr with
      | None -> R.internal ~ctx "void", None
      | Some expr ->
        let expr = E.parse ~ctx expr in
        fst expr, Some expr
    in
    (match !(ctx.env.enclosing_return) with
    | { typ = Unknown; _ } -> ctx.env.enclosing_return := t
    | rt -> T.unify_inplace ~ctx rt t);
    Return expr

  and parse_yield ctx expr =
    let t, expr =
      match expr with
      | None -> R.internal ~ctx ~args:[ R.internal ~ctx "void" ] "generator", None
      | Some expr ->
        let expr = E.parse ~ctx expr in
        R.internal ~ctx ~args:[ fst expr ] "generator", Some expr
    in
    (match !(ctx.env.enclosing_return) with
    | { typ = Unknown; _ } -> ctx.env.enclosing_return := t
    | rt -> T.unify_inplace ~ctx rt t);
    Yield expr

  and parse_while ctx (cond, stmts) =
    let cond = C.magic_call E.parse ~ctx ~magic:"__bool__" cond in
    let stmts = parse_block ctx stmts in
    While (cond, stmts)

  and parse_for ctx (for_vars, gen_expr, stmts) =
    let gen_expr = C.magic_call E.parse ~ctx ~magic:"__iter__" gen_expr in
    Ctx.add_block ~ctx;
    (* make sure it is ... ah, generator! *)
    let tvar =
      match (Ann.real_type (fst gen_expr)).typ with
      | Class { c_cache = "generator", _; c_generics = [ (_, (_, t)) ]; _ } -> t
      | TypeVar { contents = Unbound _ } -> C.make_unbound ctx
      | _ ->
        C.err ~ctx "for expression must be an iterable generator %s"
        @@ Ann.typ_to_string (fst gen_expr)
    in
    let for_var, init_stmts =
      match for_vars with
      | [ for_var ] ->
        Ctx.add ~ctx for_var (Var tvar);
        for_var, []
      | for_vars ->
        let for_var = C.make_temp ~prefix:"unroll" ctx in
        Ctx.add ~ctx for_var (Var tvar);
        ( for_var
        , List.mapi for_vars ~f:(fun idx v ->
              C.sannotate ~ctx
              @@ s_assign (e_id v) (e_index (e_id for_var) [ e_int idx ])) )
    in
    let stmts = List.concat @@ List.map (init_stmts @ stmts) ~f:(parse ~ctx) in
    Ctx.clear_block ~ctx;
    For ([ for_var ], gen_expr, stmts)

  and parse_if ctx cases =
    If
      (List.map cases ~f:(fun { cond; cond_stmts } ->
           let cond = cond >>| C.magic_call E.parse ~ctx ~magic:"__bool__" in
           let cond_stmts = parse_block ctx cond_stmts in
           { cond; cond_stmts }))

  and parse_match ctx (what, cases) =
    let what = E.parse ~ctx what in
    Match
      ( what
      , List.map cases ~f:(fun { pattern; case_stmts } ->
            Ctx.add_block ~ctx;
            let pat_t, pattern =
              match pattern with
              | BoundPattern (name, pat) ->
                Ctx.add_block ~ctx;
                let p = parse_pattern ctx (fst what) pat in
                Ctx.clear_block ~ctx;
                Ctx.add ~ctx name (Var (fst what));
                p
              | _ as pat ->
                Ctx.add_block ~ctx;
                let p = parse_pattern ctx (fst what) pat in
                Ctx.clear_block ~ctx;
                p
            in
            T.unify_inplace ~ctx (fst what) pat_t;
            let case_stmts = parse_block ctx case_stmts in
            Ctx.clear_block ~ctx;
            { pattern; case_stmts }) )

  and parse_pattern ctx what_typ = function
    | StarPattern -> C.make_unbound ctx, StarPattern
    | BoundPattern _ -> C.err ~ctx "invalid bound pattern"
    | IntPattern i -> R.internal ~ctx "int", IntPattern i
    | BoolPattern b -> R.internal ~ctx "bool", BoolPattern b
    | StrPattern s -> R.internal ~ctx "str", StrPattern s
    | SeqPattern s -> R.internal ~ctx "seq", StrPattern s
    | RangePattern r -> R.internal ~ctx "int", RangePattern r
    | OrPattern tl ->
      let tl = List.map tl ~f:(parse_pattern ctx what_typ) in
      let t = fst (List.hd_exn tl) in
      List.iter tl ~f:(fun p -> T.unify_inplace ~ctx t (fst p));
      t, OrPattern (List.map ~f:snd tl)
    | WildcardPattern (Some wild) ->
      Ctx.add ~ctx wild (Var what_typ);
      what_typ, WildcardPattern (Some wild)
    | WildcardPattern None -> what_typ, WildcardPattern None
    | GuardedPattern (pat, cond) ->
      let t, pat = parse_pattern ctx what_typ pat in
      let cond = C.magic_call E.parse ~ctx ~magic:"__bool__" cond in
      t, GuardedPattern (pat, cond)
    | ListPattern [] ->
      let lt = C.make_unbound ctx in
      R.internal ~ctx "list" ~args:[ lt ], ListPattern []
    | ListPattern (hd :: tl) ->
      let lt, hd = parse_pattern ctx what_typ hd in
      let l =
        List.map (hd :: tl) ~f:(fun p ->
            let t, p = parse_pattern ctx what_typ p in
            T.unify_inplace ~ctx lt t;
            p)
      in
      R.internal ~ctx "list" ~args:[ lt ], ListPattern l
    | TuplePattern l ->
      let l = List.map l ~f:(parse_pattern ctx what_typ) in
      Ann.create ~typ:(Tuple (List.map l ~f:fst)) (), TuplePattern (List.map l ~f:snd)

  and parse_import (ctx : C.t) ?(ext = ".seq") imports =
    let import (ctx : C.t) file Stmt.{ from; what; import_as } =
      let vtable =
        match Hashtbl.find ctx.globals.imports file with
        | Some t -> t
        | None ->
          let ictx = C.init_empty ~ctx in
          let lines = In_channel.read_lines file in
          let code = String.concat ~sep:"\n" lines ^ "\n" in
          let ast = Codegen.parse ~file code in
          let stmts = List.concat @@ List.map ast ~f:(parse ~ctx:ictx) in
          Hashtbl.set ctx.globals.imports ~key:file ~data:ictx.map;
          ictx.map
      in
      let additions =
        match what with
        | None ->
          (* import <from> (as <import as>) *)
          let name = Option.value import_as ~default:from in
          [ name, C.Import file ]
        | Some [ ("*", None) ] ->
          (* from <from> import <what := *> *)
          List.filter_map (Hashtbl.to_alist vtable) ~f:(function
              | s, [] -> None
              | s, h :: _ -> Some (s, h))
        | Some lst ->
          (* from <from> import <what> *)
          List.map lst ~f:(fun (name, import_as) ->
              match Hashtbl.find vtable name with
              | Some (data :: _) -> Option.value import_as ~default:name, data
              | _ -> C.err ~ctx "name %s not found in %s" name from)
      in
      List.iter additions ~f:(fun (key, data) -> Ctx.add ~ctx key data)
    in
    List.iter imports ~f:(fun i ->
        let from = i.from in
        let file = sprintf "%s/%s%s" (Filename.dirname ctx.env.filename) from ext in
        let file =
          match Sys.file_exists file with
          | `Yes -> file
          | `No | `Unknown ->
            (match Util.get_from_stdlib ~ext from with
            | Some file -> file
            | None -> C.err ~ctx "cannot locate module %s" from)
        in
        import ctx file i);
    Import imports

  and parse_try ctx (stmts, catches, finally) =
    let stmts = parse_block ctx stmts in
    let catches =
      List.map catches ~f:(fun { exc; var; stmts } ->
          Ctx.add_block ~ctx;
          let exc =
            match exc, var with
            | Some exc, Some var ->
              let exc = E.parse ~ctx exc in
              Ctx.add ~ctx var (Var (fst exc));
              Some exc
            | Some exc, None -> Some (E.parse ~ctx exc)
            | _ -> ierr "[parse_try] malformed try node"
          in
          let stmts = List.concat @@ List.map stmts ~f:(parse ~ctx) in
          Ctx.clear_block ~ctx;
          { exc; var; stmts })
    in
    let finally = finally >>| parse_block ctx in
    Try (stmts, catches, finally)

  and parse_extern ctx ?(prefix = "") f =
    let lang, dylib, ctxn, fn, args = f in
    let args =
      List.map args ~f:(fun arg ->
          match arg.typ with
          | Some typ ->
            let typ = E.parse ~ctx typ in
            { arg with typ = Some typ }, (arg.name, fst typ)
          | None -> C.err ~ctx "extern functions must explicitly state parameter types"
      )
    in
    let fret =
      match fn.typ with
      | Some typ -> E.parse ~ctx typ
      | None -> C.err ~ctx "extern functions must explicitly state return type"
    in
    let tf' =
      Ann.
        { f_generics = []
        ; f_name = prefix ^ fn.name
        ; f_parent = ctx.env.enclosing_name, None
        ; f_args = List.map args ~f:snd
        ; f_cache = fn.name, C.ann ctx
        ; f_ret = fst fret
        }
    in
    let typ = C.make_link ~ctx { (C.ann ctx) with typ = Func tf' } in
    Ctx.add ~ctx (prefix ^ fn.name) (Var typ);
    let fn = { fn with typ = Some fret } in
    let node = C.ann ~typ ctx, Extern (lang, dylib, ctxn, fn, List.map ~f:fst args) in
    Hashtbl.set
      ctx.globals.realizations
      ~key:tf'.f_cache
      ~data:(node, String.Table.create ());
    ignore @@ R.realize ~ctx typ;
    snd node

  and parse_function ctx ?(prefix = "") ?(shalow = false) f =
    let ctx = C.enter_level ~ctx in
    Ctx.add_block ~ctx;
    let explicits =
      List.map f.fn_generics ~f:(fun gen ->
          let typ = C.make_unbound ~is_generic:true ctx in
          let typ = { typ with is_type_ast = true } in
          let id = !(ctx.globals.unbound_counter) in
          Ctx.add ~ctx gen (Type typ);
          gen, (id, typ))
    in
    let fname = prefix ^ f.fn_name.name in
    let fret =
      Option.value_map f.fn_name.typ ~default:(C.make_unbound ctx) ~f:(fun t ->
          fst (E.parse ~ctx t))
    in
    let fn_name = { name = fname; typ = None } in
    let fn_args, args =
      List.unzip
      @@ List.mapi f.fn_args ~f:(fun i arg ->
             let arg_typ = Option.map arg.typ ~f:(fun t -> E.parse ~ctx t) in
             let typ =
               match arg_typ with
               | Some t -> Ann.real_type (fst t)
               | None when i = 0 && arg.name = "self" && prefix <> "" ->
                 (match Ctx.in_scope ~ctx (String.drop_suffix prefix 1) with
                 | Some (Type t) -> t
                 | _ ->
                   C.err ~ctx "cannot locate %s (self)" (String.drop_suffix prefix 1))
               | None -> C.make_unbound ctx
             in
             Ctx.add ~ctx arg.name (Var typ);
             { name = arg.name; typ = arg_typ }, (arg.name, typ))
    in
    (* Add self-refential type for recursive calls *)
    let tf' =
      Ann.
        { f_generics =
            List.map explicits ~f:(fun (n, (id, t)) ->
                n, (id, T.generalize ~ctx ~level:(ctx.env.level - 1) t))
        ; f_name = fname
        ; f_parent = ctx.env.enclosing_name, None
        ; f_args =
            List.map args ~f:(fun (n, t) ->
                n, T.generalize ~ctx ~level:(ctx.env.level - 1) t)
        ; f_ret = T.generalize ~ctx ~level:(ctx.env.level - 1) fret
        ; f_cache = f.fn_name.name, C.ann ctx
        }
    in
    let tfun = C.make_link ~ctx { (C.ann ctx) with typ = Func tf' } in
    let fn_stmts =
      if shalow
      then f.fn_stmts
      else (
        let ctx =
          { ctx with
            env =
              { ctx.env with
                realizing = false
              ; enclosing_name = tf'.f_cache
              ; enclosing_return = ref (Ann.create ())
              }
          }
        in
        Ctx.add ~ctx fname (Var tfun);
        let fn_stmts = List.concat @@ List.map f.fn_stmts ~f:(parse ~ctx) in
        let ret' =
          match !(ctx.env.C.enclosing_return) with
          | { typ = Unknown; _ } -> R.internal ~ctx "void"
          | typ -> typ
        in
        T.unify_inplace ~ctx ret' fret;
        fn_stmts)
    in
    let ctx = C.exit_level ~ctx in
    Ctx.clear_block ~ctx;
    List.iter explicits ~f:(fun (_, (_, t)) -> T.generalize_inplace ~ctx t);
    List.iter args ~f:(fun (_, t) -> T.generalize_inplace ~ctx t);
    T.generalize_inplace ~ctx fret;
    let tfun =
      { (C.ann ctx) with
        typ = Func { tf' with f_generics = explicits; f_args = args; f_ret = fret }
      }
    in
    (* all functions must be as general as possible: generic unification is not allowed *)
    let tfun = T.generalize ~ctx ~level:0 tfun in
    Ctx.add ~ctx fname (Var tfun);
    let node = tfun, Function { f with fn_name; fn_args; fn_stmts } in
    Hashtbl.set
      ctx.globals.realizations
      ~key:tf'.f_cache
      ~data:(node, String.Table.create ());
    let tfun = if Ann.is_realizable tfun then R.realize ~ctx tfun else tfun in
    Util.dbg "|| [fun] %s |- %s" fname @@ Ann.typ_to_string tfun;
    snd node

  and prn_class ?(generics = Int.Table.create ()) lev (name, typ) =
    Util.dbg "|| %s[typ] %s |- %s" (String.make (2 * lev) ' ') name
    @@ Ann.typ_to_string ~generics typ

  and parse_extend (ctx : C.t) (name, new_members) =
    let class_name =
      match snd name with
      | Expr.Id name -> name
      | _ -> C.err ~ctx "bad class name"
    in
    let cls, tcls, typ =
      match Ctx.in_scope ~ctx class_name with
      | Some (Type t) ->
        let t = Ann.real_type t in
        let c = Ann.cls t in
        fst @@ Hashtbl.find_exn ctx.globals.realizations c.c_cache, c, t
      | _ -> C.err ~ctx "%s is not a class" class_name
    in
    Ctx.add_block ~ctx;
    let ctx = C.enter_level ~ctx in
    let explicits =
      List.map tcls.c_generics ~f:(fun (gen, (id, t)) ->
          if not (Ann.has_unbound t) then ierr "unbound explicit";
          let t = C.make_unbound ~id ctx in
          Ctx.add ~ctx gen (Type t);
          t)
    in
    let ctx =
      { ctx with
        env =
          { ctx.env with
            enclosing_name = tcls.c_cache
          ; enclosing_type = Some tcls.c_cache
          ; realizing = false
          }
      }
    in
    let members = Hashtbl.find_exn ctx.globals.classes tcls.c_cache in
    List.iter new_members ~f:(fun s ->
        let prefix = sprintf "%s." class_name in
        match snd s with
        | Function f ->
          ignore @@ parse_function ctx ~prefix f;
          let typ = Ctx.in_block ~ctx (prefix ^ f.fn_name.name) in
          Hashtbl.add_multi members ~key:f.fn_name.name ~data:(Option.value_exn typ)
        | Extern ((_, _, _, fn, _) as f) ->
          ignore @@ parse_extern ctx ~prefix f;
          let typ = Ctx.in_block ~ctx (prefix ^ fn.name) in
          Hashtbl.add_multi members ~key:fn.name ~data:(Option.value_exn typ)
        | Class c ->
          let stmt = parse_class ctx ~prefix c in
          let typ = Ctx.in_block ~ctx (prefix ^ c.class_name) in
          Hashtbl.set members ~key:c.class_name ~data:[ Option.value_exn typ ]
        | _ -> ierr "invalid extend member");
    let ctx = C.exit_level ~ctx in
    List.iter tcls.c_generics ~f:(fun (g, _) -> Ctx.remove ~ctx g);
    List.iter explicits ~f:(T.generalize_inplace ~ctx);
    Extend (name, new_members)

  and parse_class ctx ?(prefix = "") ?(is_type = false) cls =
    let { class_name; generics; members; _ } = cls in
    let cache = class_name, C.ann ctx in
    (* Ctx.push ctx; *)
    let ctx = C.enter_level ~ctx in
    let old_explicits = List.map generics ~f:(Ctx.in_scope ~ctx) in
    let explicits =
      List.map generics ~f:(fun gen ->
          let t = C.make_unbound ~is_generic:true ctx in
          let t = { t with is_type_ast = true } in
          let id = !(ctx.globals.unbound_counter) in
          Util.A.dr "[class %s] generic %s -> %s // %d"
            class_name gen (Ann.typ_to_string ~full:true t) id;
          Ctx.add ~ctx gen (Type t);
          gen, (id, t))
    in
    let class_name = prefix ^ class_name in
    let ctyp =
      if not is_type
      then None
      else
        Some
          (List.filter_map members ~f:(function
              | _, Assign (((_, Id name), _, _, typ) as d) ->
                typ >>| E.parse ~ctx >>| fst
              | _ -> None))
    in
    let members = String.Table.create () in
    let tc' =
      Ann.
        { c_generics =
            List.map explicits ~f:(fun (n, (id, t)) ->
                n, (id, T.generalize ~ctx ~level:(pred ctx.env.level) t))
            (* seal for later use; dependents will still use same-numbered unbound while
           dependend recursive realization will reinstantiate it as another number *)
        ; c_name = class_name
        ; c_parent = ctx.env.enclosing_name, None
        ; c_cache = cache
        ; c_args = []
        ; c_type = ctyp
        }
    in
    let tcls =
      C.make_link ~ctx { (C.ann ctx) with typ = Class tc'; is_type_ast = true }
    in
    let ctx =
      { ctx with
        env =
          { ctx.env with
            enclosing_name = cache
          ; enclosing_type = Some cache
          ; realizing = false
          }
      }
    in
    Ctx.add ~ctx class_name (Type tcls);
    Hashtbl.set ctx.globals.classes ~key:cache ~data:members;
    let init =
      C.make_internal_magic ~ctx "__init__" (e_id "void")
      @@
      match cls.generics with
      | [] -> e_id class_name
      | l -> e_index (e_id class_name) (List.map cls.generics ~f:e_id)
    in
    let cls_members =
      List.map (init :: cls.members) ~f:(fun s ->
          let prefix = sprintf "%s." class_name in
          let typ, node =
            match snd s with
            | Function f ->
              Util.A.dr "--> f %s %s" prefix (Stmt.to_string s);
              let stmt = parse_function ctx ~prefix f in
              let typ = Ctx.in_block ~ctx (prefix ^ f.fn_name.name) in
              Hashtbl.add_multi
                members
                ~key:f.fn_name.name
                ~data:(Option.value_exn typ);
              typ, stmt
            | Extern ((_, _, _, fn, _) as f) ->
              let stmt = parse_extern ctx ~prefix f in
              let typ = Ctx.in_block ~ctx (prefix ^ fn.name) in
              Hashtbl.add_multi members ~key:fn.name ~data:(Option.value_exn typ);
              typ, stmt
            | Class c ->
              let stmt = parse_class ctx ~prefix c in
              let typ = Ctx.in_block ~ctx (prefix ^ c.class_name) in
              Hashtbl.set members ~key:c.class_name ~data:[ Option.value_exn typ ];
              typ, stmt
            | Declare ({ name; _ } as d) ->
              let stmt = parse_declare ctx ~prefix d in
              let typ = Ctx.in_block ~ctx (prefix ^ name) in
              let typ =
                match typ with
                | Some (Var t | Member t | Type t) -> t
                | _ -> failwith "err"
              in
              Hashtbl.set members ~key:name ~data:[ Member typ ];
              Util.A.dr "[class %s] param %s -> %s"
                  class_name name (Ann.typ_to_string ~full:true typ);

              Some (Member typ), stmt
            | _ -> ierr "invalid class member"
          in
          let typ =
            match typ with
            | Some (Var t | Member t | Type t) -> t
            | _ -> failwith "err"
          in
          let typ = typ.typ in
          { (fst s) with typ }, node)
    in
    let ctx = C.exit_level ~ctx in
    List.iter cls.generics ~f:(Ctx.remove ~ctx);
    List.iter2_exn cls.generics old_explicits ~f:(fun g t ->
        match t with
        | Some t -> Ctx.add ~ctx g t
        | None -> ());
    List.iter explicits ~f:(fun (_, (_, t)) -> T.generalize_inplace ~ctx t);
    let n = { cls with class_name; generics; members = cls_members } in
    let node = C.ann ~typ:tcls ctx, if is_type then Type n else Class n in
    Hashtbl.set ctx.globals.realizations ~key:cache ~data:(node, String.Table.create ());
    let tcls =
      if Ann.is_realizable tcls
      then R.realize ~ctx:{ ctx with env = { ctx.env with realizing = true } } tcls
      else tcls
    in
    (* prn_class 0 (cls.class_name, tcls); *)
    snd node

  (* and parse_realize ctx ann (n, real) =
    let ast, _ = Hashtbl.find_exn G.generics real in
    let name, typ, is_typ = match snd ast, ann.typ with
      | Class c, Some t -> c.class_name, t, G.Type
      | Type c, Some t -> c.class_name, t, G.Type
      | Function f, Some t -> f.fn_name.name, t, G.Var
      | Extern (_, _, _, n, _), Some t -> n.name, t, G.Var
      | _ -> failwith "cant happen [parse_realize]"
    in
    Ctx.add ctx name (typ, is_typ);
    ann.typ, StmtNode.Realize (n, real) *)
  and parse_special ctx (name, exprs, _) = ierr "No specials here"
end
