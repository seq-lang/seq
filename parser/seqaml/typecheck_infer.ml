(* ****************************************************************************
 * Seqaml.Typecheck_infer: Type checking inference algorithms
 * http://okmij.org/ftp/ML/generalization.html
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Util
open Err
open Ast
open Option.Monad_infix
module C = Typecheck_ctx

(* Utilities *)

(* let is_class ~(ctx : C.t) cache =
  match Hashtbl.find ctx.globals.realizations cache >>| fst with
  | Some (_, Stmt.Class _) -> true
  | _ -> false *)

let link_to_parent ~parent t =
  let m t p =
    let t = Ann.real_type t in
    match t with
      | Class (g, a) when p = fst g.parent ->
        Ann.Class ({ g with parent = (p, parent) }, a)
      | Func (g, a) when p = fst g.parent ->
        Func ({ g with parent = (p, parent) }, a)
      | t -> t
  in
  match t, parent with
  | t, Some (Func (g, _) | Class (g, _)) ->
    m t g.cache
  | t, _ -> t

let rec traverse_parents ~ctx ~f t =
  match Ann.real_type t with
  | Class (g, _) | Func (g, _) ->
    (* this means that non-realized classes in the chain will get not unified
       make sure that their instantiation is unique (ie not linked to sth else) *)
    (snd g.parent) >>| traverse_parents ~f ~ctx |> ignore;
    List.iter g.generics ~f:(fun (n, t) -> f n t)
  | _ -> ()

(* Algorithms *)

(** Checks whether an unbound variable [u] is referenced
    within a type [t] that is to be unified  *)
let rec occurs ~if_unify ((id, level, _) as u) t =
  let occurs = occurs ~if_unify u in
  match t with
  | Ann.Link { contents = Bound t' } -> occurs t'
  | Link { contents = Unbound (i', _, _) } when i' = id -> true
  | Link ({ contents = Unbound _ } as tv') -> if_unify level tv'; false
  | Class ({ generics; _ }, _) ->
    List.exists generics ~f:(fun (_, (_, v)) -> occurs v)
  | Func ({ generics; args; _}, { ret; _ }) ->
    List.exists generics ~f:(fun (_, (_, v)) -> occurs v)
    && List.exists args ~f:(fun (_, v) -> occurs v)
    && occurs ret
  | Tuple args -> List.exists args ~f:occurs
  | _ -> false

(** Unifies types [t1] and [t2] *)
let sum_or_neg ~f g1 g2 =
  match List.zip g1 g2 with
  | Unequal_lengths -> -1
  | Ok s ->
    List.fold_until
      s
      ~init:0
      ~f:(fun sum (x1, x2) ->
        let s = f x1 x2 in
        if s = -1 then Stop (-1) else Continue (sum + s))
      ~finish:(fun sum -> sum)

let rec unify ?(on_unify = ignore2) t1 t2 =
  let u = unify ~on_unify in
  match t1, t2 with
  | t1, t2 when t1 = t2 -> 1
  | Ann.Class (c1, t1), Ann.Class (c2, t2) when c1.name = c2.name && t1 = t2 ->
    let s = sum_or_neg c1.generics c2.generics
        ~f:(fun (n1, (_, t1)) (n2, (_, t2)) -> if n1 <> n2 then -1 else u t1 t2)
    in
    if s = -1 then -1 else 1 + s
  | Func (f1, r1), Func (f2, r2) when f1.name = f2.name ->
    let s1 = sum_or_neg f1.generics f2.generics
        ~f:(fun (n1, (_, t1)) (n2, (_, t2)) -> if n1 <> n2 then -1 else u t1 t2)
    in
    if s1 = -1
    then -1
    else (
      let s2 =
        sum_or_neg f1.args f2.args ~f:(fun (n1, t1) (n2, t2) -> if n1 <> n2 then -1 else u t1 t2)
      in
      if s1 = -1
      then -1
      else (
        let s3 = u r1.ret r2.ret in
        if s3 = -1 then -1 else 1 + s1 + s2 + s3))
  | Tuple t, Class (_, { types = Some c }) | Class (_, { types = Some c }), Tuple t ->
    1 + sum_or_neg ~f:u t c
  | Tuple a1, Tuple a2 when List.length a1 = List.length a2 ->
    1 + sum_or_neg ~f:u a1 a2
  | Link { contents = Bound t1 }, t2 | t1, Link { contents = Bound t2 } -> u t1 t2
  | Link { contents = Generic _ } , _ | _, Link { contents = Generic _ } ->
    -1
  | (Link ({ contents = Unbound u' } as tv') as t'), t
  | t, (Link ({ contents = Unbound u' } as tv') as t') ->
    let occ =
      occurs u' t ~if_unify:(fun level -> function
        | { contents = Ann.Unbound (i', l', g') } as tv' ->
          if l' > level then tv' := Unbound (i', level, g')
        | _ -> ())
    in
    if not occ
    then (
      ( match t with
        | Link ({ contents = Unbound (i, _, _) } as tv) when i > fst3 u' ->
          on_unify t t'
        | _ -> on_unify t' t );
      0)
    else -1
  | _ -> -1

let unify_inplace ~ctx t1 t2 =
  (* Util.A.dr "%s <- %s"
    (Ann.var_to_string ~full:true t1) (Ann.var_to_string ~full:true t2); *)
  let u =
    unify t1 t2 ~on_unify:(fun lht rht ->
        match lht with
        | Link { contents = Unbound (i, _, true) } ->
          (* TODO: this is very restricted check: what if unbound->generic->unbound (then it is false?) *)
          Util.dbg "[WARN] attempting to restrict generic T%d" i
            (* (Ann.typ_to_string lht) *)
            (* (Ann.typ_to_string rht) *)
        | Link ({ contents = Unbound _ } as l) ->
          (* Util.dbg "[unify] %s := %s" (Ann.var_to_string ~full:true lht) (Ann.var_to_string ~full:true rht); *)
          l := Bound rht
        | _ -> ierr "[unify_inplace] cannot unify non-unbound type")
  in
  if u = -1
  then
    C.err ~ctx "cannot unify %s and %s" (Ann.var_to_string t1) (Ann.var_to_string t2)

(** Quantifies (generalizes) any unbound variable whose rank > [level]:
    i.e. replaces ɑ with ∀ɑ.ɑ *)
let rec generalize ~level t =
  let g = generalize ~level in
  let rec f = function
    | Ann.Class (c, t) ->
      let generics = List.map c.generics ~f:(fun (n, (p, t)) -> n, (p, g t)) in
      Ann.Class ({ c with generics }, t)
    | Func (f, r) ->
      let generics = List.map f.generics ~f:(fun (n, (p, t)) -> n, (p, g t)) in
      let args = List.map f.args ~f:(fun (n, t) -> n, g t) in
      let ret = g r.ret in
      Func ({ f with generics; args }, { r with ret })
    | Tuple args -> Tuple (List.map args ~f:g)
    | Link { contents = Bound t } -> f t
    | Link { contents = Unbound (id, l, g) } when l > level ->
      Link { contents = Generic id }
    | t -> t
  in
  f t

let rec generalize_inplace ~(ctx : C.t) t =
  let g = generalize_inplace ~ctx in
  let rec f = function
    | Ann.Class (c, _) ->
      List.iter c.generics ~f:(fun (_, (_, t)) -> g t)
    | Func (f, r) ->
      List.iter f.generics ~f:(fun (_, (_, t)) -> g t);
      List.iter f.args ~f:(fun (_, t) -> g t);
      g r.ret
    | Tuple args -> List.iter args ~f:g
    | Link ({ contents = Unbound (id, l, g) } as tv) when l > ctx.env.level ->
      tv := Generic id
    | Link { contents = Bound t } -> f t
    | t -> ()
  in
  f t

(** Instantiates (specializes) any generic variable with an unbound at
    the current level: i.e. replaces ∀ɑ.ɑ with ɑ *)
let instantiate ~(ctx : C.t) ?(inst = Int.Table.create ()) ?parent t =
  let parents = String.Table.create () in
  let rec f = function
    | Ann.Link { contents = Bound t } -> f t
    | Link { contents = Generic g } ->
      ( match Hashtbl.find inst g with
        | Some s -> s
        | None ->
          let t' = C.make_unbound ctx in
          Hashtbl.set inst ~key:g ~data:t';
          t' )
    | Class (c, t) ->
      Class ({ c with
              generics = List.map c.generics ~f:(fun (n, (p, t)) -> n, (p, f t))
            }, t)
    | Func (fn, r) ->
      Func ({ fn with
              generics = List.map fn.generics ~f:(fun (n, (p, t)) -> n, (p, f t))
            ; args = List.map fn.args ~f:(fun (n, t) -> n, f t)
            },
            { r with ret = f r.ret })
    | Tuple args -> Tuple (List.map args ~f)
    | t -> t
  in
  let parent =
    match parent, Ann.real_type t with
    | Some _, _ -> parent
    | None, (Class (g, _) | Func (g, _)) -> snd g.parent
    | _ -> None
  in
  parent
  >>| traverse_parents ~ctx ~f:(fun n (key, d) ->
      Hashtbl.set inst ~key ~data:d)
  |>  ignore;
  f t
