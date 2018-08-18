(* 786 *)

open Core
open Sexplib.Std

type expr =
  | Bool of bool
  | Int of int
  | Float of float
  | String of string
  | Extern of string * string
  | Id of string
  | Regex of string
  | Seq of string

  | Tuple of expr list
  | Generator of expr * expr
  | List of expr list 
  | ListGenerator of expr * expr
  | Set of expr list
  | SetGenerator of expr * expr
  | Dict of (expr * expr) list
  | DictGenerator of (expr * expr) * expr

  | IfExpr of expr * expr * expr 
  | Lambda of vararg list * expr

  | Pipe of expr list
  | Cond of expr * string * expr
  | Not of expr
  | Binary of expr * string * expr
  | Index of expr * expr list
  | Slice of expr option * expr option * expr option
  | Dot of expr * string
  | Call of expr * vararg list

  | Comprehension of expr list * expr list * expr option
  | ComprehensionIf of expr
  | Ellipsis
and vararg = 
  | PlainArg of expr
  | TypedArg of expr * string option
  | NamedArg of expr * expr
  [@@deriving sexp]

type statement =
  | Pass | Break | Continue
  | Statements of statement list
  | Exprs of expr list
  | Assign of expr * expr list
  | AssignEq of expr * string * expr list
  | Print of expr list
  | Return of expr list
  | Yield of expr list
  | Global of expr list 
  | Assert of expr list
  | Type of expr * vararg list
  | While of expr * statement list
  | For of expr * expr * statement list
  | If of (expr option * statement list) list
  | Match of expr * (expr * expr option * statement list) list
  | Function of vararg * vararg list * statement list
  | DecoratedFunction of decorator list * statement
  | Import of (expr * expr option) list
  | ImportFrom of expr * ((expr * expr option) list) option
and decorator =
  | Decorator of expr * vararg list
  [@@deriving sexp]

type ast = 
  | Module of statement list
  [@@deriving sexp]

let prn_ast_sexp a =
  sexp_of_ast a |> Sexp.to_string 


let sci sep lst fn =
  String.concat ~sep:sep @@ List.map ~f:fn lst
let pad l = String.make (l * 2) ' '

let rec prn_expr = function
  | Bool(b) -> sprintf "Bool(%b)" b
  | Int(i) -> sprintf "Int(%d)" i
  | Float(f) -> sprintf "Float(%f)" f
  | String(s) -> sprintf "String(%s)" s
  | Regex(s) -> sprintf "Regex(%s)" s
  | Seq(s) -> sprintf "Seq(%s)" s
  | Extern(l, v) -> sprintf "Extern_%s(%s)" l v
  | Id(i) -> sprintf "%s" i
  | Tuple(el) -> sprintf "Tuple(%s)" @@ sci ", " el prn_expr
  | Generator(e, ge) -> sprintf "Gen[%s; %s]" (prn_expr e) (prn_expr ge)
  | List(el) -> sprintf "List(%s)" @@ sci ", " el prn_expr
  | ListGenerator(e, ge) -> sprintf "ListGen[%s; %s]" (prn_expr e) (prn_expr ge)
  | Set(el) -> sprintf "Set(%s)" @@ sci ", " el prn_expr
  | SetGenerator(e, ge) -> sprintf "SetGen[%s; %s]" (prn_expr e) (prn_expr ge)
  | Dict(el) -> sprintf "Dict(%s)" @@ sci ", " el (fun (x, y) -> sprintf "%s:%s" (prn_expr x) (prn_expr y))
  | DictGenerator((k, v), ge) -> sprintf "DictGen[%s:%s; %s]" (prn_expr k) (prn_expr v) (prn_expr ge)
  | IfExpr(c, i, e) -> sprintf "?[%s; %s; %s]" (prn_expr c) (prn_expr i) (prn_expr e)
  | Lambda(v, e) -> sprintf "Lambda[%s; %s]" (sci ", " v prn_va) (prn_expr e) 
  | Pipe(el) -> sprintf "Pipe[%s]" @@ sci ", " el prn_expr
  | Cond(e, o, ee) -> sprintf "%s[%s, %s]" o (prn_expr e) (prn_expr ee)
  | Not(e) -> sprintf "Not[%s]" @@ prn_expr e
  | Binary(e, o, ee) -> sprintf "%s[%s, %s]" o (prn_expr e) (prn_expr ee)
  | Index(i, e) -> sprintf "%s.[%s]" (prn_expr i) (sci ", " e prn_expr)
  | Dot(i, e) -> sprintf "%s.%s" (prn_expr i) e
  | Call(i, cl) -> sprintf "Call[%s; %s]" (prn_expr i) (sci ", " cl prn_va)
  | Comprehension(fi, ei, li) -> 
    let cont = match li with None -> "" | Some x -> prn_expr x in
    sprintf "_For[%s; %s]%s" (sci ", " fi prn_expr) (sci ", " ei prn_expr) cont
  | ComprehensionIf(e) -> sprintf "_If[%s]" @@ prn_expr e
  | Ellipsis -> "..."
  | Slice(a, b, c) ->
    let a = match a with None -> "" | Some x -> prn_expr x in
    let b = match b with None -> "" | Some x -> prn_expr x in
    let c = match c with None -> "" | Some x -> prn_expr x in
    sprintf "Slice[%s, %s, %s]" a b c
