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

let is_class ~(ctx : C.t) cache =
  match Hashtbl.find ctx.globals.realizations cache >>| fst with
  | Some (_, Stmt.Class _) -> true
  | _ -> false

let rec traverse_parents ~ctx ~f t =
  match (Ann.real_type t).typ with
  | Class { c_generics = gen; c_parent = p, pt; _ }
  | Func { f_generics = gen; f_parent = p, pt; _ } ->
    (* this means that non-realized classes in the chain will get not unified
       make sure that their instantiation is unique (ie not linked to sth else) *)
    pt >>| traverse_parents ~f ~ctx |> ignore;
    List.iter gen ~f:(fun (n, t) -> f n t)
  | _ -> ()

(* Algorithms *)

(** Checks whether an unbound variable [u] is referenced
    within a type [t] that is to be unified  *)
let rec occurs ~if_unify ((id, level, _) as u) (t : Ann.t) =
  let occurs = occurs ~if_unify in
  match t.typ with
  | TypeVar { contents = Bound t' } -> occurs u t'
  | TypeVar { contents = Unbound (i', _, _) } when i' = id -> true
  | TypeVar ({ contents = Unbound _ } as tv') ->
    if_unify level tv';
    false
  | Class { c_generics; _ } -> List.exists c_generics ~f:(fun (_, (_, v)) -> occurs u v)
  | Func { f_generics; f_args; f_ret; _ } ->
    List.exists f_generics ~f:(fun (_, (_, v)) -> occurs u v)
    && List.exists f_args ~f:(fun (_, v) -> occurs u v)
    && occurs u f_ret
  | Tuple args -> List.exists args ~f:(occurs u)
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

let rec unify ?(on_unify = ignore2) (t1 : Ann.t) (t2 : Ann.t) =
  let u = unify ~on_unify in
  match t1, t2 with
  | t1, t2 when t1 = t2 -> 1
  | ( { typ = Class { c_name = n1; c_generics = g1; c_type = i1; _ }; _ }
    , { typ = Class { c_name = n2; c_generics = g2; c_type = i2; _ }; _ } )
    when n1 = n2 && i1 = i2 ->
    let s =
      sum_or_neg g1 g2 ~f:(fun (n1, (_, t1)) (n2, (_, t2)) ->
          if n1 <> n2 then -1 else u t1 t2)
    in
    if s = -1 then -1 else 1 + s
  | ( { typ = Func { f_name = n1; f_generics = g1; f_args = a1; f_ret = r1; _ }; _ }
    , { typ = Func { f_name = n2; f_generics = g2; f_args = a2; f_ret = r2; _ }; _ } )
    when n1 = n2 ->
    let s1 =
      sum_or_neg g1 g2 ~f:(fun (n1, (_, t1)) (n2, (_, t2)) ->
          if n1 <> n2 then -1 else u t1 t2)
    in
    if s1 = -1
    then -1
    else (
      let s2 =
        sum_or_neg a1 a2 ~f:(fun (n1, t1) (n2, t2) -> if n1 <> n2 then -1 else u t1 t2)
      in
      if s1 = -1
      then -1
      else (
        let s3 = u r1 r2 in
        if s3 = -1 then -1 else 1 + s1 + s2 + s3))
  | { typ = Tuple t; _ }, { typ = Class { c_type = Some c; _ }; _ }
  | { typ = Class { c_type = Some c; _ }; _ }, { typ = Tuple t; _ } ->
    1 + sum_or_neg ~f:u t c
  | { typ = Tuple a1; _ }, { typ = Tuple a2; _ } when List.length a1 = List.length a2
    ->
    1 + sum_or_neg ~f:u a1 a2
  | { typ = TypeVar { contents = Generic _ }; _ }, _
  | _, { typ = TypeVar { contents = Generic _ }; _ } ->
    -1
  | { typ = TypeVar { contents = Bound t1 }; _ }, t2
  | t1, { typ = TypeVar { contents = Bound t2 }; _ } ->
    u t1 t2
  | ({ typ = TypeVar ({ contents = Unbound u' } as tv'); _ } as t'), t
  | t, ({ typ = TypeVar ({ contents = Unbound u' } as tv'); _ } as t') ->
    let occ =
      occurs u' t ~if_unify:(fun level -> function
        | { contents = Unbound (i', l', g') } as tv' ->
          if l' > level then tv' := Unbound (i', level, g')
        | _ -> ())
    in
    if not occ
    then (
      match t with
      | { typ = TypeVar ({ contents = Unbound (i, _, _) } as tv); _ } when i > fst3 u'
        ->
        on_unify t t' (* t := t' *)
      (* (upos, tv) t' *)
      | _ -> on_unify t' t (* on_unify (upos', tv') t *));
    if not occ then 0 else -1
  | _ -> -1

let unify_inplace ~ctx t1 t2 =
  let u =
    unify t1 t2 ~on_unify:(fun lht rht ->
        match lht.typ with
        | TypeVar { contents = Unbound (i, _, true) } ->
          (* TODO: this is very restricted check: what if unbound->generic->unbound (then it is false?) *)
          Util.dbg
            "[WARN] attempting to restring generic T%d (%s) to %s"
            i
            (Ann.typ_to_string lht)
            (Ann.typ_to_string rht)
        | TypeVar ({ contents = Unbound _ } as l) ->
          (* u := pos :: !u;  *)
          l := Bound rht
        | _ -> ierr "[unify_inplace] cannot unify non-unbound type")
  in
  if u = -1
  then
    C.err ~ctx "cannot unify %s and %s" (Ann.typ_to_string t1) (Ann.typ_to_string t2)

(** Quantifies (generalizes) any unbound variable whose rank > [level]:
    i.e. replaces ɑ with ∀ɑ.ɑ *)
let rec generalize ~ctx ~level (t : Ann.t) =
  let g = generalize ~ctx ~level in
  match t.typ with
  | Class ({ c_generics; _ } as c) ->
    let c_generics = List.map c_generics ~f:(fun (n, (p, t)) -> n, (p, g t)) in
    { t with typ = Class { c with c_generics } }
  | Func ({ f_generics; f_args; f_ret; _ } as fn) ->
    let f_generics = List.map f_generics ~f:(fun (n, (p, t)) -> n, (p, g t)) in
    let f_args = List.map f_args ~f:(fun (n, t) -> n, g t) in
    let f_ret = g f_ret in
    { t with typ = Func { fn with f_generics; f_args; f_ret } }
  | Tuple args -> { t with typ = Tuple (List.map args ~f:g) }
  | TypeVar { contents = Bound t } -> generalize ~ctx ~level t
  | TypeVar { contents = Unbound (id, l, g) } when l > level ->
    (* ref (pos :: !ann),  *)
    { t with typ = TypeVar { contents = Generic id } }
  | _ -> t

let rec generalize_inplace ~(ctx : C.t) (t : Ann.t) =
  let g = generalize_inplace ~ctx in
  match t.typ with
  | Class ({ c_generics; _ } as c) -> List.iter c_generics ~f:(fun (_, (_, t)) -> g t)
  | Func ({ f_generics; f_args; f_ret; _ } as fn) ->
    List.iter f_generics ~f:(fun (_, (_, t)) -> g t);
    List.iter f_args ~f:(fun (_, t) -> g t);
    g f_ret
  | Tuple args -> List.iter args ~f:g
  | TypeVar { contents = Bound t } -> g t
  | TypeVar ({ contents = Unbound (id, l, g) } as tv) when l > ctx.env.level ->
    (* ann := pos :: !ann; *)
    tv := Generic id
  | t -> ()

(** Instantiates (specializes) any generic variable with an unbound at
    the current level: i.e. replaces ∀ɑ.ɑ with ɑ *)
let instantiate ~(ctx : C.t) ?(inst = Int.Table.create ()) ?parent (t : Ann.t) =
  let parents = String.Table.create () in
  let rec f (t : Ann.t) =
    match t.typ with
    | TypeVar { contents = Bound t } -> f t
    | TypeVar { contents = Generic g } ->
      (match Hashtbl.find inst g with
      | Some s -> s
      | None ->
        let t' = C.make_unbound ctx in
        (* Util.A.db "instantiating generic %s -> %s..." (Ann.typ_to_string t) (Ann.typ_to_string ~full:true t'); *)
        (* fst t' := pos :: !(fst t'); *)
        Hashtbl.set inst ~key:g ~data:t';
        t')
    | Class c ->
      { t with
        typ =
          Class
            { c with
              c_generics = List.map c.c_generics ~f:(fun (n, (p, t)) -> n, (p, f t))
            }
      }
    | Func fn ->
      { t with
        typ =
          Func
            { fn with
              f_generics = List.map fn.f_generics ~f:(fun (n, (p, t)) -> n, (p, f t))
            ; f_args = List.map fn.f_args ~f:(fun (n, t) -> n, f t)
            ; f_ret = f fn.f_ret
            }
      }
    | Tuple args -> { t with typ = Tuple (List.map args ~f) }
    | _ -> t
  in
  let parent =
    match parent, (Ann.real_type t).typ with
    | Some _, _ -> parent
    | None, Class { c_parent = _, Some p; _ } | None, Func { f_parent = _, Some p; _ }
      ->
      Some p
    | _ -> None
  in
  parent
  >>| traverse_parents ~ctx ~f:(fun n (key, data) ->
      Hashtbl.set inst ~key ~data)
  |>  ignore;
  f t
