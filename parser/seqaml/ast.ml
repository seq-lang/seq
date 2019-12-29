(* *****************************************************************************
 * Seqaml.Ast: AST types and functions
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Util
open Ast_ann

(** This module unifies all AST modules and extends them with
    various utility functions. *)

(** Alias for [Ast_ann]. Adds [to_string]. *)
module Ann = struct
  include Ast_ann

  let to_string { file; line; col; _ } =
    sprintf "%s:%d:%d" (Filename.basename file) line col
end

(** Alias for [Ast_expr]. Adds [to_string]. *)
module Expr = struct
  include Ast_expr

  let rec to_string ?(pythonic=false) (enode : t ann) =
    let to_string = to_string ~pythonic in
    match snd enode with
    | Empty _ -> ""
    | Ellipsis _ -> "..."
    | Bool x -> if x then "True" else "False"
    | Int x -> sprintf "%s" x
    | IntS (x, k) -> sprintf "%s%s" x k
    | Float x -> sprintf "%f" x
    | FloatS (x, k) -> sprintf "%f%s" x k
    | String x -> sprintf "\"%s\"" (String.escaped x)
    | FString x -> sprintf "f\"%s\"" (String.escaped x)
    | Kmer x -> sprintf "k'%s'" x
    | Seq (p, x) -> sprintf "%s'%s'" p x
    | Id x -> sprintf "%s" x
    | Unpack x -> sprintf "*%s" x
    | Tuple l -> sprintf "(%s)" (ppl l ~f:to_string)
    | Set l -> sprintf "{%s}" (ppl l ~f:to_string)
    | List l -> sprintf "[%s]" (ppl l ~f:to_string)
    | Dict l ->
      sprintf "{%s}"
      @@ ppl l ~f:(fun (a, b) -> sprintf "%s: %s" (to_string a) (to_string b))
    | IfExpr (x, i, e) ->
      sprintf "%s if %s else %s" (to_string x) (to_string i) (to_string e)
    | Pipe l ->
      if pythonic
      then Err.serr ~pos:(fst enode) "Python does not support pipes"
      else sprintf "%s" (ppl l ~sep:"" ~f:(fun (p, e) -> sprintf "%s %s" p @@ to_string e))
    | Binary (l, o, r) ->
      sprintf "(%s %s %s)"
        (to_string l)
        (match o with "&&" -> "and" | "||" -> "or" | o -> o)
        (to_string r)
    | Unary (o, x) ->
      sprintf "(%s %s)"
        (match o with "!" -> "not" | o -> o)
        (to_string x)
    | Index (x, l) -> sprintf "%s[%s]" (to_string x) (to_string l)
    | Dot (x, s) -> sprintf "%s.%s" (to_string x) s
    | Call (x, l) -> sprintf "%s(%s)" (to_string x) (ppl l ~f:(call_to_string ~pythonic))
    | TypeOf x ->
      if pythonic
      then Err.serr ~pos:(fst enode) "Python does not support typeof"
      else sprintf "typeof(%s)" (to_string x)
    | Ptr x ->
      sprintf "__ptr__(%s)" (to_string x)
    | Slice (a, b, c) ->
      let l = List.map [ a; b; c ] ~f:(Option.value_map ~default:"" ~f:to_string) in
      sprintf "%s" (ppl l ~sep:":" ~f:Fn.id)
    | Generator (x, c) -> sprintf "(%s %s)" (to_string x) (comprehension_to_string ~pythonic c)
    | ListGenerator (x, c) -> sprintf "[%s %s]" (to_string x) (comprehension_to_string ~pythonic c)
    | SetGenerator (x, c) -> sprintf "{%s %s}" (to_string x) (comprehension_to_string ~pythonic c)
    | DictGenerator ((x1, x2), c) ->
      sprintf "{%s: %s %s}" (to_string x1) (to_string x2) (comprehension_to_string ~pythonic c)
    | Lambda (l, x) -> sprintf "lambda (%s): %s" (ppl l ~f:snd) (to_string x)

  and call_to_string ~pythonic (_, { name; value }) =
    sprintf
      "%s%s"
      (Option.value_map name ~default:"" ~f:(fun x -> x ^ " = "))
      (to_string ~pythonic value)

  and comprehension_to_string ~pythonic (_, { var; gen; cond; next }) =
    sprintf
      "for %s in %s%s%s"
      (ppl var ~f:Fn.id)
      (to_string ~pythonic gen)
      (Option.value_map cond ~default:"" ~f:(fun x -> sprintf "if %s" (to_string ~pythonic x)))
      (Option.value_map next ~default:"" ~f:(fun x -> " " ^ comprehension_to_string ~pythonic x))
end

(** Alias for [Ast_stmt]. Adds [to_string]. *)
module Stmt = struct
  include Ast_stmt

  let rec to_string ?(pythonic=false) ?(indent = 0) (snode : t ann) =
    let pad l = String.make (l * 2) ' ' in
    let to_string = to_string ~pythonic in
    let e_to_string = Expr.to_string ~pythonic in
    let s =
      match snd snode with
      | Pass _ -> sprintf "pass"
      | Break _ -> sprintf "break"
      | Continue _ -> sprintf "continue"
      | Expr x -> e_to_string x
      | Assign (l, r, s, q) ->
        (match q with
        | Some q ->
          sprintf
            "%s : %s %s %s"
            (e_to_string l)
            (e_to_string q)
            (match s with
            | Shadow ->
              if pythonic
              then Err.serr ~pos:(fst snode) "Python does not support shadowing"
              else ":="
            | _ -> "=")
            (e_to_string r)
        | None ->
          sprintf
            "%s %s %s"
            (e_to_string l)
            (match s with
            | Shadow ->
              if pythonic
              then Err.serr ~pos:(fst snode) "Python does not support shadowing"
              else ":="
            | _ -> "=")
            (e_to_string r))
      | Print (x, n) ->
        if pythonic
        then sprintf "print(%s%s)" (ppl x ~f:e_to_string) (if n = "\n" then "" else sprintf ", end='%s'" (String.escaped n))
        else
        sprintf "print %s%s" (ppl x ~f:e_to_string) (if n = "\n" then "" else sprintf ", %s" (String.escaped n))
      | Del x -> sprintf "del %s" (e_to_string x)
      | Assert x -> sprintf "assert %s" (e_to_string x)
      | Yield x -> sprintf "yield %s" (Option.value_map x ~default:"" ~f:e_to_string)
      | Return x ->
        sprintf "return %s" (Option.value_map x ~default:"" ~f:e_to_string)
      | TypeAlias (x, l) ->
        if pythonic
        then Err.serr ~pos:(fst snode) "Python does not support type aliases"
        else sprintf "type %s = %s" x (e_to_string l)
      | While (x, l) ->
        sprintf
          "while %s:\n%s"
          (e_to_string x)
          (ppl l ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))
      | For (v, x, l) ->
        sprintf
          "for %s in %s:\n%s"
          (ppl v ~f:Fn.id)
          (e_to_string x)
          (ppl l ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))
      | If l ->
        String.concat ~sep:"\n"
        @@ List.mapi l ~f:(fun i (_, { cond; cond_stmts }) ->
               let cond = Option.value_map cond ~default:"" ~f:e_to_string in
               let case =
                 if i = 0
                 then "if "
                 else if cond = ""
                 then sprintf "%selse" (pad indent)
                 else sprintf "%selif " (pad indent)
               in
               sprintf
                 "%s%s:\n%s"
                 case
                 cond
                 (ppl cond_stmts ~sep:"\n" ~f:(to_string ~indent:(indent + 1))))
      | Match (x, l) ->
        if pythonic
        then Err.serr ~pos:(fst snode) "Python does not support matching"
        else sprintf
          "match %s:\n%s"
          (e_to_string x)
          (ppl l ~sep:"\n" ~f:(fun (_, { pattern; case_stmts }) ->
               sprintf
                 "case <?>:\n%s"
                 (ppl case_stmts ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))))
      | Global g -> sprintf "global %s" g
      | Throw g -> sprintf "throw %s" (e_to_string g)
      | Try (t, c, f) ->
        sprintf "try:\n%s%s%s"
          (ppl t ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))
          (ppl c ~sep:"" ~f:(fun (_, { exc; var; stmts }) ->
            sprintf "catch%s%s:\n%s"
              (Option.value_map exc ~default:"" ~f:(fun s -> sprintf " %s" s))
              (Option.value_map var ~default:"" ~f:(fun s -> sprintf " as %s" s))
              (ppl stmts ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))))
          (Option.value_map f ~default:"" ~f:(fun t ->
            sprintf "finally:\n%s" (ppl t ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))))
      | Extend (n, t) ->
        if pythonic
        then Err.serr ~pos:(fst snode) "Python does not support extends"
        else sprintf "extend %s:\n%s" (e_to_string n)
          (ppl t ~sep:"\n" ~f:(fun (pos, g) -> to_string ~indent:(indent + 1) (pos, Generic g)))
      | Import l ->
        ppl l ~sep:"\n" ~f:(fun { from; what; import_as } ->
          match what with
          | Some what ->
            sprintf "from %s import %s"
              (if pythonic then String.substr_replace_all (snd from) ~pattern:"/" ~with_:"." else snd from)
              (ppl what ~f:(fun (_, (w, a)) ->
                sprintf "%s%s" w (Option.value_map a ~default:"" ~f:(fun x -> sprintf " as %s" x))
              ))
          | None ->
            sprintf "import %s%s" (snd from)
              (Option.value_map import_as ~default:"" ~f:(fun x -> sprintf " as %s" x))
        )
      | ImportPaste i ->
        if pythonic
        then Err.serr ~pos:(fst snode) "Python does not support import!"
        else sprintf "import! %s" i
      | ImportExtern e ->
        if pythonic
        then Err.serr ~pos:(fst snode) "Python does not support external imports"
        else
          Util.ppl e ~sep:(sprintf "\n%s" (pad indent)) ~f:(fun e ->
            sprintf "%s%simport %s%s"
              (match e.e_from with None -> "" | Some s -> sprintf "from %s " (e_to_string s))
              (e.lang)
              (sprintf "%s(%s) -> %s"
                e.e_name.name
                (ppl e.e_args ~f:(param_to_string ~pythonic))
                (e_to_string (Option.value_exn e.e_name.typ)))
              (Option.value_map e.e_as ~default:"" ~f:(fun x -> sprintf " as %s" x)))
      | Special _ ->
        if pythonic
        then Err.serr ~pos:(fst snode) "Python does not support specials"
        else sprintf "?"
      | Prefetch p ->
        if pythonic
        then Err.serr ~pos:(fst snode) "Python does not support prefetch"
        else sprintf "prefetch %s" (ppl p ~f:e_to_string)
      | Generic Function f ->
        sprintf "%sdef %s%s(%s)%s:\n%s"
          ( match f.fn_attrs with
            | [] -> ""
            | a -> sprintf "%s\n%s" (ppl a ~f:(fun (_, x) -> sprintf "%s@%s" (pad indent) x)) (pad indent))
          f.fn_name.name
          ( match pythonic, f.fn_generics with
            | _, [] -> ""
            | true, _ -> Err.serr ~pos:(fst snode) "Python does not support generics"
            | false, g -> sprintf "[%s]" (ppl g ~f:snd) )
          (ppl f.fn_args ~f:(param_to_string ~pythonic))
          (Option.value_map f.fn_name.typ ~default:"" ~f:(fun x -> sprintf " -> %s" (e_to_string x)))
          (ppl f.fn_stmts ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))
      | Generic (Docstring x) ->
        sprintf "\"%s\"" (String.escaped x)
      | Generic (Class f | Type f) ->
        sprintf "%s %s%s%s%s"
          ( match pythonic, snd snode with
            | _, Generic Class f -> "class"
            | true, _ -> Err.serr ~pos:(fst snode) "Python does not support type"
            | false, _ -> "type")
          f.class_name
          ( match pythonic, f.generics with
            | _, [] -> ""
            | true, _ -> Err.serr ~pos:(fst snode) "Python does not support generics"
            | false, g -> sprintf "[%s]" (ppl g ~f:snd) )
          ( match snd snode with
            | Generic Class _ ->
              sprintf ":\n%s"
              @@ Option.value_map f.args ~default:"" ~f:(fun args ->
                  sprintf "%s\n%s"
                    (ppl args ~sep:"\n" ~f:(fun p ->
                      sprintf "%s%s" (pad (indent+1)) (param_to_string ~pythonic p)))
                    (pad indent))
            | _ ->
              sprintf "(%s):\n"
              @@ Option.value_map f.args ~default:"" ~f:(ppl ~f:(param_to_string ~pythonic))
          )
          (ppl f.members ~sep:"\n" ~f:(fun (p, x) -> to_string ~indent:(indent + 1) (p, Generic x)))
      | Generic Declare d -> param_to_string ~pythonic (fst snode, d)
    in
    sprintf "%s%s" (pad indent) s

  and param_to_string ~pythonic (_, { name; typ; default }) =
    let typ = Option.value_map typ ~default:"" ~f:(fun x -> " : " ^ Expr.to_string ~pythonic x) in
    let def = Option.value_map default ~default:"" ~f:(fun x -> " = " ^ Expr.to_string ~pythonic x) in
    sprintf "%s%s%s" name typ def

  (* TODO *)
  and px = function
      | StarPattern -> "Star"
      | IntPattern i -> "Int"
      | BoolPattern b -> "Bool"
      | StrPattern s -> "Str"
      | SeqPattern s -> "Seq"
      | TuplePattern tl ->
        sprintf "Tuple(%s)" @@ Util.ppl tl ~f:px
      | RangePattern (i, j) -> "Range"
      | ListPattern tl ->
        sprintf "List(%s)" @@ Util.ppl tl ~f:px
      | OrPattern tl ->
        sprintf "Or(%s)" @@ Util.ppl tl ~f:px
      | WildcardPattern wild -> "*"
      | GuardedPattern (pat, expr) -> sprintf "Guarded(%s)" (px pat)
      | BoundPattern _ -> "Bound"
