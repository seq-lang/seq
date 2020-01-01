(* ****************************************************************************
 * Seqaml.Typecheck_stmt: Typecheck Seq code from expression AST
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Util
open Err
open Option.Monad_infix

open Ast
open Ast.Stmt

module C = Typecheck_ctx
module T = Typecheck_infer
module E = Typecheck_expr
module R = Typecheck_realize

let rec parse ~(ctx : C.t) (s : Stmt.t Ann.ann) : Stmt.t Ann.ann list =
  let ann, node = s in
  C.push_ann ~ctx ann;
  (* Util.A.dr "[s] %s" (Stmt.to_string s); *)
  Stack.push ctx.env.statements (Stack.create ());
  let (node : Stmt.t) =
    match node with
    | Expr p -> Expr (parse_expr ctx p)
    | Assert p -> Assert (parse_expr ctx p)
    | Throw p -> Throw (parse_expr ctx p)
    | AssignEq p -> parse_assigneq ctx p
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
    | Type p -> parse_class ctx p ~istype:true
    | Special p -> parse_special ctx p
    | ImportPaste s -> parse_impaste ctx s
    | Pass _ | Break _ | Continue _ | Global _ -> node
    | TypeAlias s -> parse_alias ctx s
    | Prefetch _ -> C.err ~ctx "not supported :/"
  in
  let stmts = (ann, node) :: Stack.to_list (Stack.pop_exn ctx.env.statements) in
  ignore @@ C.pop_ann ~ctx;
  (* List.iter (List.rev stmts) ~f:(fun s -> Util.A.dr ~force:true ":: %s" (Stmt.to_string s)); *)
  List.rev stmts

and parse_block ctx stmts =
  Ctx.add_block ~ctx;
  let stmts = List.concat @@ List.map stmts ~f:(parse ~ctx) in
  Ctx.clear_block ~ctx;
  stmts

(* ***************************** *)
and parse_expr ctx e = E.parse ~ctx e

and parse_assigneq ctx (lh', op, rh') =
  let magic = Util.bop2magic ~prefix:"i" op in
  let lh = E.parse ~ctx lh' in
  let rh = E.parse ~ctx rh' in
  match R.magic ~ctx ~args:[ var_of_node_exn rh ] (var_of_node_exn lh) magic with
  | None ->
    parse_assign ctx ([lh'], [C.eannotate ~ctx @@ e_binary lh' op rh'], Shadow, None)
  | Some t ->
    Expr (C.magic_call E.parse ~ctx ~magic lh ~args:[ rh ])

and parse_assign ctx ?(prefix = "") (lhs, rhs, shadow, dectyp) =
  match lhs, rhs with
  | [], _ | _, [] -> C.err ~ctx "empty assignment"
  | _, _ :: _ :: _ -> parse_assign ctx ~prefix (lhs, [ C.ann ctx, Tuple rhs ], shadow, dectyp)
  | [ (_, (List l | Tuple l)) ], [ rhs ] | (_ :: _ :: _ as l), [ rhs ] ->
    let len = List.length lhs in
    let unpack_i = ref (-1) in
    let lst =
      List.mapi lhs ~f:(fun i expr ->
          match expr with
          | _, Unpack var when !unpack_i = -1 ->
            unpack_i := i;
            let slice =
              if i = len - 1
              then e_slice (e_int i)
              else e_slice (e_int i) ~b:(e_int (i + 1 - len))
            in
            s_assign ~shadow (e_id var) (e_index rhs [ slice ])
          | ann, Unpack var when !unpack_i > -1 ->
            C.err ~ctx "cannot have two tuple unpackings on LHS"
          | _ when !unpack_i = -1 ->
            let rhs = e_index rhs [ e_int i ] in
            s_assign ~shadow expr rhs
          | _ ->
            (* right of unpack: *c, b, a = x <=> a = x[-1]; b = x[-2] *)
            let rhs = e_index rhs [ e_int (i - len) ] in
            s_assign ~shadow expr rhs)
    in
    let len, op = if !unpack_i > -1 then len - 1, ">=" else len, "==" in
    let assert_stmt = s_assert (e_binary (e_call (e_dot rhs "__len__") []) op (e_int len)) in
    (assert_stmt :: lst)
    |> List.map ~f:(fun s -> parse ~ctx (C.sannotate (C.ann ctx) s))
    |> List.concat
    |> List.iter ~f:(Stack.push (Stack.top_exn ctx.env.statements));
    Pass ()
  | [ lhs ], [ rhs ] ->
    (* TODO: ensure that all shadowing and access rules are resolved here! *)
    let rhs = E.parse ~ctx:(C.enter_level ~ctx) rhs in
    (match snd lhs, dectyp with
    | Id var, _ ->
      let t = var_of_node_exn rhs in
      dectyp >>| E.parse ~ctx >>| var_of_node_exn >>| T.unify_inplace ~ctx t |> ignore;
      (match Ctx.in_scope ~ctx var with
      | Some (Var t' | Type t') when T.unify t t' <> -1 -> T.unify_inplace ~ctx t t'
      | Some (Type t') ->
        (* Util.dbg
          "shadowing %s : %s with %s"
          var
          (Ann.var_to_string t)
          (Ann.t_to_string (fst rhs).typ); *)
        Ctx.add ~ctx (prefix ^ var) (Type t)
      | Some (Var t') ->
        (* Util.dbg
          "shadowing %s : %s with %s"
          var
          (Ann.var_to_string t)
          (Ann.t_to_string (fst rhs).typ); *)
        Ctx.add ~ctx (prefix ^ var) (Var t)
      | None ->
        Ctx.add
          ~ctx
          (prefix ^ var)
          (match (fst rhs).typ with
          | Some (Type t) -> Type t
          | _ -> Var t)
      | _ -> ierr ~ctx "[parse_assign] invalid assigment");
      let ann = { (fst lhs) with typ = (fst rhs).typ } in
      Assign ([ e_id ~ann var ], [ rhs ], shadow, None)
    | Dot d, None ->
      (* a.x = b *)
      let lhs = E.parse ~ctx lhs in
      T.unify_inplace ~ctx (var_of_node_exn lhs) (var_of_node_exn rhs);
      Assign ([ lhs ], [ rhs ], shadow, None)
    | Index (var_expr, [ idx ]), None ->
      let var_expr = E.parse ~ctx var_expr in
      Expr (C.magic_call E.parse ~ctx ~magic:"__setitem__" var_expr ~args:[ idx; rhs ])
    | _ -> C.err ~ctx "invalid assign statement")

and parse_declare ?(prefix = "") ctx { name; typ } =
  let e =
    match typ with
    | Some typ -> E.parse ~ctx typ
    | None -> C.err ~ctx "invalid declare"
  in
  (* if is_some @@ Ctx.in_scope ~ctx name then Util.dbg "shadowing %s" name; *)
  Ctx.add ~ctx (prefix ^ name) (Var (var_of_node_exn e));
  Declare { name; typ = Some e }

and parse_alias ctx (name, expr') =
  let expr = E.parse ~ctx expr' in
  (* if is_some @@ Ctx.in_scope ~ctx name then Util.dbg "shadowing %s" name; *)
  Ctx.add ~ctx name (Var (var_of_node_exn expr));
  TypeAlias (name, expr)

and parse_del ctx expr =
  match snd expr with
  | Expr.Index (lhs, [ index ]) ->
    Expr (C.magic_call E.parse ~ctx ~magic:"__delitem__" lhs ~args:[ index ])
  | Id var ->
    Ctx.remove ~ctx var;
    Del (E.parse ~ctx expr)
  | _ -> C.err ~ctx "invalid del statement"

and parse_print ctx (exprs, term) =
  let stmts =
    List.map exprs ~f:(fun expr ->
      [ s_print (C.magic_call E.parse ~ctx ~magic:"__str__" expr)
      ; s_print (e_string " ") ])
    |> List.concat
    |> (fun l -> match l with [] -> [] | l -> List.tl_exn (List.rev l))
    |> (fun l -> List.rev (match term with "" -> l | t -> (s_print (e_string t)) :: l))
  in
  match stmts with
  | [ stmt ] ->
    snd stmt
  | stmts ->
    List.map stmts ~f:(fun s -> ctx.globals.sparse ~ctx @@ C.sannotate (C.ann ctx) s)
      |> List.concat
      |> List.iter ~f:(Stack.push (Stack.top_exn ctx.env.statements));
    Pass ()

and parse_return ctx expr =
  let t, expr =
    match expr with
    | None -> R.internal ~ctx "void", None
    | Some expr ->
      let expr = E.parse ~ctx expr in
      var_of_node_exn expr, Some expr
  in
  (match !(ctx.env.enclosing_return) with
  | None -> ctx.env.enclosing_return := Some t
  | Some rt -> T.unify_inplace ~ctx rt t);
  Return expr

and parse_yield ctx expr =
  let t, expr =
    match expr with
    | None -> R.internal ~ctx ~args:[ R.internal ~ctx "void" ] "generator", None
    | Some expr ->
      let expr = E.parse ~ctx expr in
      R.internal ~ctx ~args:[ var_of_node_exn expr ] "generator", Some expr
  in
  (match !(ctx.env.enclosing_return) with
  | None -> ctx.env.enclosing_return := Some t
  | Some rt -> T.unify_inplace ~ctx rt t);
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
    match Ann.real_type (var_of_node_exn gen_expr) with
    | Class ({ cache = "generator", _; generics = [ (_, (_, t)) ]; _ }, _) -> t
    | Link { contents = Unbound _ } -> C.make_unbound ctx
    | _ ->
      C.err ~ctx "for expression must be an iterable generator %s"
      @@ Ann.t_to_string (fst gen_expr).typ
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
            C.sannotate (C.ann ctx) @@ s_assign (e_id v) (e_index (e_id for_var) [ e_int idx ]))
      )
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
              let p = parse_pattern ctx (var_of_node_exn what) pat in
              Ctx.clear_block ~ctx;
              Ctx.add ~ctx name (Var (var_of_node_exn what));
              p
            | _ as pat ->
              Ctx.add_block ~ctx;
              let p = parse_pattern ctx (var_of_node_exn what) pat in
              Ctx.clear_block ~ctx;
              p
          in
          T.unify_inplace ~ctx (var_of_node_exn what) pat_t;
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
    Tuple (List.map l ~f:fst), TuplePattern (List.map l ~f:snd)

and parse_import (ctx : C.t) ?(ext = ".seq") imports =
  let import (ctx : C.t) file Stmt.{ from; what; import_as; _ } =
    let vtable =
      match Hashtbl.find ctx.globals.imports file with
      | Some t -> t
      | None ->
        let ictx = C.init_empty ~ctx in
        if file = ctx.env.filename then C.err ~ctx "recursive import";
        let statements =
          Codegen.parse_file file
          |> List.map ~f:(parse ~ctx:ictx)
          |> List.concat
        in
        Hashtbl.set C.imports ~key:file ~data:statements;
        Hashtbl.set ctx.globals.imports ~key:file ~data:ictx.map;
        ictx.map
    in
    let additions =
      match what with
      | None ->
        (* import <from> (as <import as>) *)
        let name = Option.value import_as ~default:from in
        [ name, Ann.Import file ]
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
    List.iter additions ~f:(fun (key, data) -> Ctx.add ~ctx key data);
    { from; file = Some file; what; import_as = Some (Option.value import_as ~default:from) }
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
      Stack.push (Stack.top_exn ctx.env.statements) (C.ann ctx, Import [import ctx file i]));
  Pass ()

and parse_impaste ctx ?(ext = ".seq") from =
  match Util.get_from_stdlib ~ext from with
  | Some file ->
    let statements =
      Codegen.parse_file file
      |> List.map ~f:(ctx.globals.sparse ~ctx)
      |> List.concat
    in
    Hashtbl.set C.imports ~key:file ~data:statements;
    ImportPaste file
  | None -> C.err ~ctx "cannot locate module %s" from

and parse_try ctx (stmts, catches, finally) =
  let stmts = parse_block ctx stmts in
  let catches =
    List.map catches ~f:(fun { exc; var; stmts } ->
        Ctx.add_block ~ctx;
        let exc =
          match exc, var with
          | Some exc, Some var ->
            let exc = E.parse ~ctx exc in
            Ctx.add ~ctx var (Var (var_of_node_exn exc));
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
    List.mapi args ~f:(fun i arg ->
        match arg.typ with
        | Some typ ->
          let typ = E.parse ~ctx typ in
          let name = if arg.name = "" then sprintf "a%d" i else arg.name in
          { name; typ = Some typ }, (name, var_of_node_exn typ)
        | None -> C.err ~ctx "extern functions must explicitly state parameter types")
  in
  let fret =
    match fn.typ with
    | Some typ -> E.parse ~ctx typ
    | None -> C.err ~ctx "extern functions must explicitly state return type"
  in
  let tf' =
    Ann.(
      ( { generics = []
        ; name = prefix ^ fn.name
        ; parent = ctx.env.enclosing_name, None
        ; args = List.map args ~f:snd
        ; cache = prefix ^ fn.name, (C.ann ctx).pos
        }
      , { ret = var_of_node_exn fret; used = String.Hash_set.create () } ))
  in
  let typ = Ann.(Link (ref (Bound (Func tf')))) in
  Ctx.add ~ctx (prefix ^ fn.name) (Var typ);
  let fn = { fn with typ = Some fret } in
  let node = C.ann ~typ:(Var typ) ctx, Extern (lang, dylib, ctxn, fn, List.map ~f:fst args) in
  Hashtbl.set C.realizations ~key:(fst tf').cache ~data:(node, String.Table.create ());
  ignore @@ R.realize ~ctx ~force:true typ;
  snd node

and parse_function ctx ?(prefix = "") ?(shalow = false) f =
  let ctx = C.enter_level ~ctx in
  Ctx.add_block ~ctx;
  let explicits =
    List.map f.fn_generics ~f:(fun gen ->
        let typ = C.make_unbound ~is_generic:true ctx in
        let id = !(ctx.globals.unbound_counter) in
        Ctx.add ~ctx gen (Type typ);
        gen, (id, typ))
  in
  let fname = prefix ^ f.fn_name.name in
  let fret =
    Option.value_map f.fn_name.typ ~default:(C.make_unbound ctx) ~f:(fun t ->
        var_of_node_exn (E.parse ~ctx t))
  in
  let fn_name = { name = fname; typ = None } in
  let fn_args, args =
    List.unzip
    @@ List.mapi f.fn_args ~f:(fun i arg ->
            let arg_typ = Option.map arg.typ ~f:(fun t -> E.parse ~ctx t) in
            let typ =
              match arg_typ with
              | Some t -> Ann.real_type (var_of_node_exn t)
              | None when i = 0 && arg.name = "self" && prefix <> "" ->
                (match Ctx.in_scope ~ctx (String.drop_suffix prefix 1) with
                | Some (Type t) -> t
                | _ -> C.err ~ctx "cannot locate %s (self)" (String.drop_suffix prefix 1))
              | None -> C.make_unbound ctx
            in
            Ctx.add ~ctx arg.name (Var typ);
            { name = arg.name; typ = arg_typ }, (arg.name, typ))
  in
  (* Add self-refential type for recursive calls *)
  let tf' =
    Ann.(
      ( { generics =
            List.map explicits ~f:(fun (n, (id, t)) ->
                n, (id, T.generalize ~level:(ctx.env.level - 1) t))
        ; name = fname
        ; parent = ctx.env.enclosing_name, None
        ; args = List.map args ~f:(fun (n, t) -> n, T.generalize ~level:(ctx.env.level - 1) t)
        ; cache = fname, (C.ann ctx).pos
        }
      , { ret = T.generalize ~level:(ctx.env.level - 1) fret; used = String.Hash_set.create () }
      ))
  in
  let tfun = Ann.(Link (ref (Bound (Func tf')))) in
  let fn_stmts =
    if shalow
    then f.fn_stmts
    else (
      let ctx =
        { ctx with
          env =
            { ctx.env with
              realizing = false
            ; enclosing_name = (fst tf').cache
            ; enclosing_return = ref None
            }
        }
      in
      Ctx.add ~ctx fname (Var tfun);
      let fn_stmts = List.concat @@ List.map f.fn_stmts ~f:(parse ~ctx) in
      let ret' =
        match !(ctx.env.C.enclosing_return) with
        | None -> R.internal ~ctx "void"
        | Some typ -> typ
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
    Ann.Func
      ( { (fst tf') with generics = explicits; args }
      , { ret = fret; used = String.Hash_set.create () } )
  in
  (* all functions must be as general as possible: generic unification is not allowed *)
  let tfun = T.generalize ~level:0 tfun in
  Ctx.add ~ctx fname (Var tfun);
  let node = C.ann ctx ~typ:(Var tfun), Function { f with fn_name; fn_args; fn_stmts } in
  Hashtbl.set C.realizations ~key:(fst tf').cache ~data:(node, String.Table.create ());
  (* Util.dbg "|| [fun] %s |- %s %b" fname (Ann.var_to_string tfun) (Ann.is_realizable tfun); *)
  let tfun = R.realize ~ctx tfun in
  snd node

(* and prn_class ?(generics = Int.Table.create ()) lev (name, typ) =
  Util.dbg "|| %s[typ] %s |- %s" (String.make (2 * lev) ' ') name
  @@ Ann.var_to_string ~generics typ *)
and parse_extend (ctx : C.t) (name, new_members) =
  let class_name =
    match snd name with
    | Expr.Id name -> name
    | _ -> C.err ~ctx "bad class name"
  in
  let cls, tcls, typ =
    let t =
      Ctx.in_scope ~ctx class_name
      >>= (function
            | Type t -> Some t
            | z -> None)
      >>| Ann.real_type
    in
    match t with
    | Some (Class (c, _)) -> fst @@ Hashtbl.find_exn C.realizations c.cache, c, t
    | Some z -> C.err ~ctx "%s is not a class" class_name
    | None -> C.err ~ctx "%s is not a class" class_name
  in
  Ctx.add_block ~ctx;
  let ctx = C.enter_level ~ctx in
  let explicits =
    List.map tcls.generics ~f:(fun (gen, (id, t)) ->
        if not (Ann.has_unbound t) then ierr "unbound explicit";
        let t = C.make_unbound ~id ctx in
        Ctx.add ~ctx gen (Type t);
        t)
  in
  let ctx =
    { ctx with
      env =
        { ctx.env with
          enclosing_name = tcls.cache
        ; enclosing_type = Some tcls.cache
        ; realizing = false
        }
    }
  in
  let members = Hashtbl.find_exn C.classes tcls.cache in
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
  List.iter tcls.generics ~f:(fun (g, _) -> Ctx.remove ~ctx g);
  List.iter explicits ~f:(T.generalize_inplace ~ctx);
  Extend (name, new_members)

and parse_class ctx ?(prefix = "") ?member_of ?(istype = false) cls =
  let { class_name; generics; members; _ } = cls in
  let cache = prefix ^ class_name, (C.ann ctx).pos in
  (* Ctx.push ctx; *)
  let ctx = C.enter_level ~ctx in
  let old_explicits = List.map generics ~f:(Ctx.in_scope ~ctx) in
  let explicits =
    List.map generics ~f:(fun gen ->
        let t = C.make_unbound ~is_generic:true ctx in
        let id = !(ctx.globals.unbound_counter) in
        (* Util.A.dr "[class %s] generic %s -> %s // %d"
          class_name gen (Ann.var_to_string ~full:true t) id; *)
        Ctx.add ~ctx gen (Type t);
        gen, (id, t))
  in
  let class_name = prefix ^ class_name in
  let args =
    if istype then
    List.filter_map members ~f:(function
        | _, Declare { name; typ = Some typ } ->
          Some (name, var_of_node_exn (E.parse ~ctx typ))
        | _ -> None)
    else []
  in
  let member_table = String.Table.create () in
  let tc' =
    Ann.(
      ( { generics =
            List.map explicits ~f:(fun (n, (id, t)) ->
                n, (id, T.generalize ~level:(pred ctx.env.level) t))
            (* seal for later use; dependents will still use same-numbered unbound while
          dependend recursive realization will reinstantiate it as another number *)
        ; name = class_name
        ; parent = ctx.env.enclosing_name, None
        ; cache
        ; args
        }
      , { is_type = istype } ))
  in
  let tcls = Ann.(Link (ref (Bound (Class tc')))) in
  let ctx =
    { ctx with
      env =
        { ctx.env with enclosing_name = cache; enclosing_type = Some cache; realizing = false }
    }
  in
  Ctx.add ~ctx class_name (Type tcls);

  Hashtbl.set C.classes ~key:cache ~data:member_table;
  (match member_of with
  | Some m -> Hashtbl.set m ~key:cls.class_name ~data:[Ann.Type tcls];
  | None -> ());

  let class_members = List.filter_map members ~f:(function
    | _, Declare { name; typ = Some typ } ->
      Some (sprintf "%s: %s" name (Expr.to_string typ))
    | _ -> None)
  in
  let inits = if (snd cache) = Ann.default_pos then [] else (
    let cls = sprintf"%s%s"
      class_name
      (match generics with [] -> "" | g -> sprintf "[%s]" @@ Util.ppl generics ~f:Fn.id)
    in
    if istype
    then
      [ sprintf "cdef __init__(self: %s) -> %s" cls cls
      ; sprintf "cdef __str__(self: %s) -> str" cls
      (* ; sprintf "cdef __iter__(self: %s) -> generator[]" cls *)
      ; sprintf "cdef __len__(self: %s) -> int" cls
      ; sprintf "cdef __eq__(self: %s, o: %s) -> bool" cls cls
      ; sprintf "cdef __ne__(self: %s, o: %s) -> bool" cls cls
      ; sprintf "cdef __lt__(self: %s, o: %s) -> bool" cls cls
      ; sprintf "cdef __gt__(self: %s, o: %s) -> bool" cls cls
      ; sprintf "cdef __le__(self: %s, o: %s) -> bool" cls cls
      ; sprintf "cdef __ge__(self: %s, o: %s) -> bool" cls cls
      ; sprintf "cdef __hash__(self: %s) -> int" cls
      (* ; sprintf "cdef __contains__(self: %s) -> bool" cls *)
      ; sprintf "cdef __pickle__(self: %s, dest: ptr[byte])" cls
      ; sprintf "cdef __unpickle__(self: %s, src: ptr[byte]) -> %s" cls cls
      ; match class_members with [] -> "" | members ->
        sprintf "cdef __init__(self: %s, %s) -> %s" cls (Util.ppl members ~f:Fn.id) cls
      ]
    else
      [ sprintf "cdef __init__(self: %s)" cls
      ; sprintf "cdef __bool__(self: %s) -> bool" cls
      ; sprintf "cdef __pickle__(self: %s, dest: ptr[byte])" cls
      ; sprintf "cdef __unpickle__(self: %s, src: ptr[byte]) -> %s" cls cls
      ; sprintf "cdef __raw__(self: %s) -> ptr[byte]" cls
      ; match class_members with [] -> "" | members ->
        sprintf "cdef __init__(self: %s, %s)" cls (Util.ppl members ~f:Fn.id)
      ])
    |> String.concat ~sep:"\n"
    (* |> (fun s -> Util.A.db "%s" s; s) *)
    |> Codegen.parse
    |> List.map ~f:(C.sannotate Ann.default)
  in
  let cls_members =
    List.map (inits @ members) ~f:(fun s ->
        let prefix = sprintf "%s." class_name in
        let typ, node, name =
          match snd s with
          | Function f ->
            let stmt = parse_function ctx ~prefix f in
            let typ = Ctx.in_block ~ctx (prefix ^ f.fn_name.name) in
            Hashtbl.add_multi member_table ~key:f.fn_name.name ~data:(Option.value_exn typ);
            typ, stmt, f.fn_name.name
          | Extern ((_, _, _, fn, _) as f) ->
            let stmt = parse_extern ctx ~prefix f in
            let typ = Ctx.in_block ~ctx (prefix ^ fn.name) in
            Hashtbl.add_multi member_table ~key:fn.name ~data:(Option.value_exn typ);
            typ, stmt, fn.name
          | Class c ->
            let stmt = parse_class ctx ~prefix ~member_of:member_table c in
            let typ = Ctx.in_block ~ctx (prefix ^ c.class_name) in
            typ, stmt, c.class_name
          | Declare ({ name; _ } as d) ->
            let stmt = parse_declare ctx ~prefix d in
            let typ = Ctx.in_block ~ctx (prefix ^ name) in
            let typ =
              match typ with
              | Some (Var t | Type t) -> t
              | _ -> failwith "err"
            in
            Hashtbl.set member_table ~key:name ~data:[ Var typ ];
            (* Util.A.dr "[class %s] param %s -> %s"
                class_name name (Ann.var_to_string ~full:true typ); *)
            Some (Var typ), stmt, name
          | _ -> ierr "invalid class member"
        in
        (* Util.A.db
          ">> %s :: adding %s = %s"
          class_name
          name
          (Ann.typ_to_string @@ Option.value_exn typ); *)
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
  let node = C.ann ~typ:(Type tcls) ctx, if istype then Type n else Class n in
  Hashtbl.set C.realizations ~key:cache ~data:(node, String.Table.create ());
  let tcls = R.realize ~ctx:{ ctx with env = { ctx.env with realizing = true } } tcls in
  (* prn_class 0 (cls.class_name, tcls); *)
  snd node

and parse_special ctx (name, exprs, _) = ierr "No specials here"
