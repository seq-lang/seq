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
module Typecheck (E : Typecheck_intf.Stmt) (S : Typecheck_intf.Stmt) : Typecheck_intf.Real = struct
  open Ast

  module C = Typecheck_ctx
  module T = Typecheck_infer
  module R = Typecheck_realize

  let rec realize ~(ctx: C.t) (typ: Ann.t) =
    match ctx.env.realizing, Ann.real_type typ with
    | true, ({ typ = Func f; _ } as typ) ->  realize_function ctx typ f
    | true, ({ typ = Class c; _ } as typ) -> realize_type ctx typ c
    | false, _ | true, _ -> typ

  and realize_function ctx typ fn =
    let real_name, parents = C.get_full_name ~ctx typ in
    let ast, str2real = Hashtbl.find_exn ctx.globals.realizations fn.f_cache in
    let what =
      Ctx.in_scope ~ctx fn.f_name
      >>= function Var t -> Some (Ann.real_type t) | _ -> None
    in
    match Hashtbl.find str2real real_name, what with
    (* Case 1 - already realized *)
    | Some { realized_typ; _ }, _ ->
      Ann.link_to_parent ~parent:(List.hd parents) realized_typ
    (* Case 2 - needs realization *)
    | None, Some { typ = Func _; _ } ->
      Util.dbg "[real] realizing fn %s ==> %s [%s -> %s]" fn.f_name real_name
        (Option.value_map ~f:Ann.typ_to_string (snd fn.f_parent) ~default:"<n/a>")
        (Option.value_map ~f:Ann.typ_to_string (List.hd parents) ~default:"<n/a>");

      let typ = { typ with typ =
                  Func { fn with f_parent = fst fn.f_parent, (List.hd parents) } }
      in
      let fn_stmts = match snd ast with
        | Function ast -> ast.fn_stmts
        | Extern _ -> []
        | _ -> ierr "wrong ast for %s" real_name
      in
      let cache_entry = C.
        { realized_ast = None
        ; realized_typ = typ
        ; realized_llvm = Ctypes.null
        }
      in
      Hashtbl.set str2real ~key:real_name ~data:cache_entry;
      let enclosing_return = ref fn.f_ret in
      let env = { ctx.env with enclosing_return; realizing = true; unbounds = Stack.create () } in
      let fctx = { ctx with env } in
      Ctx.add_block ~ctx:fctx;
      T.traverse_parents ~ctx:fctx typ ~f:(fun n (_, t) -> Ctx.add ~ctx n (Type t));
      (* Ensure that all arguments of recursive realizations are fully
         specified--- otherwise an infinite recursion will ensue *)
      let is_being_realized = Hashtbl.find fctx.env.being_realized fn.f_cache in
      let fn_args = List.map fn.f_args ~f:(fun (name, fn_t) ->
        if Ann.has_unbound fn_t && is_some is_being_realized then
          C.err ~ctx "function arguments must be realized within recursive realizations";
        Ctx.add ~ctx:fctx name (Var fn_t);
        Stmt.{ name; typ = None (* no need for this anymore *) })
      in
      let fn_name = Stmt.{ name = fn.f_name; typ = None } in

      C.add_realization ~ctx:fctx fn.f_cache typ;
      let fn_stmts = S.parse_realized ~ctx:(C.enter_level ~ctx:fctx) fn_stmts in
      C.remove_last_realization ~ctx:fctx fn.f_cache;
      Ctx.clear_block ~ctx:fctx;

      let ast = typ, match snd ast with
        | Function f -> Stmt.Function { f with fn_args; fn_stmts; fn_name }
        | e -> e
      in
      let cache_entry = { cache_entry with realized_ast = Some ast } in
      Hashtbl.set str2real ~key:real_name ~data:cache_entry;
      (* set another alias in case typ changed during the realization *)
      let real_name, _ = C.get_full_name ~ctx typ in
      Hashtbl.set str2real ~key:real_name ~data:cache_entry;
      typ
    | _ -> C.err ~ctx "impossible realization case for %s" fn.f_name

  and realize_type ctx typ cls =
    let real_name, parents = C.get_full_name ~ctx typ in
    let ast, str2real = Hashtbl.find_exn ctx.globals.realizations cls.c_cache in
    let what =
      Ctx.in_scope ~ctx cls.c_name
      >>= function Var t -> Some (Ann.real_type t) | _ -> None
    in
    match Hashtbl.find str2real real_name, what with
    (* Case 1 - already realized *)
    | Some { realized_typ; _ }, _ ->
      (* ensure that parent pointer is kept correctly *)
      Ann.link_to_parent ~parent:(List.hd parents) realized_typ
    (* Case 2 - needs realization *)
    | None, Some { typ = Class _; _ } ->
      Util.dbg "[real] realizing class %s ==> %s" cls.c_name real_name;

      let c_args =
        Hashtbl.find_exn ctx.globals.classes cls.c_cache
        |>  Hashtbl.to_alist
        |>  List.filter_map ~f:(function
            | (key, [C.Member _]) as c -> Some (c, (key, C.make_unbound ctx))
            | _ -> None)
      in
      let typ = { typ with typ = Class
          { cls with c_parent = fst cls.c_parent, (List.hd parents)
          ; c_args = List.map ~f:snd c_args }
        }
      in
      let cache_entry = C.
        { realized_ast = None
        ; realized_typ = typ
        ; realized_llvm = Ctypes.null
        }
      in
      Hashtbl.set str2real ~key:real_name ~data:cache_entry;

      Ctx.add_block ~ctx;
      let inst = Int.Table.create () in
      T.traverse_parents ~ctx typ ~f:(fun n (i, t) ->
        Hashtbl.set inst ~key:i ~data:t;
        Ctx.add ~ctx n (Type t));
      C.add_realization ~ctx cls.c_cache typ;
      List.iter c_args ~f:(fun ((n, tk), (x, t')) ->
        match tk with
        | [Member t] ->
          T.instantiate ~ctx ~inst t |> realize ~ctx |> T.unify_inplace ~ctx t'
        | _ -> ());
      C.remove_last_realization ~ctx cls.c_cache;
      Ctx.clear_block ~ctx;

      let ast = typ, snd ast in
      let cache_entry = { cache_entry with realized_ast = Some ast } in
      Hashtbl.set str2real ~key:real_name ~data:cache_entry;
      typ
    | _ -> C.err ~ctx "cannot find %s to realize" cls.c_name

  and internal ~(ctx: C.t) ?(args=[]) name =
    let what =
      Ctx.in_scope ~ctx name
      >>= function Type t -> Some (Ann.real_type t) | _ -> None
    in
    match what with
    | Some ({ typ = Class { c_cache; c_generics; _ }; _ } as typ) ->
      let inst = Int.Table.of_alist_exn @@
        List.map2_exn c_generics args ~f:(fun (_, (i, _)) a -> (i, a))
      in
      let typ = T.instantiate ~ctx ~inst typ |> realize ~ctx in
      let _ = magic ~ctx typ "__init__" in
      if name = "list" then ignore @@ magic ~ctx typ "append" ~args;
      typ
    | _ -> C.err ~ctx "cannot find internal type %s" name

  and magic ~(ctx: C.t) ?(idx=0) ?(args=[]) typ name =
    let ret = match (Ann.real_type typ).typ, name with
      | Tuple el, "__getitem__" -> List.nth el idx
      | Class { c_cache; _ }, _ ->
        Hashtbl.find ctx.globals.classes c_cache
        >>= fun h ->
            Hashtbl.find h name
        >>| List.filter_map ~f:(function
            | C.(Type _ | Member _ | Import _) ->
              C.err ~ctx "wrong magic type"
            | C.Var t ->
              match (Ann.real_type t) with
              | { typ = Func f; _ } as t ->
                let ti = T.instantiate ~ctx t in
                let f_args = List.map (Ann.func ti).f_args ~f:snd in
                let s = T.sum_or_neg (typ :: args) f_args ~f:T.unify in
                if s = -1 then None else Some ((f.f_cache, ti), s)
              | _ -> None)
        >>= List.max_elt ~compare:(fun (_, i) (_, j) -> Int.compare i j)
        >>| fun ((_, t), _) ->
            let t = Ann.link_to_parent ~parent:(Some typ) t in
            let f = Ann.func t in
            List.iter2_exn (typ :: args) f.f_args
              ~f:(fun t (_, t') -> T.unify_inplace ~ctx t t');
            let f = Ann.func (realize ~ctx t) in
            f.f_ret
      | _ -> None
    in
    match ret, ctx.env.realizing with
    | Some t, _ -> Some t
    | None, false -> Some (C.make_unbound ctx)
    | None, true -> None
    (* C.err ~ctx "can't find magic %s in %s for args %s" name (Ann.typ_to_string typ) (Util.ppl args ~f:Ann.typ_to_string) *)
end