end


let rec e_setpos new_pos (_, node) =
  let f = e_setpos new_pos in
  let rec fg (p, c) = p, Expr.{ c with gen = f c.gen; cond = Option.map c.cond ~f; next = Option.map c.next ~f:fg } in
  new_pos, match node with
    | Expr.Tuple l -> Expr.Tuple (List.map l ~f)
    | List l -> List (List.map l ~f)
    | Set l -> Set (List.map l ~f)
    | Pipe l -> Pipe (List.map l ~f:(fun (x, y) -> x, f y))
    | Dict l -> Dict (List.map l ~f:(fun (x, y) -> f x, f y))
    | IfExpr (a, b, c) -> IfExpr (f a, f b, f c)
    | Unary (o, e) -> Unary (o, f e)
    | Binary (l, o, r) -> Binary (f l, o, f r)
    | Dot (e, d) -> Dot (f e, d)
    | TypeOf e -> TypeOf (f e)
    | Index (a, l) -> Index (f a, f l)
    | Call (a, l) -> Call (f a, List.map l ~f:(fun (p, v) -> p, { v with value = f v.value }))
    | ListGenerator (e, c) -> ListGenerator (f e, fg c)
    | SetGenerator (e, c) -> SetGenerator (f e, fg c)
    | Generator (e, c) -> Generator (f e, fg c)
    | DictGenerator ((e1, e2), c) -> DictGenerator ((f e1, f e2), fg c)
    | Slice (a, b, c) -> Slice (Option.map a ~f, Option.map b ~f, Option.map c ~f)
    | Lambda (s, a) -> Lambda (s, f a)
    | Ptr x -> Ptr (f x)
    | t -> t

