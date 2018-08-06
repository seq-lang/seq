(* 786 *)

open Core
open Sexplib.Std

type expr =
  | Bool of bool
  | Int of int
  | Float of float
  | String of string
  | Id of string
  | Tuple of expr list
  | List of expr list 
  | Dict of (expr * expr) list
  | Set of expr list
  | IfExpr of expr * expr * expr 
  | Lambda of vararg list * expr
  | Pipe of expr list
  | Cond of expr * string * expr
  | Not of expr
  | Call of expr * expr list
  | Index of expr * expr list
  | Dot of expr * expr
  | Binary of expr * string * expr
  | KeyValue of expr * expr 
  | Ellipsis
  | Slice of expr option * expr option * expr option
and vararg = 
  | TypedArg of expr * string option
  | NamedArg of expr * expr
  [@@deriving sexp]

type statement =
  | Pass | Break | Continue
  | Statements of statement list
  | Assign of expr list * expr list
  | AssignEq of expr list * string * expr list
  | Print of expr list
  | Return of expr list
  | Yield of expr list
  | Global of expr list 
  | Assert of expr list
  | While of expr * statement list
  | For of expr * expr list * statement list
  | If of (expr * statement list) list
  | Match of expr * (expr * expr option * statement list) list
  | Function of vararg * vararg list * statement list
  | DecoratedFunction of decorator list * statement
and decorator =
  | Decorator of expr * expr list
  [@@deriving sexp]

type ast = 
  | Module of statement list
  [@@deriving sexp]

let prn_ast_sexp a =
  sexp_of_ast a |> Sexp.to_string 

(*
let sci sep lst fn =
  String.concat ~sep:sep @@ List.map ~f:fn lst
let pad l = String.make (l * 2) ' '
let rec prn_ast = function 
  | Module sl -> sci "\n" sl (prn_statement 0)
and prn_statement level st = 
  let s = match st with
  | Pass -> "Pass"
  | Expr(e) -> prn_expr e
  | Print(el) -> sprintf "Print[%s]" (sci ", " el (prn_expr))
  | Assign(s, e) -> sprintf "Asgn[%s := %s]" s (prn_expr e)
  | For(s, e, sl) -> 
    sprintf "For[%s, %s,\n%s]" s (prn_expr e) @@
      sci "\n" sl (prn_statement (level + 1))
  | While(e, sl) ->
    sprintf "While([%s,\n%s]" (prn_expr e) @@
      sci "\n" sl (prn_statement (level + 1))
  | Break -> "Break"
  | Continue -> "Continue"
  | If(el) -> sprintf "If[\n%s]" @@ 
      sci "\n" el (fun (e, sl) -> 
        sprintf "%s%s -> [\n%s]" (pad (level+1)) (prn_expr e) @@
          sci "\n" sl (prn_statement (level+2)))
  | Match(e, ml) -> sprintf "Match[%s\n%s]" (prn_expr e) @@ 
      sci "\n" ml (fun (e, v, sl) -> 
        let pv = match v with | Some e -> " AS " ^ e | None -> "" in
        sprintf "%s%s%s -> [\n%s]" (pad (level+1)) (prn_expr e) pv @@
          sci "\n" sl (prn_statement (level+2)))
  | Function(t, al, sl) -> 
      let pt (n, t) = 
        let t = match t with Type tt -> tt | Auto -> "*" in
        sprintf "%s:%s" n t in
      let pda = function 
        | Arg t -> pt t 
        | KeyValArg(t, e) -> sprintf "%s = %s" (pt t) (prn_expr e)
      in
      sprintf "Def[%s := %s, \n%s]" (pt t) (sci ", " al pda) @@ 
        sci "\n" sl (prn_statement (level + 1))
  | Return(e) -> sprintf "Return[%s]" @@ prn_expr e
  | Yield(e) -> sprintf "Yield[%s]" @@ prn_expr e
  | Eof -> "Eof"
  in (pad level) ^ s
and prn_expr = function
  | Default -> "Default"
  | Bool(b) -> sprintf "Bool(%b)" b
  | Int(i) -> sprintf "Int(%d)" i
  | Float(f) -> sprintf "Float(%f)" f
  | String(s) -> sprintf "String(%s)" s
  | Ident(i) -> sprintf "%s" i
  | Binary(e, o, ee) -> sprintf "%s[%s, %s]" o (prn_expr e) (prn_expr ee)
  | Pipe(el) -> sprintf "Pipe[%s]" @@ sci ", " el prn_expr
  | Index(i, e) -> sprintf "%s[[%s]]" i (prn_expr e)
  | Call(i, cl) -> sprintf "Call[%s; %s]" i (sci ", " cl (fun x -> match x with
      | ExprArg e -> prn_expr e
      | NamedArg(n, e) -> sprintf "%s = %s" n (prn_expr e)
    ))
  | List(el) -> sprintf "List[%s]" (sci ", " el prn_expr)
  | Dict(el) -> sprintf "Dict[%s]" (sci ", " el (fun (k, v) ->
      sprintf "%s: %s" (prn_expr k) (prn_expr v)))
  | Extern(s, p) -> sprintf "Extern[%s, %s]" s p
  | As(s, t) -> sprintf "As[%s, %s]" s t


*)