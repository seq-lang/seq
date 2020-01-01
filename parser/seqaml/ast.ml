(* *****************************************************************************
 * Seqaml.Ast: AST types and functions
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Util
open Ast_ann
open Option.Monad_infix

(** This module unifies all AST modules and extends them with
    various utility functions. *)

(** Alias for [Ast_ann]. Adds [to_string]. *)
module Ann = struct
  include Ast_ann

  let pos_to_string t =
    match t.file with
    | "/internal" -> "<internal>"
    | f -> sprintf "%s:%d:%d" (Filename.basename f) t.line t.col

  let rec var_to_string ?(generics = Int.Table.create ()) ?(useds=false) ?(full=false) t =
    let f = var_to_string ~generics ~useds ~full in
    let gen2str g = ppl ~sep:"," g ~f:(fun (_, (g, t)) -> sprintf "%s" (f t)) in
    match t with
    | Tuple args ->
      sprintf "tuple[%s]" (ppl ~sep:"," args ~f)
    | Func ({ generics; args; _ }, { ret; used }) ->
      let g = gen2str generics in
      sprintf
        "function%s((%s),%s)"
        (if g = "" then "" else sprintf "[%s]" g)
        (ppl ~sep:"," (List.filter args ~f:(fun (n, _) -> useds || not (Hash_set.exists used ~f:((=)n)) ))
          ~f:(fun (_, t) -> f t))
        (f ret)
    | Class ({ name; generics; _ }, _) ->
      let g = gen2str generics in
      sprintf "%s%s" name @@
        if g = "" then "" else sprintf "[%s]" g
    | Link { contents = Unbound (u, _, _) } ->
      if full then sprintf "'%d" u else "?"
    | Link { contents = Bound t } ->
      if full then sprintf "^%s" @@ f t else f t
    | Link { contents = Generic u } ->
      sprintf
        "T%d"
        (match Hashtbl.find generics u with
        | Some s -> s
        | None ->
          let w = succ @@ Hashtbl.length generics in
          Hashtbl.set generics ~key:u ~data:w;
          w)

  let typ_to_string ?(useds=false) ?(full=false) = function
    | Import s -> sprintf "<import: %s>" s
    | Type t ->
      if full then sprintf "<type: %s>" (var_to_string ~full ~useds t) else (var_to_string ~useds t)
    | Var t -> var_to_string ~full ~useds t

  let t_to_string ?(useds=false) ?(full=false) = function
    | None -> "_"
    | Some s -> typ_to_string ~full ~useds s

  let to_string t =
    sprintf "<%s |= %s>" (pos_to_string t.pos) (t_to_string t.typ)

  let create ?(file="/internal") ?(line=(-1)) ?(col=(-1)) ?(len=0) ?typ () =
    { pos = { file; line; col; len } ; typ }

  let rec real_type = function
    | Link { contents = Bound t } -> real_type t
    | t -> t

  let real_t t =
    match t with
    | None | Some (Import _) -> t
    | Some Type t  -> Some (Type (real_type t))
    | Some Var t -> Some (Var (real_type t))

  let is_type = function
    | { typ = Some (Type _) ; _ } -> true
    | _ -> false

  let patch t ~f =
    match t.typ with
    | Some (Type v) -> { t with typ = Some (Type (f v)) }
    | Some (Var v) -> { t with typ = Some (Var (f v)) }
    | _ -> t

  let rec has_unbound ?(count_generics=false) t =
    let f = has_unbound ~count_generics in
    match t with
    | Link { contents = Unbound _ } -> true
    | Link { contents = Bound t } -> f t
    | Link { contents = Generic _ } -> count_generics
    | Tuple el -> List.exists el ~f
    | Class ({ generics; _ }, _) ->
      List.exists generics ~f:(fun (_, (_, t)) -> f t)
    | Func ({ generics; args; _ }, { ret; _ }) ->
      (List.exists generics ~f:(fun (_, (_, t)) -> f t)) ||
      (List.exists args ~f:(fun (_, t) -> f t)) ||
      f ret

  let is_realizable t =
    let f = has_unbound ~count_generics:true in
    not (match t with
    | Func ({ generics; args; _ }, { ret; _ }) ->
      (List.exists generics ~f:(fun (_, (_, t)) -> f t)) ||
      (List.exists args ~f:(fun (_, t) -> f t))
      (* NOT THIS: function return is automatic!! || f ret *)
    | Class ({ generics; _ }, _) ->
      List.exists generics ~f:(fun (_, (_, t)) -> f t)
    | Tuple el -> List.exists el ~f
    | _ -> true)

  let var_of_typ t =
    match t with
    | None | Some (Import _) -> None
    | Some (Type t | Var t) -> Some t

  let var_of_typ_exn t =
    match var_of_typ t with Some s -> s | None -> failwith "invalid type"
end


(** Alias for [Ast_expr]. Adds [to_string]. *)
module Expr = struct
  include Ast_expr

  let rec to_string (enode : t ann) =
    match snd enode with
    | Empty _ -> ""
    | Ellipsis _ -> "..."
    | Bool x -> if x then "True" else "False"
    | Int (x, k) -> sprintf "%s%s" x k
    | Float (x, k) -> sprintf "%s%s" x k
    | String x -> sprintf "'%s'" (String.escaped x)
    | Kmer x -> sprintf "k'%s'" x
    | Seq x -> sprintf "s'%s'" x
    | Id x -> sprintf "%s" x
    | Unpack x -> sprintf "*%s" x
    | Tuple l when List.length l = 1 -> sprintf "(%s, )" (ppl l ~f:to_string)
    | Tuple l -> sprintf "(%s)" (ppl l ~f:to_string)
    | List l -> sprintf "[%s]" (ppl l ~f:to_string)
    | Set l -> sprintf "{%s}" (ppl l ~f:to_string)
    | Dict l ->
      List.chunks_of l ~length:2
      |> ppl ~f:(function
        | [a; b] -> sprintf "%s: %s" (to_string a) (to_string b)
        | _ -> failwith "cannot happen")
      |> sprintf "{%s}"
    | IfExpr (x, i, e) ->
      sprintf "%s if %s else %s" (to_string x) (to_string i) (to_string e)
    | Pipe l ->
      sprintf "%s" (ppl l ~sep:"" ~f:(fun (p, e) -> sprintf "%s %s" p @@ to_string e))
    | Binary (l, o, r) -> sprintf "(%s %s %s)" (to_string l) o (to_string r)
    | Unary (o, x) -> sprintf "(%s %s)" o (to_string x)
    | Index (x, l) -> sprintf "%s[%s]" (to_string x) (ppl l ~f:to_string)
    | Dot (x, s) -> sprintf "%s.%s" (to_string x) s
    | Call (x, l) -> sprintf "%s(%s)" (to_string x) (ppl l ~f:call_to_string)
    | TypeOf x -> sprintf "typeof(%s)" (to_string x)
    | Ptr x -> sprintf "ptr(%s)" (to_string x)
    | Slice (a, b, c) ->
      let l = List.map [ a; b; c ] ~f:(Option.value_map ~default:"" ~f:to_string) in
      sprintf "%s" (ppl l ~sep:":" ~f:Fn.id)
    | Lambda (l, x) -> sprintf "lambda (%s): %s" (ppl l ~f:Fn.id) (to_string x)
    | Generator (t, x, c) ->
      let c = comprehension_to_string c in
      match t, x with
      | "list", [x] -> sprintf "[%s %s]" (to_string x) c
      | "set", [x] -> sprintf "{%s %s}" (to_string x) c
      | "tuple", [x] -> sprintf "(%s %s)" (to_string x) c
      | "dict", [x; y] -> sprintf "{%s: %s %s}" (to_string x) (to_string y) c
      | _ -> failwith "invalid generator"

  and call_to_string { name; value } =
    sprintf
      "%s%s"
      (Option.value_map name ~default:"" ~f:(fun x -> x ^ " = "))
      (to_string value)

  and comprehension_to_string { var; gen; cond; next } =
    sprintf
      "for %s in %s%s%s"
      (ppl var ~f:Fn.id)
      (to_string gen)
      (Option.value_map cond ~default:"" ~f:(fun x -> sprintf "if %s" (to_string x)))
      (Option.value_map next ~default:"" ~f:(fun x -> " " ^ comprehension_to_string x))




  (** [walk ~f expr] walks through an AST node [expr] and calls
      [f expr] on each child that potentially contains an identifier [Id].
      Useful for locating all captured variables within [expr]. *)
  let rec walk ~f (pos, node) =
    let walk = walk ~f in
    let rec fg { var; gen; cond; next } =
      { var; gen = walk gen; cond = cond >>| walk; next = next >>| fg }
    in
    f (pos, match node with
      | Tuple l -> Tuple (List.map l ~f:walk)
      | List l -> List (List.map l ~f:walk)
      | Set l -> Set (List.map l ~f:walk)
      | Dict l -> Dict (List.map l ~f:walk)
      | Generator (t, g, tc) -> Generator (t, List.map g ~f:walk, fg tc)
      | IfExpr (a, b, c) -> IfExpr (walk a, walk b, walk c)
      | Unary (s, e) -> Unary (s, walk e)
      | Binary (e1, s, e2) -> Binary (walk e1, s, walk e2)
      | Pipe l -> Pipe (List.map l ~f:(fun (s, e) -> s, walk e))
      | Index (l, r) -> Index (walk l, List.map r ~f:walk)
      | Call (t, l) -> Call (walk t, List.map l
          ~f:(fun { name; value } -> { name; value = walk value }))
      | Slice (a, b, c) -> Slice (a >>| walk, b >>| walk, c >>| walk)
      | Dot (a, s) -> Dot (walk a, s)
      | TypeOf t -> TypeOf (walk t)
      | Ptr t -> Ptr (walk t)
      | Lambda (l, t) -> Lambda (l, walk t)
      | node -> node)
end

(** Alias for [Ast_stmt]. Adds [to_string]. *)
module Stmt = struct
  include Ast_stmt

  let rec to_string ?(indent = 0) (snode : t ann) =
    let s =
      match snd snode with
      | Pass _ -> sprintf "PASS"
      | Break _ -> sprintf "BREAK"
      | Continue _ -> sprintf "CONTINUE"
      | Expr x -> Expr.to_string x
      | AssignEq (l, op, r) ->
        sprintf "%s %s= %s" (Expr.to_string l) op (Expr.to_string r)
      | Assign (l, r, s, q) ->
        (match q with
        | Some q ->
          sprintf
            "%s : %s %s %s"
            (ppl ~sep:"," l ~f:Expr.to_string)
            (Expr.to_string q)
            (match s with
            | Shadow -> ":="
            | _ -> "=")
            (ppl ~sep:"," r ~f:Expr.to_string)
        | None ->
          sprintf
            "%s %s %s"
            (ppl ~sep:"," l ~f:Expr.to_string)
            (match s with
            | Shadow -> ":="
            | _ -> "=")
            (ppl ~sep:"," r ~f:Expr.to_string))
      | Print (x, n) ->
        sprintf "PRINT %s, %s" (ppl x ~f:Expr.to_string) (String.escaped n)
      | Del x -> sprintf "DEL %s" (Expr.to_string x)
      | Assert x -> sprintf "ASSERT %s" (Expr.to_string x)
      | Yield x -> sprintf "YIELD %s" (Option.value_map x ~default:"" ~f:Expr.to_string)
      | Return x ->
        sprintf "RETURN %s" (Option.value_map x ~default:"" ~f:Expr.to_string)
      | TypeAlias (x, l) -> sprintf "TYPE %s = %s" x (Expr.to_string l)
      | While (x, l) ->
        sprintf
          "WHILE %s:\n%s"
          (Expr.to_string x)
          (ppl l ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))
      | For (v, x, l) ->
        sprintf
          "FOR %s IN %s:\n%s"
          (ppl v ~f:Fn.id)
          (Expr.to_string x)
          (ppl l ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))
      | If l ->
        String.concat ~sep:"\n"
        @@ List.mapi l ~f:(fun i { cond; cond_stmts } ->
               let cond = Option.value_map cond ~default:"" ~f:Expr.to_string in
               let case =
                 if i = 0 then "IF " else if cond = "" then "ELSE" else "ELIF "
               in
               sprintf
                 "%s%s:\n%s"
                 case
                 cond
                 (ppl cond_stmts ~sep:"\n" ~f:(to_string ~indent:(indent + 1))))
      | Match (x, l) ->
        sprintf
          "MATCH %s:\n%s"
          (Expr.to_string x)
          (ppl l ~sep:"\n" ~f:(fun { pattern; case_stmts } ->
               sprintf
                 "case <?>:\n%s"
                 (ppl case_stmts ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))))
      | _ -> "?"
    in
    let pad l = String.make (l * 2) ' ' in
    sprintf "%s%s" (pad indent) s

  and param_to_string (_, { name; typ }) =
    let typ = Option.value_map typ ~default:"" ~f:(fun x -> " : " ^ Expr.to_string x) in
    sprintf "%s%s" name typ

  (** [walk ~f expr] walks through an AST node [expr] and calls
      [f expr] on each child that potentially contains an identifier [Id].
      Useful for locating all captured variables within [expr]. *)
  let rec walk ~fe ~f (pos, node) =
    let walk = walk ~f ~fe in
    let ewalk = Expr.walk ~f:fe in
    let pwalk = fun { name; typ } -> { name; typ = typ >>| ewalk } in
    let rec mwalk = function
      | TuplePattern pl -> TuplePattern (List.map pl ~f:mwalk)
      | ListPattern pl -> ListPattern (List.map pl ~f:mwalk)
      | OrPattern pl -> OrPattern (List.map pl ~f:mwalk)
      | GuardedPattern (p, e) -> GuardedPattern (mwalk p, ewalk e)
      | BoundPattern (s, p) -> BoundPattern (s, mwalk p)
      | p -> p
    in
    f (pos, match node with
      | Expr s -> Expr (ewalk s)
      | AssignEq (l, o, r) -> AssignEq (ewalk l, o, ewalk r)
      | Assign (l, r, a, t) -> Assign (List.map l ~f:ewalk, List.map r ~f:ewalk, a, t >>| ewalk)
      | Del t -> Del (ewalk t)
      | Print (l, s) -> Print (List.map l ~f:ewalk, s)
      | Return e -> Return (e >>| ewalk)
      | Yield e -> Yield (e >>| ewalk)
      | Assert e -> Assert (ewalk e)
      | TypeAlias (s, e) -> TypeAlias (s, ewalk e)
      | While (c, l) -> While (ewalk c, List.map l ~f:walk)
      | For (s, c, l) -> For (s, ewalk c, List.map l ~f:walk)
      | If l -> If (List.map l ~f:(fun { cond; cond_stmts } ->
          { cond = cond >>| ewalk; cond_stmts = List.map cond_stmts ~f:walk }))
      | Match (e, l) -> Match (ewalk e, List.map l ~f:(fun { pattern; case_stmts } ->
          { pattern = mwalk pattern; case_stmts = List.map case_stmts ~f:walk }))
      | Extend (e, gl) -> Extend(ewalk e, List.map gl ~f:walk)
      | Extern (a, b, c, p, pl) ->
        Extern (a, b, c, pwalk p, List.map pl ~f:pwalk)
      | Function f -> Function
        { f with fn_name = pwalk f.fn_name
        ; fn_args = List.map f.fn_args ~f:pwalk
        ; fn_stmts = List.map f.fn_stmts ~f:walk }
      | Class f -> Class
        { f with args = f.args >>| List.map ~f:pwalk
        ; members = List.map f.members ~f:walk }
      | Type f -> Type
        { f with args = f.args >>| List.map ~f:pwalk
        ; members = List.map f.members ~f:walk }
      | Declare p -> Declare (pwalk p)
      | Try (tl, cl, fl) ->
       (* of (t ann list * catch ann list * t ann list option) *)
        Try (List.map tl ~f:walk, List.map cl ~f:(fun e ->
          { e with exc = e.exc >>| ewalk
          ; stmts = List.map e.stmts ~f:walk}), fl >>| List.map ~f:walk)
      (** [Try(stmts, catch, finally_stmts)] corresponds to
      [try: stmts ... ; (catch1: ... ; catch2: ... ;) finally: finally_stmts ] *)
      | Throw e -> Throw (ewalk e)
      | Prefetch l -> Prefetch (List.map l ~f:ewalk)
      | Special (s, l, sl) -> Special (s, List.map l ~f:walk, sl)
      | Pass _ | Break _ | Continue _ | Import _ | ImportPaste _ | Global _ -> node
      )
