(* ****************************************************************************
 * Seqaml.Typecheck_realize: Realize Seq nodes
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Err
open Option.Monad_infix

(** This module implements [Typecheck_intf.Expr].
    Parametrized by [Typecheck_intf.Stmt] for parsing generators ([parse_for] and [finalize]) *)
module Typecheck (E : Typecheck_intf.Expr) (S : Typecheck_intf.Stmt) : Typecheck_intf.Real = struct
  open Ast
  module C = Typecheck_ctx
  module T = Typecheck_infer

  let rec realize ~(ctx : C.t) (typ : Ann.tvar) =
    match ctx.env.realizing, typ with
    | true, Func f -> realize_function ctx typ f
    | true, Class c -> realize_type ctx typ c
    | (true | false), t -> t

  and realize_function ctx typ (fn, f_ret) =
    let real_name, parents = C.get_full_name ~ctx typ in
    let ast, str2real = Hashtbl.find_exn ctx.globals.realizations fn.cache in
    match Hashtbl.find str2real real_name with
    (* Case 1 - already realized *)
    | Some { realized_typ; _ } ->
      T.link_to_parent ~parent:(List.hd parents) (Ann.var_of_typ_exn realized_typ.typ)
    (* Case 2 - needs realization *)
    | None ->
      Util.dbg
        "[real] realizing fn %s ==> %s [%s -> %s]"
        fn.name
        real_name
        (Option.value_map
           ~f:(fun t -> fst @@ C.get_full_name ~ctx t)
           (snd fn.parent)
           ~default:"<n/a>")
        (Option.value_map
           ~f:(fun t -> fst @@ C.get_full_name ~ctx t)
           (List.hd parents)
           ~default:"<n/a>");
      let tv = Ann.Func ({ fn with parent = fst fn.parent, List.hd parents }, f_ret) in
      let typ = C.ann ~typ:(Var tv) ctx in
      let fn_stmts =
        match snd ast with
        | Function ast -> ast.fn_stmts
        | _ -> []
      in
      let cache_entry =
        C.{ realized_ast = None; realized_typ = typ; realized_llvm = Ctypes.null }
      in
      Hashtbl.set str2real ~key:real_name ~data:cache_entry;
      let enclosing_return = ref (Some f_ret.ret) in
      let env = { ctx.env with enclosing_return; realizing = true; unbounds = Stack.create () } in
      let fctx = { ctx with env } in
      Ctx.add_block ~ctx:fctx;
      T.traverse_parents ~ctx:fctx tv ~f:(fun n (_, t) -> Ctx.add ~ctx n (Type t));
      (* Ensure that all arguments of recursive realizations are fully
         specified--- otherwise an infinite recursion will ensue *)
      let is_being_realized = Hashtbl.find fctx.env.being_realized fn.cache in
      let fn_args =
        List.map fn.args ~f:(fun (name, fn_t) ->
            if Ann.has_unbound fn_t && is_some is_being_realized
            then C.err ~ctx "function arguments must be realized within recursive realizations";
            (* Util.A.dg "adding %s = %s" name (fst@@C.get_full_name fn_t ~ctx:fctx); *)
            Ctx.add ~ctx:fctx name (Var fn_t);
            Stmt.{ name; typ = None (* no need for this anymore *) })
      in
      let fn_name = Stmt.{ name = fn.name; typ = None } in
      C.add_realization ~ctx:fctx fn.cache typ;
      let fn_stmts = S.parse_realized ~ctx:(C.enter_level ~ctx:fctx) fn_stmts in
      C.remove_last_realization ~ctx:fctx fn.cache;
      Ctx.clear_block ~ctx:fctx;
      let ast =
        ( typ
        , match snd ast with
          | Function f -> Stmt.Function { f with fn_args; fn_stmts; fn_name }
          | e -> e )
      in
      let cache_entry = { cache_entry with realized_ast = Some ast } in
      Hashtbl.set str2real ~key:real_name ~data:cache_entry;
      (* set another alias in case typ changed during the realization *)
      let real_name, _ = C.get_full_name ~ctx tv in
      Hashtbl.set str2real ~key:real_name ~data:cache_entry;
      tv

  and realize_type ctx typ (cls, cls_t) =
    let real_name, parents = C.get_full_name ~ctx typ in
    let ast, str2real = Hashtbl.find_exn ctx.globals.realizations cls.cache in
    match Hashtbl.find str2real real_name with
    (* Case 1 - already realized *)
    | Some { realized_typ; _ } ->
      (* ensure that parent pointer is kept correctly *)
      T.link_to_parent ~parent:(List.hd parents) (Ann.var_of_typ_exn realized_typ.typ)
    (* Case 2 - needs realization *)
    | None ->
      Util.dbg "[real] realizing class %s ==> %s" cls.name real_name;
      let c_args =
        Hashtbl.find_exn ctx.globals.classes cls.cache
        |> Hashtbl.to_alist
        |> List.filter_map ~f:(function
               | key, [ Ann.Var (Func _) ] -> None
               | key, [ Var c ] -> Some (c, (key, C.make_unbound ctx))
               | _ -> None)
      in
      let tv =
        Ann.Class
          ( { cls with parent = fst cls.parent, List.hd parents; args = List.map ~f:snd c_args }
          , cls_t )
      in
      let typ = C.ann ~typ:(Type tv) ctx in
      let cache_entry =
        C.{ realized_ast = None; realized_typ = typ; realized_llvm = Ctypes.null }
      in
      Hashtbl.set str2real ~key:real_name ~data:cache_entry;
      Ctx.add_block ~ctx;
      let inst = Int.Table.create () in
      T.traverse_parents ~ctx tv ~f:(fun n (i, t) ->
          Hashtbl.set inst ~key:i ~data:t;
          (* Util.A.dr "%s -> %d, %s" n i (Ann.var_to_string t); *)
          Ctx.add ~ctx n (Type t));
      C.add_realization ~ctx cls.cache typ;
      List.iter c_args ~f:(fun (c, (x, t')) ->
          (* Util.A.dr ":: [%s] %s -> %s" n (Ann.typ_to_string ~full:true t) (Ann.typ_to_string ~full:true @@ T.instantiate ~ctx ~inst t); *)
          T.instantiate ~ctx ~inst c |> realize ~ctx |> T.unify_inplace ~ctx t');
      C.remove_last_realization ~ctx cls.cache;
      Ctx.clear_block ~ctx;
      let ast = typ, snd ast in
      let cache_entry = { cache_entry with realized_ast = Some ast } in
      Hashtbl.set str2real ~key:real_name ~data:cache_entry;
      (* if snd cls.cache = Ann.default_pos then (
        let open Llvm.Type in
        let name = fst cls.cache in
        ( match name with
          | "void" -> void ()
          | "int" -> int ()
          | "bool" -> bool ()
          | "float" -> float ()
          | "byte" -> byte ()
          | "str" -> str ()
          | "seq" -> seq ()
          | "ptr" -> ptr [typ]
          | "array" -> ptr [typ]
          | "KInt" -> kmerN n
          | "Int" -> intN n
          | "UInt" -> uintN n
          | _ -> Ctypes.null )
        |> get_methods
        |> List.iter ~f:(fun (s, t) -> add_internal_method name s (get_name t))
      ); *)
      tv

  and internal ~(ctx : C.t) ?(args = []) name =
    let what =
      Ctx.in_scope ~ctx name
      >>= function
      | Type t -> Some (Ann.real_type t)
      | _ -> None
    in
    match what with
    | Some (Class ({ cache; generics; _ }, _) as typ) ->
      let inst =
        Int.Table.of_alist_exn @@ List.map2_exn generics args ~f:(fun (_, (i, _)) a -> i, a)
      in
      let typ = T.instantiate ~ctx ~inst typ |> realize ~ctx in
      let _ = magic ~ctx typ "__init__" in
      if name = "list" then ignore @@ magic ~ctx typ "append" ~args;
      typ
    | _ -> C.err ~ctx "cannot find internal type %s" name

  and magic ~(ctx : C.t) ?(idx = 0) ?(args = []) typ name =
    (* Util.A.dy "MAGIC %s.%s :: [%s]" (Ann.typ_to_string typ) name (Util.ppl args ~f:Ann.typ_to_string); *)
    let ret =
      match Ann.real_type typ, name with
      | Tuple el, "__getitem__" -> List.nth el idx
      | Class ({ cache; _ }, _), _ ->
        Hashtbl.find ctx.globals.classes cache
        >>= fun h ->
        Hashtbl.find h name
        >>| List.filter_map ~f:(function
                | Ann.(Type _ | Import _) -> C.err ~ctx "wrong magic type"
                | Var t ->
                  (match Ann.real_type t |> T.instantiate ~ctx with
                  | Func (f, _) as t when List.(length f.args = 1 + length args) ->
                    (* Util.A.dy "IN: %s.%s [%s] <~> %s" (Ann.var_to_string typ) name
                       (Util.ppl (typ::args) ~f:Ann.var_to_string)
                       (Ann.var_to_string ~full:true t); *)
                    let f_args = List.map f.args ~f:(fun f -> snd f) in
                    let in_args = List.map (typ::args) ~f:T.copy in
                    (* List.iter2_exn in_args f_args ~f:(fun i j ->
                      Util.A.dy "%s.%s :: %s vs %s" (Ann.var_to_string typ) name (Ann.var_to_string ~full:true i) (Ann.var_to_string ~full:true j)); *)
                    let s = T.sum_or_neg in_args f_args ~f:(T.unify ~on_unify:T.unify_merge) in
                    (* Util.A.dy "RESULT: %s.%s :: %s (%d)" (Ann.var_to_string typ) name (Ann.var_to_string t) s; *)
                    if s = -1 then None else Some ((f.cache, t), s)
                  | _ -> None))
        >>| List.fold ~init:(None, -1) ~f:(fun (acc, cnt) cur ->
                match acc, cur with
                | None, cur -> Some cur, 1
                | Some (c, max), (ci, i) when i = max -> acc, succ cnt
                | Some (_, max), (_, i) when i > max -> Some cur, 1
                | Some _, _ -> acc, cnt)
        >>= (function
        | Some ((_, t), _), 1 ->
          (match T.link_to_parent ~parent:(Some typ) t with
          | Func (f, fret) as t ->
            List.iter2_exn (typ :: args) f.args ~f:(fun t (_, t') -> T.unify_inplace ~ctx t t');
            (match realize ~ctx (Func (f, fret)) with
            (* (match realize_function ctx t (f, fret) with *)
              | Func (_, f_ret) -> Some f_ret.ret
              | _ -> failwith "cannot happen")
          | _ -> ierr ~ctx "got non-function")
        | Some ((_, t), _), j ->
          (* many choices *)
          let hasu = Ann.has_unbound typ in
          let hasu = hasu || List.exists args ~f:Ann.has_unbound in
          if hasu then Some (C.make_unbound ctx) else None
        (* C.err ~ctx "many equally optimal magic functions" *)
        | None, _ -> None (* C.err ~ctx "cannot find fitting magic function") *))
      | _ -> None
    in
    ret

  (* C.err ~ctx "can't find magic %s in %s for args %s" name (Ann.typ_to_string typ) (Util.ppl args ~f:Ann.typ_to_string) *)
end
