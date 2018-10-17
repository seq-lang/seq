(* 786 *)

open Core

exception NotImplentedError of string

type pos_t = Lexing.position
type ident = string * pos_t

type expr =
  | Bool of bool * pos_t
  | Int of int * pos_t
  | Float of float * pos_t
  | String of ident
  | Id of ident
  (* | Extern of string * string *)
  (* | Regex of string *)
  | Seq of ident

  | Generic of ident
  | TypeOf of expr * pos_t 

  | Tuple of expr list * pos_t
  (* | Generator of expr * expr *)
  (* | List of expr list  *)
  (* | ListGenerator of expr * expr *)
  (* | Set of expr list *)
  (* | SetGenerator of expr * expr *)
  (* | Dict of (expr * expr) list *)
  (* | DictGenerator of (expr * expr) * expr *)

  | IfExpr of expr * expr * expr
  (* | Lambda of vararg list * expr  *)

  | Pipe of expr list
  | Unary of ident * expr
  | Binary of expr * ident * expr
  | Index of expr * expr list
  | Slice of expr option * expr option * expr option * pos_t
  | Dot of expr * ident
  | Call of expr * expr list 

  (* | Comprehension of expr list * expr list * expr option *)
  (* | ComprehensionIf of expr *)
  | Ellipsis
and arg = 
  | Arg of ident * expr option 
  (* | NamedArg of string * expr *)

type statement =
  | Pass of pos_t 
  | Break of pos_t 
  | Continue of pos_t
  | Statements of statement list
  | Exprs of expr
  | Assign of expr * expr * bool
  | AssignEq of expr * ident * expr
  | Print of expr list * pos_t
  | Return of expr * pos_t
  | Yield of expr * pos_t
  (* | Global of expr list  *)
  (* | Assert of expr list *)
  | Type of ident * arg list * pos_t
  | While of expr * statement list * pos_t
  | For of expr * expr * statement list * pos_t
  | If of (expr option * statement list * pos_t) list
  | Match of expr * (expr option * ident option * statement list * pos_t) list * pos_t
  | Function of arg * expr list * arg list * statement list * pos_t 
    (* def arg [typ list] (arg list): st list *)
  | Extern of string * string option * arg * arg list * pos_t 
    (* def lang lib arg [typ list] (arg list): st list *)
  | Class of ident * expr list * arg list * statement list * pos_t
  | Extend of ident * statement list * pos_t
  (* | DecoratedFunction of decorator list * statement *)
  | Import of (ident * ident option) list * pos_t
  (* | ImportFrom of expr * ((expr * expr option) list) option *)
(* and decorator = *)
  (* | Decorator of expr * vararg list *)

type ast = 
  | Module of statement list

let sci lst ?sep fn =
  let sep = Option.value sep ~default:", " in
  String.concat ~sep @@ List.map ~f:fn lst

let rec prn_expr ?prn_pos e = 
  let prn_pos = Option.value prn_pos ~default:(fun x -> "") in
  let prn_expr = prn_expr ~prn_pos in 
  let repr, pos = match e with
  | Ellipsis        -> "...", None
  | Bool(b, pos)    -> sprintf "Bool(%b)" b, Some pos
  | Int(i, pos)     -> sprintf "Int(%d)" i, Some pos
  | Float(f, pos)   -> sprintf "Float(%f)" f, Some pos
  | String(s, pos)  -> sprintf "String(%s)" s, Some pos
  | Seq(s, pos)     -> sprintf "Seq(%s)" s, Some pos
  | Id(i, pos)      -> sprintf "%s" i, Some pos
  | Generic(i, pos) -> sprintf "%s" i, Some pos
  | Tuple(els, pos) -> sprintf "Tuple(%s)" (sci els prn_expr), Some pos

  | IfExpr(conde, ife, ele) -> 
    sprintf "If(%s; %s; %s)" (prn_expr conde) (prn_expr ife) (prn_expr ele), None
  | Pipe(els) -> 
    sprintf "Pipe(%s)" (sci els prn_expr), None
  | Binary(lhs, (op, pos), rhs) -> 
    sprintf "%s(%s; %s)" op (prn_expr lhs) (prn_expr rhs), Some pos
  | Unary((op, pos), rhs) -> 
    sprintf "%s(%s)" op (prn_expr rhs), Some pos
  | Index(arre, indices) -> 
    sprintf "Index(%s; %s)" (prn_expr arre) (sci indices prn_expr), None
  | Dot(me, (dote, pos)) -> 
    sprintf "Dot(%s; %s)" (prn_expr me) dote, Some pos
  | Call(calee, args) -> 
    sprintf "Call(%s; %s)" (prn_expr calee) (sci args prn_expr), None
  | TypeOf(whate, pos) -> 
    sprintf "TypeOf(%s)" (prn_expr whate), Some pos
  | Slice(a, b, c, pos) ->
    let a = Option.value_map a ~default:"" ~f:prn_expr in
    let b = Option.value_map b ~default:"" ~f:prn_expr in
    let c = Option.value_map c ~default:"" ~f:prn_expr in
    sprintf "Slice(%s; %s; %s)" a b c, Some pos
  in
  sprintf "%s%s" (Option.value_map pos ~default:"" ~f:prn_pos) repr