end

let var_of_node n =
  Ann.var_of_typ (fst n).Ann.typ

let var_of_node_exn n =
  Ann.var_of_typ_exn (fst n).Ann.typ

let e_id ?(ann=default) n =
  ann, Expr.Id n

let e_int ?(ann=default) n =
  ann, Expr.Int (sprintf "%d" n, "")

let e_string ?(ann=default) s =
  ann, Expr.String s

let e_ellipsis ?(ann=default) () =
  ann, Expr.Ellipsis ()

let e_tuple ?(ann=default) args =
  ann, Expr.Tuple args

let e_call ?(ann=default) callee args =
  ann, Expr.(Call (callee,
    List.map args ~f:(fun value -> { name = None; value })))

let e_slice ?(ann=default) ?b ?c a  =
  ann, Expr.Slice (Some a, b, c)

let e_index ?(ann=default) collection args =
  ann, Expr.Index (collection, args)

let e_binary ?(ann=default) lhs op rhs =
  ann, Expr.Binary (lhs, op, rhs)

let e_dot ?(ann=default) left what =
  ann, Expr.Dot (left, what)

let e_typeof ?(ann=default) what =
  ann, Expr.TypeOf what

let s_expr ?(ann=default) expr =
  ann, Stmt.Expr expr

let s_assign ?(ann=default) ?(shadow=Stmt.Normal) lhs rhs =
  ann, Stmt.(Assign ([lhs], [rhs], shadow, None))

