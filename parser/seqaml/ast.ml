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

  let pos_to_string { file; line; col; _ } =
    sprintf "%s:%d:%d" (Filename.basename file) line col

  let rec typ_to_string ?(generics = Int.Table.create ()) typ =
    let to_string = typ_to_string ~generics in
    let gen2str g =
      ppl ~sep:"," g ~f:(fun (_, (g, t)) -> sprintf "%s" (to_string t))
    in
    match typ.kind with
    | Unknown ->
      "?"
    | Import s ->
      sprintf "<%s>" s
    | Tuple args ->
      sprintf "tuple[%s]" (ppl ~sep:"," args ~f:to_string)
    | Func { f_generics; f_args; f_ret; _ } ->
      let g = gen2str f_generics in
      sprintf
        "function%s((%s),%s)"
        (if g = "" then "" else sprintf "[%s]" g)
        (ppl ~sep:"," f_args ~f:(fun (_, t) -> to_string t))
        (to_string f_ret)
    | Class { c_name; c_generics; _ } ->
      let g = gen2str c_generics in
      sprintf "%s%s" c_name @@
        if g = "" then "" else sprintf "[%s]" g
    | TypeVar { contents = Unbound (u, _, _) } ->
      sprintf "'%d" u
    | TypeVar { contents = Bound t } ->
      to_string t
    | TypeVar { contents = Generic u } ->
      sprintf
        "T%d"
        (match Hashtbl.find generics u with
        | Some s -> s
        | None ->
          let w = succ @@ Hashtbl.length generics in
          Hashtbl.set generics ~key:u ~data:w;
          w)

  let to_sring { pos; typ } =
    sprintf "<%s |= %s>" (pos_to_string pos) (typ_to_string typ)
end


(** Alias for [Ast_expr]. Adds [to_string]. *)
module Expr = struct
  include Ast_expr

  let rec to_string (enode : t ann) =
    match snd enode with
    | Empty _ -> ""
    | Ellipsis _ -> "..."
    | Bool x -> if x then "True" else "False"
    | Int x -> sprintf "%s" x
    | IntS (x, k) -> sprintf "%s%s" x k
    | Float x -> sprintf "%f" x
    | FloatS (x, k) -> sprintf "%f%s" x k
    | String x -> sprintf "'%s'" (String.escaped x)
    | Kmer x -> sprintf "k'%s'" x
    | Seq x -> sprintf "s'%s'" x
    | Id x -> sprintf "%s" x
    | Unpack x -> sprintf "*%s" x
    | Tuple l -> sprintf "(%s)" (ppl l ~f:to_string)
    | List l -> sprintf "[%s]" (ppl l ~f:to_string)
    | Set l -> sprintf "{%s}" (ppl l ~f:to_string)
    | Dict l ->
      sprintf "{%s}"
      @@ ppl l ~f:(fun (a, b) -> sprintf "%s: %s" (to_string a) (to_string b))
    | IfExpr (x, i, e) ->
      sprintf "%s if %s else %s" (to_string x) (to_string i) (to_string e)
    | Pipe l ->
      sprintf "%s" (ppl l ~sep:"" ~f:(fun (p, e) -> sprintf "%s %s" p @@ to_string e))
    | Binary (l, o, r) -> sprintf "(%s %s %s)" (to_string l) o (to_string r)
    | Unary (o, x) -> sprintf "(%s %s)" o (to_string x)
    | Index (x, l) -> sprintf "%s[%s]" (to_string x) (to_string l)
    | Dot (x, s) -> sprintf "%s.%s" (to_string x) s
    | Call (x, l) -> sprintf "%s(%s)" (to_string x) (ppl l ~f:call_to_string)
    | TypeOf x -> sprintf "typeof(%s)" (to_string x)
    | Ptr x -> sprintf "ptr(%s)" (to_string x)
    | Slice (a, b, c) ->
      let l = List.map [ a; b; c ] ~f:(Option.value_map ~default:"" ~f:to_string) in
      sprintf "%s" (ppl l ~sep:":" ~f:Fn.id)
    | Generator (x, c) -> sprintf "(%s %s)" (to_string x) (comprehension_to_string c)
    | ListGenerator (x, c) -> sprintf "[%s %s]" (to_string x) (comprehension_to_string c)
    | SetGenerator (x, c) -> sprintf "{%s %s}" (to_string x) (comprehension_to_string c)
    | DictGenerator ((x1, x2), c) ->
      sprintf "{%s: %s %s}" (to_string x1) (to_string x2) (comprehension_to_string c)
    | Lambda (l, x) -> sprintf "lambda (%s): %s" (ppl l ~f:snd) (to_string x)

  and call_to_string (_, { name; value }) =
    sprintf
      "%s%s"
      (Option.value_map name ~default:"" ~f:(fun x -> x ^ " = "))
      (to_string value)

  and comprehension_to_string (_, { var; gen; cond; next }) =
    sprintf
      "for %s in %s%s%s"
      (ppl var ~f:Fn.id)
      (to_string gen)
      (Option.value_map cond ~default:"" ~f:(fun x -> sprintf "if %s" (to_string x)))
      (Option.value_map next ~default:"" ~f:(fun x -> " " ^ comprehension_to_string x))
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
      | Assign (l, r, s, q) ->
        (match q with
        | Some q ->
          sprintf
            "%s : %s %s %s"
            (Expr.to_string l)
            (Expr.to_string q)
            (match s with
            | Shadow -> ":="
            | _ -> "=")
            (Expr.to_string r)
        | None ->
          sprintf
            "%s %s %s"
            (Expr.to_string l)
            (match s with
            | Shadow -> ":="
            | _ -> "=")
            (Expr.to_string r))
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
        @@ List.mapi l ~f:(fun i (_, { cond; cond_stmts }) ->
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
          (ppl l ~sep:"\n" ~f:(fun (_, { pattern; case_stmts }) ->
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
end
