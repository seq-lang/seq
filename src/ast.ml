(* 786 *)

open Core
open Sexplib.Std

type expr =
  | Default
  | Bool of bool
  | Int of int
  | Float of float
  | String of string
  | Ident of string
  | Binary of expr * string * expr
  | Pipe of expr list  
  | Index of string * expr
  | Call of string * callarg list
  | List of expr list
  | Dict of (expr * expr) list
  | Extern of string * string
  | As of string * string 
and callarg =
  | NamedArg of string * expr
  | ExprArg of expr
  [@@deriving sexp]

type statement =
  | Pass
  | Expr of expr
  | Print of expr list
  | Assign of string * expr (* x = sth *)
  | For of string * expr * statement list (* var, array, for-st *)
  | While of expr * statement list 
    | Break 
    | Continue
  | If of (expr * statement list) list
  | Match of expr * (expr * string option * statement list) list
  | Function of (string * typet) * defarg list * statement list (* name, args, fun-st *)
    | Return of expr
    | Yield of expr
  | Eof
and typet =
  | Type of string
  | Auto
and defarg =
  | Arg of (string * typet)
  | KeyValArg of (string * typet) * expr
  [@@deriving sexp]

type ast = 
  | Module of statement list
  [@@deriving sexp]

let rec flatten = function 
  | Binary(e1, op, e2) when op = "|>" -> 
    begin
      let f1 = flatten e1 in
      let f2 = flatten e2 in
      match f1, f2 with 
      (* TODO optimize calls below, @ is O(n) *)
      | Pipe f1, Pipe f2 -> Pipe (f1 @ f2) 
      | Pipe f1, f2 -> Pipe (f1 @ [f2])
      | f1, Pipe f2 -> Pipe (f1 :: f2)
      | f1, f2 -> Pipe [f1; f2]
    end
  | Binary(e1, op, e2) -> Binary(flatten e1, op, flatten e2)
  | Index(s, e) -> Index(s, flatten e)
  | Call(c, l) -> Call(c, List.map l ~f:(
      fun x -> match x with
        | NamedArg(n, e) -> NamedArg(n, flatten e)
        | ExprArg(e) -> ExprArg(flatten e)))
  | List(l) -> List(List.map l ~f:flatten)
  | Dict(l) -> Dict(List.map l ~f:(
      fun (k, v) -> (flatten k, flatten v)))
  | _ as e -> e

let prn_ast_sexp a =
  sexp_of_ast a |> Sexp.to_string 

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