let s_if ?(ann=default) cond stmts =
  ann, Stmt.(If [ { cond = Some cond; cond_stmts = stmts } ])

let s_for ?(ann=default) vars iter stmts =
  ann, Stmt.For (vars, iter, stmts)

let s_assert ?(ann=default) expr =
  ann, Stmt.Assert expr

let s_print ?(ann=default) ?(term="") expr =
  ann, Stmt.Print ([expr], term)

let s_extern ?(ann=default) ?(lang="c") name ret params =
  ann, Stmt.Extern
    ( lang
    , None
    , name
    , { name = name; typ = Some ret }
    , List.map params ~f:(fun (name, typ) -> Stmt.{ name; typ = Some typ })
    )


let rec e_dbg (enode : Expr.t ann) =
  let s = match snd enode with
  | Empty _ -> "#empty("
  | Ellipsis _ -> "#ellipsis("
  | Bool x -> if x then "#bool(1" else "#bool(0"
  | Int (x, k) -> sprintf "#int(%s, %s" x k
  | Float (x, k) -> sprintf "#float(%s, %s" x k
  | String x -> sprintf "#string('%s'" (String.escaped x)
  | Kmer x -> sprintf "#kmer('%s'" x
  | Seq x -> sprintf "#seq('%s'" x
  | Id x -> sprintf "#id(%s" x
  | Unpack x -> sprintf "#unpack(%s" x
  | Tuple l when List.length l = 1 -> sprintf "#tuple(%s" (ppl l ~f:e_dbg)
  | Tuple l -> sprintf "#tuple(%s" (ppl l ~f:e_dbg)
  | List l -> sprintf "#list(%s" (ppl l ~f:e_dbg)
  | Set l -> sprintf "#set(%s" (ppl l ~f:e_dbg)
  | Dict l ->
    List.chunks_of l ~length:2
    |> ppl ~f:(function
      | [a; b] -> sprintf "(%s, %s)" (e_dbg a) (e_dbg b)
      | _ -> failwith "cannot happen")
    |> sprintf "#dict(%s"
  | IfExpr (x, i, e) ->
    sprintf "#eif(%s, %s, %s" (e_dbg x) (e_dbg i) (e_dbg e)
  | Pipe l ->
    sprintf "#pipe(%s" (ppl l ~sep:", " ~f:(fun (p, e) -> sprintf "#op='%s', %s" p @@ e_dbg e))
  | Binary (l, o, r) -> sprintf "#op(%s, %s, %s" o (e_dbg l) (e_dbg r)
  | Unary (o, x) -> sprintf "#op(%s, %s" o (e_dbg x)
  | Index (x, l) -> sprintf "#index(%s, %s" (e_dbg x) (ppl l ~f:e_dbg)
  | Dot (x, s) -> sprintf "#dot(%s, %s" (e_dbg x) s
  | Call (x, l) -> sprintf "#call(%s, %s" (e_dbg x) (ppl l ~f:(fun Expr.{name; value} ->
    sprintf
      "%s%s"
      (Option.value_map name ~default:"" ~f:(fun x -> sprintf "%s=" x))
      (e_dbg value)
    ))
  | TypeOf x -> sprintf "#typeof(%s" (e_dbg x)
  | Ptr x -> sprintf "#ptr(%s" (e_dbg x)
  | Slice (a, b, c) ->
    let l = List.map [ ("st", a); ("ed", b); ("step", c) ] ~f:(function (x, y) ->
      match y with None -> ""
      | Some y -> sprintf "#%s=%s" x (e_dbg y)) in
    sprintf "#slice(%s" (ppl l ~f:Fn.id)
  | Lambda (l, x) -> sprintf "#lambda(%s, %s" (ppl l ~f:Fn.id) (e_dbg x)
  | Generator (t, x, c) ->
    let c = comp_dbg c in
    match t, x with
    | "list", [x] -> sprintf "#lgen(%s, @next=%s" (e_dbg x) c
    | "set", [x] -> sprintf "#sgen(%s, @next=%s" (e_dbg x) c
    | "tuple", [x] -> sprintf "#gen(%s, @next=%s" (e_dbg x) c
    | "dict", [x; y] -> sprintf "#dgen(%s, %s, @next=%s" (e_dbg x) (e_dbg y) c
    | _ -> failwith "invalid generator"
  in
  sprintf "%s, @typ=%s)" s (Ann.t_to_string ~useds:true (fst enode).typ)