let e_id ?(pos=Ann.default) n =
  pos, Expr.Id n

let e_int ?(pos=Ann.default) n =
  pos, Expr.Int n

let e_string ?(pos=Ann.default) s =
  pos, Expr.String s

let e_ellipsis ?(pos=Ann.default) () =
  pos, Expr.Ellipsis ()

let e_tuple ?(pos=Ann.default) args =
  pos, Expr.Tuple args

let e_call ?(pos=Ann.default) callee args =
  pos, Expr.(Call (callee,
    List.map args ~f:(fun value -> pos, { name = None; value })))

let e_slice ?(pos=Ann.default) ?b ?c a  =
  pos, Expr.Slice (Some a, b, c)

let e_index ?(pos=Ann.default) collection args =
  pos, Expr.Index (collection, args)

let e_binary ?(pos=Ann.default) lhs op rhs =
  pos, Expr.Binary (lhs, op, rhs)

let e_dot ?(pos=Ann.default) left what =
  pos, Expr.Dot (left, what)

let e_typeof ?(pos=Ann.default) what =
  pos, Expr.TypeOf what

let s_expr ?(pos=Ann.default) expr =
  pos, Stmt.Expr expr

let s_assign ?(pos=Ann.default) ?(shadow=Stmt.Normal) lhs rhs =
  pos, Stmt.(Assign (lhs, rhs, shadow, None))

let s_return ?(pos=Ann.default) what =
  pos, Stmt.Return what

let s_if ?(pos=Ann.default) cond stmts =
  pos, Stmt.(If [ pos, { cond = Some cond; cond_stmts = stmts } ])

let s_for ?(pos=Ann.default) vars iter stmts =
  pos, Stmt.For (vars, iter, stmts)