and prn_va = function
  | PlainArg(p) -> prn_expr p
  | TypedArg(p, o) -> 
    let o = match o with None -> "any" | Some x -> x in 
    sprintf "%s of %s" (prn_expr p) o
  | NamedArg(n, e) -> sprintf "%s = %s" (prn_expr n) (prn_expr e)

let rec prn_statement level st = 
  let s = match st with
  | Pass -> "Pass" | Break -> "Break" | Continue -> "Continue"
  | Statements(sl) -> sprintf "Statements[\n%s]" (sci "\n" sl (prn_statement (level+1)))
  | Exprs(el) -> sprintf "Exprs[%s]" (sci "," el prn_expr)
  | Assign(sl, el) -> sprintf "Asgn[%s := %s]" (prn_expr sl) (sci ", " el prn_expr)
  | AssignEq(sl, op, el) -> sprintf "Asgn[%s %s %s]" (prn_expr sl) op (sci ", " el prn_expr)
  | Print(el) -> sprintf "Print[%s]" (sci ", " el (prn_expr))
  | Yield(el) -> sprintf "Yield[%s]" (sci ", " el (prn_expr))
  | Return(el) -> sprintf "Return[%s]" (sci ", " el (prn_expr))
  | Global(el) -> sprintf "Global[%s]" (sci ", " el (prn_expr))
  | Assert(el) -> sprintf "Assert[%s]" (sci ", " el (prn_expr))
  | Type(e, vl) -> sprintf "Type[%s; %s]" (prn_expr e) (sci ", " vl prn_va)
  | While(e, sl) ->
    sprintf "While[%s;\n%s]" (prn_expr e) @@
      sci "\n" sl (prn_statement (level + 1))
  | For(sl, el, stl) -> 
    sprintf "For[%s; %s;\n%s]" (prn_expr sl) (prn_expr el) @@
      sci "\n" stl (prn_statement (level + 1))
  | If(el) -> sprintf "If[\n%s]" @@ 
      sci "\n" el (fun (e, sl) -> 
        let cnd = match e with | Some _e -> prn_expr _e | None -> "_" in
        sprintf "%s%s -> [\n%s]" (pad (level+1)) cnd @@
          sci "\n" sl (prn_statement (level+2)))
  | Match(e, ml) -> sprintf "Match[%s;\n%s]" (prn_expr e) @@ 
      sci "\n" ml (fun (e, v, sl) -> 
        let pv = match v with | Some e -> " AS " ^ (prn_expr e) | None -> "" in
        sprintf "%s%s%s -> [\n%s]" (pad (level+1)) (prn_expr e) pv @@
          sci "\n" sl (prn_statement (level+2)))
  | DecoratedFunction(dl, f) -> (sci ("\n" ^ (pad level)) dl 
        (fun d -> match d with Decorator(dd, da) -> 
          sprintf "Decorator[%s; %s]" (prn_expr dd) @@ sci ", " da prn_va)) ^ 
      (prn_statement level f)
  | Function(v, vl, sl) -> 
      sprintf "Def[%s; %s;\n%s]" (prn_va v) (sci ", " vl prn_va) @@ 
        sci "\n" sl (prn_statement (level + 1))
  | Import(el) -> 
    sprintf "Import[%s]" @@ sci ", " el (fun (a, b) ->
      let b = match b with None -> "" | Some x -> " as " ^ (prn_expr x) in
      (prn_expr a) ^ b)
  | ImportFrom(e, el) -> 
    let el = match el with None -> "all" | Some x -> sci ", " x (fun (a, b) ->
      let b = match b with None -> "" | Some x -> " as " ^ (prn_expr x) in
      (prn_expr a) ^ b
    ) in sprintf "Import[%s; %s]" (prn_expr e) el
  in (pad level) ^ s
 
 let prn_ast = function 
  | Module sl -> sci "\n" sl (prn_statement 0)