and comp_dbg { var; gen; cond; next } =
  sprintf
    "#gen(%s, %s%s%s)"
    (ppl var ~f:Fn.id)
    (e_dbg gen)
    (Option.value_map cond ~default:"" ~f:(fun x -> sprintf ", @cond=%s" (e_dbg x)))
    (Option.value_map next ~default:"" ~f:(fun x -> ", @next=%s" ^ comp_dbg x))
and s_dbg ?(indent = 0) (snode : Stmt.t ann) =
  let s = match snd snode with
    | Pass _ -> sprintf "#pass"
    | Break _ -> sprintf "#break"
    | Continue _ -> sprintf "#continue"
    | Expr x -> sprintf "#expr(%s)" @@ e_dbg x
    | AssignEq (l, op, r) ->
      sprintf "#assign(%s, %s, @op='%s=')" (e_dbg l) (e_dbg r) op
    | Assign (l, r, s, q) ->
      (match q with
      | Some q ->
        sprintf
          "#assign(%s, %s, @op='%s', @typ=%s)"
          (ppl ~sep:"," l ~f:e_dbg)
          (ppl ~sep:"," r ~f:e_dbg)
          (match s with
          | Shadow -> ":="
          | _ -> "=")
          (e_dbg q)
      | None ->
        sprintf
          "#assign(%s, %s, @op='%s')"
          (ppl ~sep:"," l ~f:e_dbg)
          (ppl ~sep:"," r ~f:e_dbg)
          (match s with
          | Shadow -> ":="
          | _ -> "="))
    | Print (x, n) ->
      sprintf "#print(%s, @end='%s')" (ppl x ~f:e_dbg) (String.escaped n)
    | Del x -> sprintf "#del(%s)" (e_dbg x)
    | Assert x -> sprintf "#assert(%s)" (e_dbg x)
    | Yield x -> sprintf "#yield(%s)" (Option.value_map x ~default:"" ~f:e_dbg)
    | Return x ->
      sprintf "#return(%s)" (Option.value_map x ~default:"" ~f:e_dbg)
    | TypeAlias (x, l) -> sprintf "#alias(%s, %s)" x (e_dbg l)
    | While (x, l) ->
      sprintf
        "#while(%s,\n%s)"
        (e_dbg x)
        (ppl l ~sep:",\n" ~f:(s_dbg ~indent:(indent + 1)))
    | For (v, x, l) ->
      sprintf
        "#for(%s, %s,\n%s)"
        (ppl v ~f:Fn.id)
        (e_dbg x)
        (ppl l ~sep:",\n" ~f:(s_dbg ~indent:(indent + 1)))
    | If l ->
      String.concat ~sep:"\n"
      @@ List.mapi l ~f:(fun i { cond; cond_stmts } ->
              let cond = Option.value_map cond ~default:"" ~f:e_dbg in
              let case =
                if i = 0 then "#if" else if cond = "" then "#else" else "#elif"
              in
              sprintf
                "%s(%s,\n%s)"
                case
                cond
                (ppl cond_stmts ~sep:",\n" ~f:(s_dbg ~indent:(indent + 1))))
    | Match (x, l) ->
      sprintf
        "#match(%s,\n%s"
        (e_dbg x)
        (ppl l ~sep:"\n" ~f:(fun Stmt.{ pattern; case_stmts } ->
              sprintf
                "case <?>:\n%s"
                (ppl case_stmts ~sep:",\n" ~f:(s_dbg ~indent:(indent + 1)))))
    | _ -> "?"
  in
  let pad l = String.make (l * 2) ' ' in
  sprintf "%s%s" (pad indent) s

(* and param_to_string (_, { name; typ }) =
  let typ = Option.value_map typ ~default:"" ~f:(fun x -> " : " ^ Expr.to_string x) in
  sprintf "%s%s" name typ *)
