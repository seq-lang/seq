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
    (* Util.A.dr ">>= %s" (Expr.to_string (ann, node)); *)
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
    C.ann ~typ ctx, n

  and parse_ellipsis ctx n =
    (* Partial calls might add "..." to the context *)
    let typ = match Ctx.in_scope ~ctx "..." with
      | Some (C.Var t) -> t
      | Some _ -> C.err ~ctx "wrong ellipsis type"
      | None -> C.make_unbound ctx
    in
    C.ann ~typ ctx, n

  and parse_internal ~name ctx n =
    let typ = R.internal ~ctx name in
    C.ann ~typ ctx, n

  and parse_id ctx name =
    let typ = match Ctx.in_scope ~ctx name with
      | Some (Var t | Member t | Type t) -> T.instantiate ~ctx t
      | Some (Import t) -> C.ann ctx
      | None -> C.err ~ctx "%s not found" name
    in
    let typ =
      if Ann.is_realizable typ then R.realize ~ctx typ else typ
    in
    C.ann ~typ ~is_type_ast:typ.is_type_ast ctx, Id name

  and parse_tuple ctx args =
    let args = List.map args ~f:(parse ~ctx) in
    let typ = Ann.create ~typ:(Tuple (List.map args ~f:fst)) () in
    C.ann ~typ ctx, Tuple args

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
              if List.length acc > 0 then
                T.unify_inplace ~ctx (fst e) (fst @@ List.hd_exn acc);
              e :: acc)
        in
        R.internal ~ctx ~args:[fst (List.hd_exn items)] kind, items
    in
    let n =
      match kind with
      | "list" -> List items
      | "set" -> Set items
      | k -> ierr "[parse_collection] unknown kind %s" k
    in
    C.ann ~typ ctx, n

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
                  T.unify_inplace ~ctx (fst ekey) (fst hkey);
                  T.unify_inplace ~ctx (fst eval) (fst hval);
                  (ekey, eval) :: acc)
        in
        let tkey, tval = List.hd_exn items in
        R.internal ~ctx "dict" ~args:[fst tkey; fst tval], items
    in
    let n = Dict items in
    C.ann ~typ ctx, n

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
    T.unify_inplace ~ctx (fst if_expr) (fst else_expr);

    C.ann ~typ:(fst if_expr) ctx, IfExpr (cond, if_expr, else_expr)

  and parse_unary ctx (op, expr) =
    C.magic_call parse ~ctx ~magic:(uop2magic op) expr

  and parse_binary ctx (lh, bop, rh) =
    match bop with
    | "&&" | "||" -> (* these have no magics *)
      let lh = C.magic_call parse ~ctx ~magic:"__bool__" lh in
      let rh = C.magic_call parse ~ctx ~magic:"__bool__" rh in
      C.ann ~typ:(fst rh) ctx, Binary (lh, bop, rh)
    | bop when (String.prefix bop 8) = "inplace_" ->
      let magic = bop2magic ~prefix:"i" bop in
      let bop = String.drop_prefix bop 8 in
      C.magic_call parse ~ctx ~magic lh ~args:[rh]
    | bop ->
      let magic = bop2magic bop in
      let lh = parse ~ctx lh in
      let rh = parse ~ctx rh in
      let typ = R.magic ~ctx ~args:[fst rh] (fst lh) magic in
      let typ = match typ with
        | Some t -> t
        | None ->
          let rmagic = bop2magic ~prefix:"r" bop in
          let typ = R.magic ~ctx ~args:[fst lh] (fst rh) rmagic in
          match typ with
          | Some t -> t
          | None -> C.err ~ctx "cannot locate appropriate %s or %s" magic rmagic
      in
      C.ann ~typ ctx, Binary (lh, bop, rh)

  and parse_call ctx (expr, args) =
    match expr, args with
    (* Special magics for those : str(...), int(...) etc *)
    | (_, Id ("str" as name)), [arg]
    | (_, Id ("int" as name)), [arg]
    | (_, Id ("bool" as name)), [arg] ->
      let magic = sprintf "__%s__" name in
      C.magic_call parse ~ctx ~magic arg.value
    | _ ->
      (* Util.A.dg "[in] %s : %s" (Ann.to_string (fst expr)) (Expr.to_string (expr)); *)
      let expr = parse ~ctx expr in
      (* Util.A.dg "[out] %s : %s" (Ann.to_string (fst expr)) (Expr.to_string (expr)); *)
      let args = List.map args ~f:(fun p -> { p with value = parse ~ctx p.value }) in
      let typ = Ann.real_type (fst expr) in
      match typ.typ with
      | Func f ->
        let f_args = match snd expr with
          | Dot _ -> List.tl_exn f.f_args (** Avoid methods! *)
          | _ -> f.f_args
        in
        if List.(length args <> length f_args) then
          C.err ~ctx "function argument length mismatch";
        List.iter2_exn args f_args
          ~f:(fun { name; value } (t_name, t) ->
            if (Option.value name ~default:t_name) <> t_name then
              C.err ~ctx "names do not match";
            T.unify_inplace ~ctx (fst value) t);
        Util.A.dg "[will_realize] %s " (Ann.to_string { typ with typ = Func f });
        let typ = R.realize ~ctx { typ with typ = Func f } in
        let expr = typ, snd expr in
        C.ann ~typ:f.f_ret ctx, Call (expr, args)
      | Class _ ->
        let arg_t = List.map args ~f:(fun { name; value } -> fst value) in
        if is_none (R.magic ~ctx typ "__init__" ~args:arg_t) then
          C.err ~ctx "cannot find __init__ for %s" (Ann.typ_to_string typ);
        C.ann ~typ ctx, Call (expr, args)
      | TypeVar { contents = Unbound _ } ->
        let typ = C.make_unbound ctx in
        C.ann ~typ ctx, Call (expr, args)
      | _ ->
        C.err ~ctx "wrong call type %s" (Ann.typ_to_string typ)

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
      let has_types = List.for_all indices ~f:(fun (x, _) -> x.is_type_ast) in
      let all_types = List.exists  indices ~f:(fun (x, _) -> x.is_type_ast) in
      if has_types && (not all_types) then
        C.err ~ctx "passed types where variables are expected";
      match all_types, snd lh_expr' with
      | true, (Id "tuple" as lh) ->
        { (C.ann ctx) with typ = Ann.Tuple (List.map indices ~f:fst) }, lh
      | false, (Id "tuple" as lh) ->
        C.err ~ctx "tuple needs types"
      (* TODO: Id function *)
      | true, _ ->
        let lh_expr = parse ~ctx lh_expr' in
        let is_type_ast = (fst lh_expr).is_type_ast in
        let typ = Ann.real_type (fst lh_expr) in
        (* Type instantiation case *)
        ( match typ.typ with
          | Class { c_generics = g; _ } | Func { f_generics = g; _ } ->
            if List.((length g) <> (length indices)) then
              C.err ~ctx "Generic length does not match";
            List.iter2_exn g indices ~f:(fun (_, (_, t)) (i, _) ->
              T.unify_inplace ~ctx t i);
            let typ =
              if Ann.is_realizable typ then R.realize ~ctx typ else typ
            in
            if not ctx.env.realizing then (
              C.ann ~typ ~is_type_ast ctx, Index (lh_expr, indices)
            )
            else (* remove type info: e.g. T[t,u] -> T *) (
              C.ann ~typ ~is_type_ast ctx, snd lh_expr
            )
          (* Recursive class instantiation (e.g. class A[T]: y:A[T]) *)
          | TypeVar { contents = Unbound _ } as t when not ctx.env.realizing ->
            assert is_type_ast;
            let typ = C.make_unbound ctx in
            C.ann ~typ ~is_type_ast:true ctx, Index (lh_expr, indices)
          | _ ->
            (* C.dump_ctx ctx; *)
            C.err ~ctx "cannot instantiate non-generic %s" @@
              Ann.typ_to_string (fst lh_expr) )
      (* Member access case *)
      | false, _ ->
        let lh_expr = parse ~ctx lh_expr' in
        let lt = Ann.real_type (fst lh_expr) in
        match lt.typ, indices' with
        | Tuple ts, [_, Int i] -> (* tuple static access typecheck *)
          let typ = Caml.Int64.of_string_opt i >>= Int.of_int64 >>= List.nth ts in
          ( match typ with
            | Some typ -> C.ann ~typ ctx, Index (lh_expr, indices)
            | None -> C.err ~ctx "invalid tuple index" )
        | Tuple _, _ ->
          C.err ~ctx "tuple access requires a valid compile-time integer constant"
        | _ ->
          let ast = e_call (e_dot lh_expr "__getitem__") [e_tuple indices] in
          parse ~ctx (C.epatch ~ctx ast)

  and parse_dot ctx (lh_expr, rhs) =
    let lh_expr = parse ~ctx lh_expr in
    let is_type_ast = (fst lh_expr).is_type_ast in
    let lt = Ann.real_type (fst lh_expr) in
    let resolved = (match lt.typ with
      | Class { c_cache; _ } ->
        Hashtbl.find ctx.globals.classes c_cache
        >>= fun dict -> Hashtbl.find dict rhs
        >>= List.hd
      | Import name ->
        ierr "no imports yet"
        (* Hashtbl.find C.imported name >>= fun (_, dict) -> Hashtbl.find dict rhs *)
      | _ -> None)
    in
    let ann = match is_type_ast, resolved with
      (* Nested class access--- X.Y or obj.Y *)
      | _, Some (Type t) ->
        let typ = Ann.link_to_parent ~parent:(Some lt) (T.instantiate ~ctx ~parent:lt t) in
        C.ann ~typ ~is_type_ast:true ctx
      (* Nested function access--- X.fn *)
      | true, Some (Var ({ typ = Func _; _ } as t)) ->
        let typ = Ann.link_to_parent ~parent:(Some lt) (T.instantiate ~ctx ~parent:lt t) in
        C.ann ~typ ctx
      (* Object method access--- obj.fn *)
      | false, Some (Var ({ typ = Func _; _ } as t)) ->
        let typ = Ann.link_to_parent ~parent:(Some lt) (T.instantiate ~ctx ~parent:lt t) in
        ( match t.typ with
          | Func { f_args = (_, hd_t) :: _; _ } ->
            T.unify_inplace ~ctx (T.instantiate ~ctx hd_t) lt;
            C.ann ~typ ctx
          | _ -> C.err ~ctx "cannot call %s as a method" rhs )
      (* Object member access--- obj.mem *)
      | false, Some (Member t) ->
        (* What if there are NO proper parents? *)
        let typ = Ann.link_to_parent ~parent:(Some lt) (T.instantiate ~ctx ~parent:lt t) in
        C.ann ~typ ctx
      (* Nested member access: X.member *)
      | _, Some _ ->
        C.err ~ctx "member access requires object"
      | _, None ->
        let typ = match lt.typ with
          | TypeVar { contents = Unbound _ } -> C.make_unbound ctx
          | Class { c_cache; _ } when Some c_cache = ctx.env.enclosing_type -> C.make_unbound ctx
          | t -> C.err ~ctx "cannot find %s in %s" rhs (Ann.typ_to_string lt)
        in
        C.ann ~typ ctx
  in
  ann, Dot (lh_expr, rhs)

  and parse_typeof ctx expr =
    let expr = parse ~ctx expr in
    C.ann ~typ:(fst expr) ~is_type_ast:true ctx, TypeOf expr

  and parse_ptr ctx expr =
    let expr = parse ~ctx expr in
    let typ = R.internal ~ctx "ptr" ~args:[fst expr] in
    C.ann ~typ ctx, Ptr expr

  and parse_pipe ctx exprs =
    match exprs with
    | [] | [_] ->
      ierr "empty pipe construct"
    | (_, hd) :: tl ->
      let hd = parse ~ctx hd in
      let (last_expr, is_gen), exprs = List.fold_map tl ~init:(hd, false)
        ~f:(fun (pt, g) (op, expr) ->
            let pt = Ann.real_type (fst pt) in
            let missing_type, g = match pt.typ with
              | Class { c_cache = ("generator", _); c_generics = [_, (_, t)]; _ } ->
                t, true
              | _ -> pt, g
            in
            Ctx.add_block ~ctx;
            Ctx.add ~ctx "..." (C.Type missing_type);
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
        if is_gen then R.internal ~ctx ~args:[fst last_expr] "generator"
        else fst last_expr
      in
      C.ann ~typ ctx, Pipe exprs

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

  and bop2magic ?(prefix = "") op =
    let op =
      match op with
      | "+" -> "add"
      | "-" -> "sub"
      | "*" -> "mul"
      | "/" -> "truediv"
      | "//" -> "floordiv"
      | "**" -> "pow"
      | "%" -> "mod"
      | "@" -> "mathmul"
      | "==" -> "eq"
      | "!=" -> "ne"
      | "<" -> "lt"
      | ">" -> "gt"
      | "<=" -> "le"
      | ">=" -> "ge"
      | "&" -> "and"
      | "|" -> "or"
      | "^" -> "xor"
      | "<<" -> "lshift"
      | ">>" -> "rshift"
      | "in" -> "contains"
      (* patches for C++ transform *)
      (* | "&&" -> "and" *)
      (* | "||" -> "or" *)
      | s -> s
    in
    sprintf "__%s%s__" prefix op

  and uop2magic op =
    let op =
      match op with
      | "+" -> "pos"
      | "-" -> "neg"
      | "~" -> "invert"
      | "not" -> "not"
      | s -> s
    in
    sprintf "__%s__" op
end
