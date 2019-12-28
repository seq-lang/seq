(* ****************************************************************************
 * Seqaml.Typecheck_expr: Typecheck Seq code from expression AST
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Err
open Option.Monad_infix

open Ast
open Ast.Expr

module C = Typecheck_ctx
module T = Typecheck_infer
module R = Typecheck_realize

(* ***************************************************************
    Public interface
    *************************************************************** *)

(** [parse ~ctx expr] dispatches an expression AST node [expr] to the proper code generation function. *)
let rec parse ~(ctx : C.t) ((ann, node) : Expr.t Ann.ann) : Expr.t Ann.ann =
  C.push_ann ~ctx ann;
  let expr : Expr.t Ann.ann =
    match node with
    | Empty _ -> parse_none ctx node
    | Bool _ -> parse_internal ctx node ~name:"bool"
    | Int p -> parse_int ctx p
    | Float p -> parse_float ctx p
    | String _ -> parse_internal ctx node ~name:"str"
    | Seq _ -> parse_internal ctx node ~name:"seq"
    | Kmer k -> parse_kmer ctx k
    | Id p -> parse_id ctx p
    | Tuple p -> parse_tuple ctx p
    | List p -> parse_collection ctx p ~kind:"list"
    | Set p -> parse_collection ctx p ~kind:"set"
    | Dict p -> parse_collection ctx p ~kind:"dict"
    | Generator p -> parse_generator ctx p
    | IfExpr p -> parse_if ctx p
    | Unary p -> parse_unary ctx p
    | Binary p -> parse_binary ctx p
    | Call p -> parse_call ctx p
    | Index p -> parse_index ctx p
    | Dot p -> parse_dot ctx p
    | TypeOf p -> parse_typeof ctx p
    | Lambda p -> parse_lambda ctx p
    | Pipe p -> parse_pipe ctx p
    | Ptr p -> parse_ptr ctx p
    | Ellipsis _ -> parse_ellipsis ctx node
    | Slice _ -> C.err ~ctx "slice is only valid within an index"
    | Unpack _ -> C.err ~ctx "unpack is not valid here"
  in
  ignore @@ C.pop_ann ~ctx;
  Util.A.dbg "[e] %s [%s] -> %s [%s]"
    (Expr.to_string (ann,node))
    (Ann.to_string ann)
    (Expr.to_string expr)
    (Ann.to_string (fst expr));
  expr

and parse_none ctx n =
  let typ = C.make_unbound ctx in
  C.ann ~typ:(Var typ) ctx, n

and parse_ellipsis ctx n =
  (* Partial calls might add "..." to the context *)
  let typ =
    match Ctx.in_scope ~ctx "..." with
    | Some (Var _ as t) -> t
    | Some _ -> C.err ~ctx "wrong ellipsis type"
    | None -> Var (C.make_unbound ctx)
  in
  C.ann ~typ ctx, n

and parse_internal ~name ctx n =
  let typ = R.internal ~ctx name in
  C.ann ~typ:(Var typ) ctx, n

and parse_int ctx (i, kind) =
  match kind, Caml.Int64.of_string_opt i with
  | ("u" | "U"), t ->
    (match t, Caml.Int64.of_string_opt ("0u" ^ i) with
    | Some t, _ ->
      parse ~ctx
      @@ C.eannotate ~ctx
      @@ e_call (e_id "u64") [C.ann ctx, Int ("0u" ^ i, "")]
    | None, Some t ->
      parse ~ctx
      @@ C.eannotate ~ctx
      @@ e_call (e_id "u64") [C.ann ctx, Int ("0u" ^ i, "")]
    | None, None -> C.err ~ctx "cannot parse %s as unsigned" i)
  | _, Some _ -> C.ann ~typ:(Var (R.internal ~ctx "int")) ctx, Int (i, kind)
  | _ -> C.err ~ctx "integer %s too large" i

and parse_float ctx (i, kind) =
  C.ann ~typ:(Var (R.internal ~ctx "float")) ctx, Float (i, kind)

and parse_kmer ctx s =
  parse ~ctx
  @@ C.eannotate ~ctx
  @@ e_call (e_index (e_id "Kmer") [e_int (String.length s)]) [e_string s]

and parse_id ctx name =
  let typ =
    match Ctx.in_scope ~ctx name with
    | Some (Type t) -> Ann.Type (R.realize ~ctx (T.instantiate ~ctx t))
    | Some (Var t) -> Var (R.realize ~ctx (T.instantiate ~ctx t))
    | Some (Import s) -> Import s
    (* Ann.var_of_typ_exn (C.ann ctx).typ *)
    | _ -> C.err ~ctx "%s not found" name
  in
  C.ann ~typ ctx, Id name

and parse_tuple ctx args =
  let args = List.map args ~f:(parse ~ctx) in
  let typ = Ann.Tuple (List.map args ~f:(fun (t, _) -> Ann.var_of_typ_exn t.typ)) in
  C.ann ~typ:(Var typ) ctx, Tuple args

and parse_collection ~kind ctx items =
  let append, init_args =
    match kind with
    | "list" -> "append", [e_int (List.length items)]
    | "set" -> "add", []
    | "dict" -> "__setitem__", []
    | k -> ierr "[parse_comprehension] unknown kind %s" k
  in
  let temp_var = C.make_temp ctx ~prefix:"col" in
  let s_init = s_assign (e_id temp_var) (e_call (e_id kind) init_args) in
  let s_set =
    match kind with
    | "dict" ->
      List.chunks_of items ~length:2
      |> List.map ~f:(fun args -> s_expr (e_call (e_dot (e_id temp_var) append) args))
    | _ ->
      List.map items ~f:(fun e -> s_expr (e_call (e_dot (e_id temp_var) append) [e]))
  in
  let stmts = s_init :: s_set in
  List.map stmts ~f:(fun s -> ctx.globals.sparse ~ctx @@ C.sannotate (C.ann ctx) s)
  |> List.concat
  |> List.iter ~f:(Stack.push (Stack.top_exn ctx.env.statements));
  parse_id ctx temp_var

and parse_generator ctx (kind, exprs, comp) =
  let append, init_args =
    match kind with
    | "list" -> "append", [] (** TODO: do initialize *)
    | "set" -> "add", []
    | "dict" -> "__setitem__", []
    | k -> ierr "[parse_comprehension] unknown kind %s" k
  in
  let temp_var = C.make_temp ctx ~prefix:"comp" in
  let rec s_generator = function
    | None -> s_expr (e_call (e_dot (e_id temp_var) append) exprs)
    | Some { var; gen; cond; next } ->
      let stmt = s_generator next in
      let stmt = Option.value_map cond ~default:stmt ~f:(fun c -> s_if c [ stmt ]) in
      s_for var gen [ stmt ]
  in
  let stmts = [ s_assign (e_id temp_var) (e_call (e_id kind) init_args); s_generator (Some comp) ] in
  List.map stmts ~f:(fun s -> ctx.globals.sparse ~ctx @@ C.sannotate (C.ann ctx) s)
  |> List.concat
  |> List.iter ~f:(Stack.push (Stack.top_exn ctx.env.statements));
  parse_id ctx temp_var

and parse_if ctx (cond, if_expr, else_expr) =
  let cond = C.magic_call parse ~ctx ~magic:"__bool__" cond in
  let if_expr = parse ~ctx if_expr in
  let else_expr = parse ~ctx else_expr in
  T.unify_inplace ~ctx (var_of_node_exn if_expr) (var_of_node_exn else_expr);
  C.ann ~typ:(Var (var_of_node_exn if_expr)) ctx, IfExpr (cond, if_expr, else_expr)

and parse_unary ctx (op, expr) =
  match op with
  | "!" ->
    let expr = C.magic_call parse ~ctx ~magic:"__bool__" expr in
    C.ann ~typ:(Var (var_of_node_exn expr)) ctx, Unary (op, expr)
  | _ -> C.magic_call parse ~ctx ~magic:(Util.uop2magic op) expr

and parse_binary ctx (lh, bop, rh) =
  match bop with
  | "&&" | "||" ->
    (* these have no magics *)
    let lh = C.magic_call parse ~ctx ~magic:"__bool__" lh in
    let rh = C.magic_call parse ~ctx ~magic:"__bool__" rh in
    C.ann ~typ:(Var (var_of_node_exn rh)) ctx, Binary (lh, bop, rh)
  | bop when String.prefix bop 8 = "inplace_" ->
    ierr ~ctx "inplace should be handled at assignment time"
  | bop ->
    let magic = Util.bop2magic bop in
    let lh = parse ~ctx lh in
    let rh = parse ~ctx rh in
    let typ = R.magic ~ctx ~args:[ var_of_node_exn rh ] (var_of_node_exn lh) magic in
    let typ =
      match typ with
      | Some t -> t
      | None ->
        let rmagic = Util.bop2magic ~prefix:"r" bop in
        let typ = R.magic ~ctx ~args:[ var_of_node_exn lh ] (var_of_node_exn rh) rmagic in
        (match typ, ctx.env.realizing with
        | Some t, _ -> t
        | None, false -> C.make_unbound ctx
        | None, _ ->
          C.err
            ~ctx
            "cannot locate appropriate %s or %s for %s and %s"
            magic
            rmagic
            (Ann.var_to_string @@ var_of_node_exn lh)
            (Ann.var_to_string @@ var_of_node_exn rh))
    in
    C.ann ~typ:(Var typ) ctx, Binary (lh, bop, rh)

and parse_call ctx (expr, args) =
  match snd expr, List.map args ~f:(fun x -> x.value) with
  | Dot (e, "__len__"), [] -> (* Won't catch partial application though... *)
    let e = parse ~ctx e in
    (match Ann.real_type (var_of_node_exn e) with
    | Tuple t -> parse_int ctx (string_of_int @@ List.length t, "")
    | _ -> parse_call_real ctx (parse ~ctx expr, args))
  | _, [a, Generator (("list" | "tuple"), _, { gen; cond = None; next = None; _ } as g)] ->
    let expr = parse ~ctx expr in
    (match Ann.real_type (var_of_node_exn expr) with
    | Func (f, _) when (fst f.cache = "join") && (fst (fst f.parent) = "str") ->
      (* TODO: won't catch shadows and extensions! *)
      parse ~ctx
      @@ C.eannotate ~ctx
      @@ e_call (e_dot (e_id "str") "cati_ext") [ a, Generator g ]
    | _ -> parse_call_real ctx (expr, args))
  | _ -> parse_call_real ctx (parse ~ctx expr, args)

and parse_call_real ctx (expr, args) =
  let is_partial { value; _ } =
    match value with
    | _, Expr.Ellipsis _ -> true
    | _ -> false
  in
  (* ASSUME expr IS PARSED *)
  (* let expr = parse ~ctx expr in *)
  let typ = Ann.real_type (var_of_node_exn expr) |> T.instantiate ~ctx in
  (match typ with
  | Func (g, f) ->
    let arg_table = String.Table.create () in
    let def_args =
      Array.of_list
      @@ List.filter_mapi g.args ~f:(fun i a ->
              (* Util.A.db "->> defarg %s" @@ (Ann.var_to_string (snd a)); *)
              Hashtbl.set arg_table ~key:(fst a) ~data:(i, snd a);
              if Hash_set.exists f.used ~f:(( = ) (fst a)) then None else Some a)
    in
    let has_partials = List.exists args ~f:is_partial in
    let args = List.map args ~f:(fun arg -> { arg with value = parse ~ctx arg.value }) in
    let in_args =
      if List.length args <> Array.length def_args
      then
        if List.length args = Array.length def_args + 1 && is_partial (List.last_exn args)
        then List.rev args |> List.tl_exn |> List.rev
        else
          C.err
            ~ctx
            "function argument length mismatch %d vs %d"
            (List.length args)
            (Array.length def_args)
      else args
    in
    (* Check ordering *)
    let names_started = ref false in
    let used = Hash_set.copy f.used in
    List.iteri in_args ~f:(fun i arg ->
        let name =
          match arg.name, !names_started with
          | None, false -> fst def_args.(i)
          | None, true -> C.err ~ctx "named arguments must be followed by named arguments"
          | Some name, false ->
            names_started := true;
            name
          | Some name, true -> name
        in
        match Hashtbl.find arg_table name, is_partial arg with
        | Some (_, typ'), true ->
          let typ = C.make_unbound ctx in
          T.unify_inplace ~ctx typ' typ
        | Some (i, typ'), false ->
          Hash_set.add used name;
          T.unify_inplace ~ctx (var_of_node_exn arg.value) typ';
          Hashtbl.set arg_table ~key:name ~data:(i, var_of_node_exn arg.value)
        | _ -> C.err ~ctx "argument %s does not exist" name);
    if has_partials
    then (
      let new_args =
        Hashtbl.to_alist arg_table
        |> List.sort ~compare:(fun (_, (i, _)) (_, (j, _)) -> Int.compare i j)
        |> List.map ~f:(fun (n, (_, v)) -> n, v)
      in
      let typ =
        T.generalize ~level:ctx.env.level
        @@ Ann.(Func ({ g with args = new_args }, { f with used }))
      in
      C.ann ~typ:(Var typ) ctx, Call (expr, args))
    else (
      (* TODO: reorder args *)
      let typ = R.realize ~ctx ~force:true typ in
      let expr = Ann.patch (fst expr) ~f:(fun _ -> typ), snd expr in
      C.ann ~typ:(Var f.ret) ctx, Call (expr, args))
  | Class (_, { is_type }) ->
    let args = List.map args ~f:(fun p -> { p with value = parse ~ctx p.value }) in
    let arg_typs = List.map args ~f:(fun p -> var_of_node_exn p.value) in
    (match R.magic ~ctx typ "__init__" ~args:arg_typs with
    | None when ctx.env.realizing ->
      C.err
        ~ctx
        "cannot locate %s.__init__(%s)"
        (Ann.var_to_string typ)
        (Util.ppl arg_typs ~f:Ann.var_to_string)
    | Some (Func (_, { ret; _ })) when is_type -> T.unify_inplace ~ctx ret typ
    | _ -> ());
    C.ann ~typ:(Var typ) ctx, Call (expr, args)
  | Link { contents = Unbound _ } ->
    let args = List.map args ~f:(fun p -> { p with value = parse ~ctx p.value }) in
    let typ = C.make_unbound ctx in
    C.ann ~typ:(Var typ) ctx, Call (expr, args)
  | _ -> C.err ~ctx "wrong call type %s" (Ann.var_to_string typ))


and parse_index ctx (lh_expr', indices') =
  match indices' with
  | [ (_, Slice (Some a, Some b, None)) ] ->
    parse ~ctx @@ C.epatch ~ctx (e_call (e_dot lh_expr' "__slice__") [ a; b ])
  | [ (_, Slice (None, Some b, None)) ] ->
    parse ~ctx @@ C.epatch ~ctx (e_call (e_dot lh_expr' "__slice_left__") [ b ])
  | [ (_, Slice (Some a, None, None)) ] ->
    parse ~ctx @@ C.epatch ~ctx (e_call (e_dot lh_expr' "__slice_right__") [ a ])
  | _ ->
    let indices = List.map indices' ~f:(parse ~ctx) in
    let has_types = List.for_all indices ~f:(fun (x, _) -> Ann.is_type x) in
    let all_types = List.exists indices ~f:(fun (x, _) -> Ann.is_type x) in
    if has_types && not all_types then C.err ~ctx "passed types where variables are expected";
    (match all_types, snd lh_expr' with
    | true, (Id "tuple" as lh) ->
      { (C.ann ctx) with typ = Some (Type (Tuple (List.map indices ~f:var_of_node_exn))) }, lh
    | false, (Id "tuple" as lh) -> C.err ~ctx "tuple needs types"
    (* TODO: Id function *)
    | true, _ ->
      let lh_expr = parse ~ctx lh_expr' in
      let typ = Ann.real_type (var_of_node_exn lh_expr) in
      (* Type instantiation case *)
      (match typ with
      | Class (g, _) | Func (g, _) ->
        if List.(length g.generics <> length indices)
        then C.err ~ctx "Generic length does not match";
        List.iter2_exn g.generics indices ~f:(fun (_, (_, t)) (i, _) ->
            T.unify_inplace ~ctx t (Ann.var_of_typ_exn i.typ));
        let typ = R.realize ~ctx typ in
        if not ctx.env.realizing
        then C.ann ~typ:(Type typ) ctx, Index (lh_expr, indices)
        else (* remove type info: e.g. T[t,u] -> T *)
          C.ann ~typ:(Type typ) ctx, snd lh_expr
      (* Recursive class instantiation (e.g. class A[T]: y:A[T]) *)
      | Link { contents = Unbound _ } as t when not ctx.env.realizing ->
        let typ = C.make_unbound ctx in
        C.ann ~typ:(Type typ) ctx, Index (lh_expr, indices)
      | _ ->
        (* C.dump_ctx ctx; *)
        C.err ~ctx "cannot instantiate non-generic %s" @@ Ann.t_to_string (fst lh_expr).typ)
    (* Member access case *)
    | false, _ ->
      let lh_expr = parse ~ctx lh_expr' in
      let getitem_ast = C.epatch ~ctx
        @@ e_call
              (e_dot lh_expr' "__getitem__")
              [ (match indices' with
                | [ a ] -> a
                | a -> e_tuple a)
              ]
      in
      let lt = Ann.real_type (var_of_node_exn lh_expr) in
      (match lt, indices' with
      | Class ({ args = ts; cache; _ }, { is_type = true }), [_, Int i] ->
        let h = Hashtbl.find_exn ctx.globals.classes cache in
        (match Hashtbl.find h "__getitem__" with
        | Some _ -> parse ~ctx getitem_ast
        | None ->
          let typ = Caml.Int64.of_string_opt (fst i) >>= Int.of_int64 >>= List.nth ts in
          match typ with
          | Some (_, typ) -> C.ann ~typ:(Var typ) ctx, Index (lh_expr, indices)
          | None -> C.err ~ctx "invalid tuple index")
      | Tuple ts, [_, Int i] ->
        (* tuple static access typecheck *)
        let typ = Caml.Int64.of_string_opt (fst i) >>= Int.of_int64 >>= List.nth ts in
        (match typ with
        | Some typ -> C.ann ~typ:(Var typ) ctx, Index (lh_expr, indices)
        | None -> C.err ~ctx "invalid tuple index")
      | Tuple _, _ -> C.err ~ctx "tuple access requires a valid compile-time integer constant"
      | _, _ -> parse ~ctx getitem_ast))

and parse_dot ctx (lh_expr', rhs) =
  let lh_expr = parse ~ctx lh_expr' in
  let resolved =
    match (fst lh_expr).typ with
    | None -> ierr ~ctx "cannot happen"
    | Some (Import name) ->
      (* lt,  *)
      Hashtbl.find ctx.globals.imports name
      >>= fun dict -> Hashtbl.find dict rhs >>= List.hd
      (* >>= fun (_, dict) -> Hashtbl.find dict rhs *)
    | Some (Type lt | Var lt) ->
      (match Ann.real_type lt with
      | Class (g, _) ->
        Hashtbl.find ctx.globals.classes g.cache
        >>= fun dict -> Hashtbl.find dict rhs >>= List.hd
      | _ -> None)
  in
  match (fst lh_expr).typ, Ann.real_t resolved with
  (* Nested class access--- X.Y or obj.Y *)
  | Some (Type lt | Var lt), Some (Type t) ->
    let typ = T.link_to_parent ~parent:(Some lt) (T.instantiate ~ctx ~parent:lt t) in
    C.ann ~typ:(Type typ) ctx, Dot (lh_expr, rhs)
  (* Nested function access--- X.fn *)
  | Some (Type lt), Some (Var (Func (g, _) as t)) ->
    let typ = T.link_to_parent ~parent:(Some lt) (T.instantiate ~ctx ~parent:lt t) in
    C.ann ~typ:(Var typ) ctx, Dot (lh_expr, rhs)
  (* Object method access--- obj.fn *)
  | Some (Var lt), Some (Var ((Func (tg, tf)) as t)) ->
    let ast, _ = Hashtbl.find_exn C.realizations tg.cache in
    let typ = T.link_to_parent ~parent:(Some lt) (T.instantiate ~ctx ~parent:lt t) in
    (match lt, typ with
    | (Class (lg, _) | Func (lg, _)),
      Func ({ args = (hd_n, hd_t) :: tl; _ } as g, f) ->
      T.unify_inplace ~ctx (T.instantiate ~ctx hd_t) lt;
      let hd_t = T.link_to_parent ~parent:(snd lg.parent) hd_t in
      let used = Hash_set.copy f.used in
      Hash_set.add used hd_n;
      let typ = Ann.Func ({ g with args = (hd_n, hd_t) :: tl }, { f with used }) in
      (match ast with
      | _, Function { fn_attrs; _ } when List.exists fn_attrs ~f:((=) "property") ->
        (* Util.A.db "property! %s" rhs; *)
        C.ann ~typ:(Var f.ret) ctx, Call ((C.ann ~typ:(Var typ) ctx, Dot (lh_expr, rhs)), [])
      | _ ->
        C.ann ~typ:(Var typ) ctx, Dot (lh_expr, rhs))
    | _ -> ierr "cannot happen")
  (* Nested member access: X.member *)
  | Some (Var lt), Some (Var t) ->
    (* What if there are NO proper parents? *)
    let typ = T.link_to_parent ~parent:(Some lt) (T.instantiate ~ctx ~parent:lt t) in
    C.ann ~typ:(Var typ) ctx, Dot (lh_expr, rhs)

  (* Imported symbol *)
  | Some (Import _), Some (Import _ as lt) ->
    C.ann ~typ:lt ctx, Dot (lh_expr, rhs)
  | Some (Import _), Some (Type lt) ->
    C.ann ~typ:(Type (T.instantiate ~ctx lt)) ctx, Dot (lh_expr, rhs)
  | Some (Import _), Some (Var lt) ->
    C.ann ~typ:(Var (T.instantiate ~ctx lt)) ctx, Dot (lh_expr, rhs)
  | Some (Type lt | Var lt), None ->
    let typ =
      match lt with
      | Link { contents = Unbound _ } -> C.make_unbound ctx
      | Class ({ cache; _ }, _) when Some cache = ctx.env.enclosing_type -> C.make_unbound ctx
      | t -> C.err ~ctx "cannot find %s in %s" rhs (Ann.var_to_string lt)
    in
    C.ann ~typ:(Var typ) ctx, Dot (lh_expr, rhs)
  | _ -> C.err ~ctx "member access requires object"

and parse_typeof ctx expr =
  let expr = parse ~ctx expr in
  let typ =
    match var_of_node expr with
    | Some t -> t
    | _ -> C.err ~ctx "typeof does not support imports"
  in
  C.ann ~typ:(Type typ) ctx, TypeOf expr

and parse_ptr ctx expr =
  match snd expr with
  | Id var ->
    (* TODO: scoping rules? can access globals? *)
    (match Ctx.in_scope ~ctx var with
    | Some (Var v) ->
      let typ = R.internal ~ctx "ptr" ~args:[v] in
      C.ann ~typ:(Var typ) ctx, Ptr expr
    | _ -> C.err ~ctx "symbol %s not found" var)
  | _ -> C.err ~ctx "ptr requires an identifier as a parameter"

and parse_pipe ctx exprs =
  match exprs with
  | [] | [ _ ] -> ierr "empty pipe construct"
  | (_, hd) :: tl ->
    let hd = parse ~ctx hd in
    let (last_expr, is_gen), exprs =
      List.fold_map tl ~init:(hd, false) ~f:(fun (pt, g) (op, expr) ->
          let pt = Ann.real_type (var_of_node_exn pt) in
          let missing_type, g =
            match pt with
            | Class ({ cache = "generator", _; generics = [ (_, (_, t)) ]; _ }, _) -> t, true
            | _ -> pt, g
          in
          Ctx.add_block ~ctx;
          Ctx.add ~ctx "..." (Var missing_type);
          let has_ellipsis { value = _, x; _ } =
            match x with
            | Ellipsis _ -> true
            | _ -> false
          in
          let expr =
            match expr with
            | _, Call (callee, args) when List.exists args ~f:has_ellipsis -> parse ~ctx expr
            | ann, Call (callee, args) ->
              let args = List.map args ~f:(fun { value; _ } -> value) in
              parse ~ctx (e_call ~ann callee @@ (e_ellipsis ~ann () :: args))
            | ann, _ -> parse ~ctx (e_call ~ann expr @@ [ e_ellipsis ~ann () ])
          in
          Ctx.clear_block ~ctx;
          (expr, g), (op, expr))
    in
    Ctx.clear_block ~ctx;
    let typ =
      if is_gen
      then R.internal ~ctx ~args:[ var_of_node_exn last_expr ] "generator"
      else var_of_node_exn last_expr
    in
    C.ann ~typ:(Var typ) ctx, Pipe exprs

and parse_lambda ctx (args, expr) =
  (* Ctx.add_block ctx;
  let ctx = TC.enter_level ctx in
  let args = List.map args ~f:(fun arg ->
    let typ = TU.make_unbound ~pos ctx in
    Ctx.add ctx arg (typ, false);
    arg, typ)
  in
  let expr = fst @@ parse ctx expr in
  let ctx = TC.exit_level ctx in
  Ctx.clear_block ctx; *)
  C.err ~ctx "lambda/not yet"

(* let t = TU.make_link ~pos @@ (ref [pos], Func
    { name = "<lambda>";
      cache = "<lambda>", Ast.Pos.dummy;
      realization = ctx.env.enclosing_name, None;
      generics = [], [];
      args = (args, TU.type_of expr) })
  in
  (t, false), Lambda (List.map args ~f:fst, expr) *)
