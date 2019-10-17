(* ****************************************************************************
 * Seqaml.Typecheck_expr: Typecheck Seq code from expression AST
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Err
open Option.Monad_infix

(** This module implements [Typecheck_intf.Expr].
    Parametrized by [Typecheck_intf.Stmt] for parsing generators ([parse_for] and [finalize]) *)
module Typecheck (R : Typecheck_intf.Real) : Typecheck_intf.Expr = struct
  open Ast
  open Ast.Expr

  module C = Typecheck_ctx
  module T = Typecheck_infer

  (* ***************************************************************
     Public interface
     *************************************************************** *)

  (** [parse ~ctx expr] dispatches an expression AST node [expr] to the proper code generation function. *)
  let rec parse ~(ctx : C.t) ((ann, node) : Expr.t Ann.ann) : Expr.t Ann.ann =
    C.push_ann ~ctx ann;
    Util.A.db "[e] %s" (Expr.to_string (ann, node));
    let expr : Expr.t Ann.ann =
      match node with
      | Empty          _ -> parse_none     ctx node
      | Bool           _ -> parse_internal ctx node ~name:"bool"
      | Int            _ -> parse_internal ctx node ~name:"int"
      | Float          _ -> parse_internal ctx node ~name:"float"
      | String         _ -> parse_internal ctx node ~name:"str"
      | Seq            _ -> parse_internal ctx node ~name:"seq"
      | Id             p -> parse_id       ctx p
      | Tuple          p -> parse_tuple    ctx p
      | List           p -> parse_collection ctx p ~kind:"list"
      | Set            p -> parse_collection ctx p ~kind:"set"
      | Dict           p -> parse_dict ctx p
      | Generator      p -> parse_generator     ctx p
      | ListGenerator  p -> parse_comprehension ctx p ~kind:"list"
      | SetGenerator   p -> parse_comprehension ctx p ~kind:"set"
      | DictGenerator  p -> parse_dict_comprehension ctx p
      | IfExpr         p -> parse_if       ctx p
      | Unary          p -> parse_unary    ctx p
      | Binary         p -> parse_binary   ctx p
      | Call           p -> parse_call     ctx p
      | Index          p -> parse_index    ctx p
      | Dot            p -> parse_dot      ctx p
      | TypeOf         p -> parse_typeof   ctx p
      | Lambda         p -> parse_lambda   ctx p
      | Pipe           p -> parse_pipe     ctx p
      | Ptr            p -> parse_ptr      ctx p
      | Ellipsis       _ -> parse_ellipsis ctx node
      | Slice  _ -> C.err ~ctx "slice is only valid within an index"
      | Unpack _ -> C.err ~ctx "unpack is not valid here"
      | IntS _ | FloatS _ | Kmer _ -> C.err ~ctx "expr not supported :/"
    in
    C.pop_ann ~ctx |> ignore;
    (* Util.A.dr "%s -> %s |= %s"
      (Expr.to_string (ann, node)) (Expr.to_string expr) (Ann.to_string (fst expr)); *)
    expr

  and parse_none ctx n =
    let typ = C.make_unbound ctx in
    C.ann ~typ:(Var typ) ctx, n

  and parse_ellipsis ctx n =
    (* Partial calls might add "..." to the context *)
    let typ = match Ctx.in_scope ~ctx "..." with
      | Some (Var _ as t) -> t
      | Some _ -> C.err ~ctx "wrong ellipsis type"
      | None -> Var (C.make_unbound ctx)
    in
    C.ann ~typ ctx, n

  and parse_internal ~name ctx n =
    let typ = R.internal ~ctx name in
    C.ann ~typ:(Var typ) ctx, n

  and parse_id ctx name =
    let inst t =
      let t = T.instantiate ~ctx t in
      if Ann.is_realizable t then R.realize ~ctx t else t
    in
    let typ = match Ctx.in_scope ~ctx name with
      | Some Type t -> Ann.Type (inst t)
      | Some Var t -> Var (inst t)
      | Some Import s -> Import s
        (* Ann.var_of_typ_exn (C.ann ctx).typ *)
      | _ -> C.err ~ctx "%s not found" name
    in
    C.ann ~typ ctx, Id name

  and parse_tuple ctx args =
    let args = List.map args ~f:(parse ~ctx) in
    let typ = Ann.Tuple (List.map args ~f:(fun (t, _) -> Ann.var_of_typ_exn t.typ)) in
    C.ann ~typ:(Var typ) ctx, Tuple args

  and parse_collection ~kind ctx items =
    let typ, items =
      match kind, items with
      | _, [] ->
        let typ = C.make_unbound ctx in
        R.internal ~ctx kind ~args:[typ], []
      | _, items ->
        let items = List.rev @@ List.fold items ~init:[]
          ~f:(fun acc e ->
              let e = parse ~ctx e in
              let t = var_of_node_exn e in
              if List.length acc > 0 then
                T.unify_inplace ~ctx t (var_of_node_exn (List.hd_exn acc));
              e :: acc)
        in
        R.internal ~ctx ~args:[var_of_node_exn (List.hd_exn items)] kind, items
    in
    let n =
      match kind with
      | "list" -> List items
      | "set" -> Set items
      | k -> ierr "[parse_collection] unknown kind %s" k
    in
    C.ann ~typ:(Var typ) ctx, n

  and parse_dict ctx items =
    let typ, items =
      match items with
      | [] ->
        let tkey = C.make_unbound ctx in
        let tval = C.make_unbound ctx in
        R.internal ~ctx "dict" ~args:[tkey; tval], []
      | items ->
        let items = List.rev @@ List.fold items ~init:[]
          ~f:(fun acc (tkey, tval) ->
              let ekey = parse ~ctx tkey in
              let eval = parse ~ctx tval in
              match acc with
                | [] ->
                  (ekey, eval) :: acc
                | (hkey, hval) :: _ ->
                  T.unify_inplace ~ctx (var_of_node_exn ekey) (var_of_node_exn hkey);
                  T.unify_inplace ~ctx (var_of_node_exn eval) (var_of_node_exn hval);
                  (ekey, eval) :: acc)
        in
        let tkey, tval = List.hd_exn items in
        R.internal ~ctx "dict" ~args:[var_of_node_exn tkey; var_of_node_exn tval], items
    in
    let n = Dict items in
    C.ann ~typ:(Var typ) ctx, n

  and parse_comprehension ~kind ctx (expr, comp) =
    let append, empty =
      match kind with
      | "list" -> "append", List []
      | "dict" -> "insert", Dict []
      | "set" -> "insert", Set []
      | k -> ierr "[parse_comprehension] unknown kind %s" k
    in
    let temp_var = C.make_temp ctx ~prefix:"cph" in
    let rec s_generator = function
      | None ->
        s_expr (e_call (e_dot (e_id temp_var) append) [expr])
      | Some { var ; gen ; cond ; next } ->
        let stmt = s_generator next in
        let stmt = Option.value_map cond ~default:stmt ~f:(fun c -> s_if c [stmt]) in
        s_for var gen [stmt]
    in
    let stmts =
      [ s_assign (e_id temp_var) (C.ann ctx, empty)
      ; s_generator (Some comp) ]
    in
    List.iter stmts ~f:(fun s -> Stack.push ctx.env.statements (C.sannotate ~ctx s));
    parse_id ctx temp_var

  and parse_dict_comprehension ctx (expr, comp) =
    let temp_var = C.make_temp ctx ~prefix:"cph" in
    let rec s_generator = function
      | None ->
        s_expr (e_call (e_dot (e_id temp_var) "insert") [snd expr])
      | Some { var ; gen ; cond ; next } ->
        let stmt = s_generator next in
        let stmt = Option.value_map cond ~default:stmt ~f:(fun c -> s_if c [stmt]) in
        s_for var gen [stmt]
    in
    let stmts =
      [ s_assign (e_id temp_var) (C.ann ctx, Dict [])
      ; s_generator (Some comp) ]
    in
    List.iter stmts ~f:(fun s -> Stack.push ctx.env.statements (C.sannotate ~ctx s));
    parse_id ctx temp_var

  and parse_generator ctx  _ =
    ierr "Not yet there"

  and parse_if ctx (cond, if_expr, else_expr) =
    let cond = C.magic_call parse ~ctx ~magic:"__bool__" cond in

    let if_expr = parse ~ctx if_expr in
    let else_expr = parse ~ctx else_expr in
    T.unify_inplace ~ctx (var_of_node_exn if_expr) (var_of_node_exn else_expr);

    C.ann ~typ:(Var (var_of_node_exn if_expr)) ctx, IfExpr (cond, if_expr, else_expr)

  and parse_unary ctx (op, expr) =
    C.magic_call parse ~ctx ~magic:(Util.uop2magic op) expr

  and parse_binary ctx (lh, bop, rh) =
    match bop with
    | "&&" | "||" -> (* these have no magics *)
      let lh = C.magic_call parse ~ctx ~magic:"__bool__" lh in
      let rh = C.magic_call parse ~ctx ~magic:"__bool__" rh in
      C.ann ~typ:(Var (var_of_node_exn rh)) ctx, Binary (lh, bop, rh)
    | bop when (String.prefix bop 8) = "inplace_" ->
      ierr ~ctx "inplace should be handled at assignment time"
    | bop ->
      let magic = Util.bop2magic bop in
      let lh = parse ~ctx lh in
      let rh = parse ~ctx rh in
      let typ = R.magic ~ctx ~args:[var_of_node_exn rh] (var_of_node_exn lh) magic in
      let typ = match typ with
        | Some t -> t
        | None ->
          let rmagic = Util.bop2magic ~prefix:"r" bop in
          let typ = R.magic ~ctx ~args:[var_of_node_exn lh] (var_of_node_exn rh) rmagic in
          match typ, ctx.env.realizing with
          | Some t, _ -> t
          | None, false -> C.make_unbound ctx
          | None, _ -> C.err ~ctx "cannot locate appropriate %s or %s for %s and %s"
            magic rmagic
            (Ann.var_to_string @@ var_of_node_exn lh)
            (Ann.var_to_string @@ var_of_node_exn rh)
      in
      C.ann ~typ:(Var typ) ctx, Binary (lh, bop, rh)

  and parse_call ctx (expr, args) =
    let is_partial = fun { value; _ } ->
      match value with _, Expr.Ellipsis _ -> true | _ -> false
    in
    match expr, args with
    (* Special magics for those : str(...), int(...) etc *)
    (* | (_, Id ("str" as name)), [arg]
    | (_, Id ("int" as name)), [arg]
    | (_, Id ("bool" as name)), [arg] ->
      let magic = sprintf "__%s__" name in
      C.magic_call parse ~ctx ~magic arg.value *)
    | _ ->
      let expr = parse ~ctx expr in
      (* Util.A.dg "[out] %s  : %s [%s]"  (Expr.to_string (expr)) (Ann.to_string (fst expr))
        (fst@@C.get_full_name ~ctx @@ var_of_node_exn expr); *)
      let typ = Ann.real_type (var_of_node_exn expr) |> T.instantiate ~ctx in
      match typ with
      | Func (g, f) ->
        let arg_table = String.Table.create () in
        let def_args = Array.of_list
          @@  List.filter_mapi g.args ~f:(fun i a ->
                (* Util.A.db "->> defarg %s" @@ (fst @@ C.get_full_name ~ctx (snd a)); *)
                Hashtbl.set arg_table ~key:(fst a) ~data:(i, snd a);
                if Hash_set.exists f.used ~f:((=) (fst a)) then None else Some a)
        in
        let has_partials = List.exists args ~f:is_partial in
        let args = List.map args ~f:(fun arg -> { arg with value = parse ~ctx arg.value }) in
        let in_args =
          if List.length args <> Array.length def_args then (
            if List.length args = Array.length def_args + 1
              && is_partial (List.last_exn args)
            then List.rev args |> List.tl_exn |> List.rev
            else C.err ~ctx "function argument length mismatch")
          else args
        in
        (* Check ordering *)
        let names_started = ref false in
        let used = Hash_set.copy f.used in
        List.iteri in_args ~f:(fun i arg ->
          let name = match arg.name, !names_started with
            | None, false -> fst def_args.(i)
            | None, true ->
              C.err ~ctx "named arguments must be followed by named arguments"
            | Some name, false -> names_started := true; name
            | Some name, true -> name
          in
          match Hashtbl.find arg_table name, is_partial arg with
          | Some (_, typ'), true ->
            let typ = C.make_unbound ctx in
            T.unify_inplace ~ctx typ' typ
          | Some (i, typ'), false ->
            Hash_set.add used name;
            Util.A.db "->> inarg %s: def = %s, pass = %s" name
              (Ann.var_to_string typ')
              (Ann.var_to_string (var_of_node_exn arg.value));
            T.unify_inplace ~ctx (var_of_node_exn arg.value) typ';
            Hashtbl.set arg_table ~key:name ~data:(i, var_of_node_exn arg.value)
                        (* Util.A.db "->< inarg %s: def = %s, pass = %s" name
              (fst @@ C.get_full_name ~ctx typ')
              (fst @@ C.get_full_name ~ctx (var_of_node_exn arg.value)); *)
          | _ ->
            C.err ~ctx "argument %s does not exist" name);
        (* Hashtbl.to_alist arg_table |> List.iter ~f:(fun (n, (_, v)) ->
             Util.A.dr "[arg] %s= %s" n (Ann.var_to_string ~full:true v)); *)
        if has_partials then (
          let new_args =
            Hashtbl.to_alist arg_table
             |> List.sort ~compare:(fun (_, (i, _)) (_, (j, _)) -> Int.compare i j)
             |> List.map ~f:(fun (n, (_, v)) -> n, v)
          in
          let typ =
            T.generalize ~level:(ctx.env.level) @@
            Ann.(Func ({ g with args = new_args }, { f with used }))
          in
          (* Util.A.db "%s -> %s" (Expr.to_string (C.ann ctx, Call (expr, args)))
            (Ann.var_to_string typ); *)
          C.ann ~typ:(Var typ) ctx, Call (expr, args)
        ) else (
          let typ = R.realize ~ctx typ in
          let expr = (Ann.patch (fst expr) typ), snd expr in
          C.ann ~typ:(Var f.ret) ctx, Call (expr, args))
      | Class ({ cache = "ptr", p; _ }, _) when p = Ann.default_pos ->
        let args = List.map args ~f:(fun p -> { p with value = parse ~ctx p.value }) in
        let typ = match List.map args ~f:(fun p -> var_of_node_exn p.value |> Ann.real_type) with
          | [ Class ({ cache = "int", p; _ }, _)  ] -> typ
          | _ -> C.err ~ctx "ptr needs an int initializer"
        in
        (* ( match R.magic ~ctx typ "__init__" ~args:arg_typs with
          | None when ctx.env.realizing ->
            C.err ~ctx "cannot locate %s.__init__(%s)"
              (Ann.var_to_string typ) (Util.ppl arg_typs ~f:(Ann.var_to_string))
          | Some (Func (_, { ret; _ })) when is_some types ->
            T.unify_inplace ~ctx ret typ
          | _ -> ()); *)
        C.ann ~typ:(Var typ) ctx, Call (expr, args)
      | Class (_, { types }) ->
        let args = List.map args ~f:(fun p -> { p with value = parse ~ctx p.value }) in
        let arg_typs = List.map args ~f:(fun p -> var_of_node_exn p.value) in
        ( match R.magic ~ctx typ "__init__" ~args:arg_typs with
          | None when ctx.env.realizing ->
            C.err ~ctx "cannot locate %s.__init__(%s)"
              (Ann.var_to_string typ) (Util.ppl arg_typs ~f:(Ann.var_to_string))
          | Some (Func (_, { ret; _ })) when is_some types ->
            T.unify_inplace ~ctx ret typ
          | _ -> ());
        C.ann ~typ:(Var typ) ctx, Call (expr, args)
      | Link { contents = Unbound _ } ->
        let args = List.map args ~f:(fun p -> { p with value = parse ~ctx p.value }) in
        let typ = C.make_unbound ctx in
        C.ann ~typ:(Var typ) ctx, Call (expr, args)
      | _ ->
        C.err ~ctx "wrong call type %s" (Ann.var_to_string typ)

  and parse_index ctx (lh_expr', indices') =
    match indices' with
    | [_, Slice (Some a, Some b, None) ] ->
      parse ~ctx @@ C.epatch ~ctx (e_call (e_dot lh_expr' "__slice__") [a; b])
    | [_, Slice (None, Some b, None)] ->
      parse ~ctx @@ C.epatch ~ctx (e_call (e_dot lh_expr' "__slice_left__") [b])
    | [_, Slice (Some a, None, None)] ->
      parse ~ctx @@ C.epatch ~ctx (e_call (e_dot lh_expr' "__slice_right__") [a])
    | _ ->
      let indices = List.map indices' ~f:(parse ~ctx) in
      let has_types = List.for_all indices ~f:(fun (x, _) -> Ann.is_type x) in
      let all_types = List.exists  indices ~f:(fun (x, _) -> Ann.is_type x) in
      if has_types && (not all_types) then
        C.err ~ctx "passed types where variables are expected";
      match all_types, snd lh_expr' with
      | true, (Id "tuple" as lh) ->
        { (C.ann ctx) with typ = Some (Type (Tuple (List.map indices ~f:var_of_node_exn))) }, lh
      | false, (Id "tuple" as lh) ->
        C.err ~ctx "tuple needs types"
      (* TODO: Id function *)
      | true, _ ->
        let lh_expr = parse ~ctx lh_expr' in
        let typ = Ann.real_type (var_of_node_exn lh_expr) in
        (* Type instantiation case *)
        ( match typ with
          | Class (g, _) | Func (g, _) ->
            if List.((length g.generics) <> (length indices)) then
              C.err ~ctx "Generic length does not match";
            List.iter2_exn g.generics indices ~f:(fun (_, (_, t)) (i, _) ->
              T.unify_inplace ~ctx t (Ann.var_of_typ_exn i.typ));
            let typ =
              if Ann.is_realizable typ then R.realize ~ctx typ else typ
            in
            if not ctx.env.realizing then (
              C.ann ~typ:(Type typ) ctx, Index (lh_expr, indices)
            )
            else (* remove type info: e.g. T[t,u] -> T *) (
              C.ann ~typ:(Type typ) ctx, snd lh_expr
            )
          (* Recursive class instantiation (e.g. class A[T]: y:A[T]) *)
          | Link { contents = Unbound _ } as t when not ctx.env.realizing ->
            let typ = C.make_unbound ctx in
            C.ann ~typ:(Type typ) ctx, Index (lh_expr, indices)
          | _ ->
            (* C.dump_ctx ctx; *)
            C.err ~ctx "cannot instantiate non-generic %s" @@
              Ann.t_to_string (fst lh_expr).typ )
      (* Member access case *)
      | false, _ ->
        let lh_expr = parse ~ctx lh_expr' in
        let lt = Ann.real_type (var_of_node_exn lh_expr) in
        match lt, indices' with
        | _, [] -> C.err ~ctx "empty index"
        | Tuple ts, [_, Int i] -> (* tuple static access typecheck *)
          let typ = Caml.Int64.of_string_opt i >>= Int.of_int64 >>= List.nth ts in
          ( match typ with
            | Some typ -> C.ann ~typ:(Var typ) ctx, Index (lh_expr, indices)
            | None -> C.err ~ctx "invalid tuple index" )
        | Tuple _, _ ->
          C.err ~ctx "tuple access requires a valid compile-time integer constant"
        | Class ({ cache = ("ptr", pos); generics = [_, (_, t)]; _ }, _), [v]
          when pos = Ann.default_pos ->
          C.ann ~typ:(Var t) ctx, Index (lh_expr, indices)
        | _, _ ->
          let ast = e_call (e_dot lh_expr "__getitem__")
            [match indices with [a] -> a | a -> e_tuple a]
          in
          parse ~ctx (C.epatch ~ctx ast)

  and parse_dot ctx (lh_expr, rhs) =
    let lh_expr = parse ~ctx lh_expr in
    let lt, resolved = match (fst lh_expr).typ with
      | None ->
        ierr ~ctx "cannot happen"
      | Some (Import name) ->
        ierr ~ctx "no imports yet"
        (* Hashtbl.find C.imported name >>= fun (_, dict) -> Hashtbl.find dict rhs *)
      | Some (Type lt | Var lt) ->
        match Ann.real_type lt with
        | Class (g, _) ->
          lt, Hashtbl.find ctx.globals.classes g.cache
          >>= fun dict -> Hashtbl.find dict rhs
          >>= List.hd
        | lt -> lt, None
    in
    let is_lt_type = Ann.is_type (fst lh_expr) in
    match is_lt_type, resolved with
    (* Nested class access--- X.Y or obj.Y *)
    | _, Some (Type t) ->
      let typ = T.link_to_parent ~parent:(Some lt) (T.instantiate ~ctx ~parent:lt t) in
      (* Util.A.dy "[%s]" (fst@@C.get_full_name ~ctx typ); *)
      C.ann ~typ:(Type typ) ctx, Dot (lh_expr, rhs)
    (* Nested function access--- X.fn *)
    | true, Some (Var (Func (g, _) as t)) ->
      (* Util.A.dy "[%s] = linking %s to %s"
        (Expr.to_string (C.ann ctx, Expr.Dot (lh_expr, rhs)))
        (Ann.var_to_string @@ T.instantiate ~ctx ~parent:lt t)
        (fst@@C.get_full_name ~ctx lt); *)
      let typ = T.link_to_parent ~parent:(Some lt) (T.instantiate ~ctx ~parent:lt t) in
      (* ( match Ann.real_type typ  with
        | Func (g, _) -> Util.A.dy "... is now %s ~ %s" (fst@@C.get_full_name ~ctx typ)
        (fst@@C.get_full_name ~ctx (snd@@List.hd_exn g.args))
        | _ -> ()); *)
      C.ann ~typ:(Var typ) ctx, Dot (lh_expr, rhs)
    (* Object method access--- obj.fn *)
    | false, Some (Var (Func ({ args = ((hd_n, hd_t) :: _) as args; _ } as g, ({ used; _ } as f)) as t)) ->
      let trail = match args with
        | [a] -> [e_ellipsis ()]
        | a -> List.init (pred @@ List.length args) ~f:(fun _ -> e_ellipsis ())
      in
      (* Util.A.dy "<< lhs := %s" (Ann.var_to_string ~full:true lt); *)
      (* T.unify_inplace ~ctx (T.instantiate ~ctx hd_t) lt; *)
      (*Util.A.dy "[ >>> %s [%s]"
            (fst @@ C.get_full_name ~ctx lt)
            (fst @@ C.get_full_name ~ctx @@ var_of_node_exn lh_expr);
      let used = Hash_set.copy used in
      Hash_set.add used hd_n;
      C.ann ~typ:(Var (Func (g, { f with used }))) ctx,
        Call (
          (C.ann ~typ:(Var t) ctx, Dot ((C.ann ~typ:(Var lt) ctx, TypeOf lh_expr), rhs)),
          List.map (lh_expr :: trail) ~f:(fun value -> { name = None; value })
        ) *)
      (* let typ = T.link_to_parent ~parent:(Some lt) (T.instantiate ~ctx ~parent:lt t) in *)
      (* Util.A.dy "[ >>> %s [%s]"
            (fst @@ C.get_full_name ~ctx lt)
            (fst @@ C.get_full_name ~ctx hd_t); *)
      let lhs = parse ~ctx @@ C.eannotate ~ctx @@ e_dot (e_typeof lh_expr) rhs in
      let res = parse_call ctx (lhs, List.map (lh_expr :: trail) ~f:(fun value -> { name = None; value })) in
      res
      (* e_call ( (lh_expr :: trail) *)
       (* |> *)
       (* |> *)
      (* T.unify_inplace ~ctx (T.instantiate ~ctx hd_t) lt; *)
      (* C.ann ~typ:(Var typ) ctx, Dot (lh_expr, rhs) *)
    (* Object member access--- obj.mem *)
    | false, Some (Var t) ->
      (* What if there are NO proper parents? *)
      let typ = T.link_to_parent ~parent:(Some lt) (T.instantiate ~ctx ~parent:lt t) in
      C.ann ~typ:(Var typ) ctx, Dot (lh_expr, rhs)
    (* Nested member access: X.member *)
    | _, Some _ ->
      C.err ~ctx "member access requires object"
    | _, None ->
      (* Util.A.dy ">> unknown %s.%s" (Ann.var_to_string lt) rhs; *)
      let typ = match lt with
        | Link { contents = Unbound _ } -> C.make_unbound ctx
        | Class ({ cache; _ }, _) when Some cache = ctx.env.enclosing_type ->
          C.make_unbound ctx
        | t -> C.err ~ctx "cannot find %s in %s" rhs (Ann.var_to_string lt)
      in
      C.ann ~typ:(Var typ) ctx, Dot (lh_expr, rhs)

  and parse_typeof ctx expr =
    let expr = parse ~ctx expr in
    let typ = match var_of_node expr with
      | Some t -> t
      | _ -> C.err ~ctx "typeof does not support imports"
    in
    C.ann ~typ:(Type typ) ctx, TypeOf expr

  and parse_ptr ctx expr =
    let expr = parse ~ctx expr in
    let typ = R.internal ~ctx "ptr" ~args:[var_of_node_exn expr] in
    C.ann ~typ:(Var typ) ctx, Ptr expr

  and parse_pipe ctx exprs =
    match exprs with
    | [] | [_] ->
      ierr "empty pipe construct"
    | (_, hd) :: tl ->
      let hd = parse ~ctx hd in
      let (last_expr, is_gen), exprs = List.fold_map tl ~init:(hd, false)
        ~f:(fun (pt, g) (op, expr) ->
            let pt = Ann.real_type (var_of_node_exn pt) in
            let missing_type, g = match pt with
              | Class ({ cache = ("generator", _); generics = [_, (_, t)]; _ }, _) ->
                t, true
              | _ -> pt, g
            in
            Ctx.add_block ~ctx;
            Ctx.add ~ctx "..." (Var missing_type);
            let has_ellipsis = fun { value = _, x ; _ } ->
              match x with Ellipsis _ -> true | _ -> false
            in
            let expr = match expr with
              | _, Call (callee, args) when List.exists args ~f:has_ellipsis  ->
                parse ~ctx expr
              | ann, Call (callee, args) ->
                let args = List.map args ~f:(fun { value; _ } -> value) in
                parse ~ctx (e_call ~ann callee @@ (e_ellipsis ~ann ()) :: args)
              | ann, _ ->
                parse ~ctx (e_call ~ann expr @@ [e_ellipsis ~ann ()])
            in
            Ctx.clear_block ~ctx;
            (expr, g), (op, expr))
      in
      Ctx.clear_block ~ctx;
      let typ =
        if is_gen then R.internal ~ctx ~args:[var_of_node_exn last_expr] "generator"
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
end