and prn_vararg ?prn_pos vararg = 
  let prn_pos = Option.value prn_pos ~default:(fun x -> "") in
  let prn_expr = prn_expr ~prn_pos in 
  let repr, pos = match vararg with
  | Arg((p, pos), o) -> 
    let o = Option.value_map o ~default:"any" ~f:prn_expr in
    sprintf "%s of %s" p o, Some pos
  in
  sprintf "%s%s" (Option.value_map pos ~default:"" ~f:prn_pos) repr

let rec prn_stmt ?prn_pos level st = 
  let pad l = String.make (l * 2) ' ' in

  let prn_pos = Option.value prn_pos ~default:(fun x -> "") in
  let prn_stmt = prn_stmt ~prn_pos (level + 1) in
  let prn_expr = prn_expr ~prn_pos in
  let prn_vararg = prn_vararg ~prn_pos in

  let repr, pos = match st with
  | Pass pos     -> sprintf "Pass", Some pos
  | Break pos    -> sprintf "Break", Some pos
  | Continue pos -> sprintf "Continue", Some pos

  | Statements(stmts) -> 
    sprintf "Statements[\n%s]" (sci ~sep:"\n" stmts prn_stmt), None
  | Exprs(exprs) -> 
    sprintf "Exprs[%s]" (sci [exprs] prn_expr), None
  | Assign(lhs, rhs, shadow) -> 
    sprintf "Asgn%s[%s; %s]" (if shadow then "!" else "") (prn_expr lhs) (prn_expr rhs), None
  | AssignEq(lhs, (op, pos), rhs) -> 
    sprintf "Asgn[%s; %s; %s]" (prn_expr lhs) op (prn_expr rhs), Some pos
  | Print(exprs, pos) -> 
    sprintf "Print[%s]" (sci exprs prn_expr), Some pos
  | Yield(expr, pos) -> 
    sprintf "Yield[%s]" (prn_expr expr), Some pos
  | Return(expr, pos) -> 
    sprintf "Return[%s]" (prn_expr expr), Some pos
  | Type((typ, _), args, pos) -> 
    sprintf "Type[%s; %s]" typ (sci args prn_vararg), Some pos
  | While(conde, stmts, pos) ->
    sprintf "While[%s;\n%s]" (prn_expr conde) (sci ~sep:"\n" stmts prn_stmt), Some pos
  | For(var, itere, stmts, pos) -> 
    sprintf "For[%s; %s;\n%s]" (prn_expr var) (prn_expr itere) (sci ~sep:"\n" stmts prn_stmt), Some pos
  | If(conds) -> 
    sprintf "If[\n%s]" @@ sci ~sep:"\n" conds (
      fun (cond, stmts, _) -> 
        let cond = Option.value_map cond ~default:"_" ~f:prn_expr in
        sprintf "%s%s -> [\n%s]" (pad (level + 1)) cond 
                                 (sci ~sep:"\n" stmts prn_stmt)), None
  | Match(var, cases, pos) -> 
    sprintf "Match[%s;\n%s]" (prn_expr var) @@ sci ~sep:"\n" cases (
      fun (e, v, stmts, _) ->
        let pv = Option.value_map v ~default:"" ~f:(fun (e, _) -> " AS " ^ e) in
        let pe = Option.value_map e ~default:"DEFUALT" ~f:prn_expr in
        sprintf "%s%s%s -> [\n%s]" (pad (level + 1)) pe pv
                                   (sci ~sep:"\n" stmts prn_stmt)), Some pos
  | Function(name, generics, params, stmts, pos) -> 
    sprintf "Def<%s>[%s; %s;\n%s]" (sci generics prn_expr) (prn_vararg name) (sci params prn_vararg) 
                                   (sci ~sep:"\n" stmts prn_stmt), Some pos
  | Extern(lang, dylib, name, params, pos) -> 
    let dylib = Option.value_map dylib ~default:"" ~f:(fun s -> ", " ^ s) in
    sprintf "Extern<%s%s>[%s; %s]" lang dylib (prn_vararg name) (sci params prn_vararg), Some pos
  | Class((name, _), generics, params, stmts, pos) -> 
    sprintf "Class<%s>[%s; %s;\n%s]" (sci generics prn_expr ) name (sci params prn_vararg) 
                                     (sci ~sep:"\n" stmts prn_stmt), Some pos
  | Extend((name, _), stmts, pos) -> 
    sprintf "Extend[%s;\n%s]" name (sci ~sep:"\n" stmts prn_stmt), Some pos
  | Import(libraries, pos) -> 
    sprintf "Import[%s]" @@ sci libraries (fun ((a, _), b) ->
      Option.value_map b ~default:a ~f:(fun (b, _) -> a ^ " as " ^ b)), Some pos
  in 
  sprintf "%s%s%s" (pad level) (Option.value_map pos ~default:"" ~f:prn_pos) repr

let prn_ast ?prn_pos ast =
  let prn_pos = Option.value prn_pos ~default:(fun x -> "") in
  match ast with 
  | Module stmts -> sci ~sep:"\n" stmts (prn_stmt ~prn_pos 0)
