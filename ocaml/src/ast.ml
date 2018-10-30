(* 786 *)

open Core

exception NotImplentedError of string

type pos_t = Lexing.position
type ident = string * pos_t

type expr = [
  | `None of pos_t
  | `Bool of bool * pos_t
  | `Int of int * pos_t
  | `Float of float * pos_t
  | `String of ident
  | `Id of ident
  | `Seq of ident
  (* | Regex of string *)

  | `Generic of ident
  | `TypeOf of expr * pos_t 

  | `Tuple of expr list * pos_t
  | `List of expr list * pos_t
  | `Set of expr list * pos_t
  | `Dict of (expr * expr) list * pos_t
  (* | `Generator of expr * expr *)
  (* | `ListGenerator of expr * expr *)
  (* | `SetGenerator of expr * expr *)
  (* | `DictGenerator of (expr * expr) * expr *)

  | `IfExpr of expr * expr * expr * pos_t
  (* | `Lambda of vararg list * expr  *)

  | `Pipe of expr list * pos_t
  | `Unary of ident * expr
  | `Binary of expr * ident * expr
  | `Index of expr * expr list * pos_t
  | `Slice of expr option * expr option * expr option * pos_t
  | `Dot of expr * ident * pos_t
  | `Call of expr * expr list  * pos_t

  (* | `Comprehension of expr list * expr list * expr option *)
  (* | `ComprehensionIf of expr *)
  | `Ellipsis of pos_t 
]
and arg = [
  | `Arg of ident * expr option 
  (* | `NamedArg of string * expr *)
]

type statement = [
  | `Pass of pos_t 
  | `Break of pos_t 
  | `Continue of pos_t
  | `Statements of statement list
  | `Exprs of expr
  | `Assign of expr list * expr list * bool * pos_t
  | `Del of expr list * pos_t
  | `Print of expr list * pos_t
  | `Return of expr option * pos_t
  | `Yield of expr option * pos_t
  (* | `Global of expr list  *)
  (* | `Assert of expr list *)
  | `Type of ident * arg list * pos_t
  | `While of expr * statement list * pos_t
  | `For of expr * expr * statement list * pos_t
  | `If of (expr option * statement list * pos_t) list * pos_t
  | `Match of expr * (pattern * statement list * pos_t) list * pos_t
  | `Function of arg * expr list * arg list * statement list * pos_t 
    (* def arg [typ list] (arg list): st list *)
  | `Extern of string * string option * arg * arg list * pos_t 
    (* def lang lib arg [typ list] (arg list): st list *)
  | `Class of ident * expr list * arg list * statement list * pos_t
  | `Extend of ident * statement list * pos_t
  (* | `DecoratedFunction of decorator list * statement *)
  | `Import of (ident * ident option) list * pos_t
  (* | `ImportFrom of expr * ((expr * expr option) list) option *)
(* and decorator = *)
  (* | `Decorator of expr * vararg list *)
] 
and pattern = [
  | `WildcardPattern of ident option
  | `StarPattern
  | `BoundPattern of ident * pattern
  | `IntPattern of int
  | `BoolPattern of bool
  | `StrPattern of string
  | `SeqPattern of string
  | `TuplePattern of pattern list
  | `ListPattern of pattern list
  | `RangePattern of int * int
  | `OrPattern of pattern list
  | `GuardedPattern of pattern * expr
]

type ast = 
  | Module of statement list

let sci lst ?sep fn =
  let sep = Option.value sep ~default:", " in
  String.concat ~sep @@ List.map ~f:fn lst

let rec prn_expr ?prn_pos e = 
  let prn_pos = Option.value prn_pos ~default:(fun _ -> "") in
  let prn_expr = prn_expr ~prn_pos in 
  let repr, pos = match e with
  | `None(pos)       -> "None", pos
  | `Ellipsis(pos)   -> "...", pos
  | `Bool(b, pos)    -> sprintf "Bool(%b)" b, pos
  | `Int(i, pos)     -> sprintf "Int(%d)" i, pos
  | `Float(f, pos)   -> sprintf "Float(%f)" f, pos
  | `String(s, pos)  -> sprintf "String(%s)" s, pos
  | `Seq(s, pos)     -> sprintf "Seq(%s)" s, pos
  | `Id(i, pos)      -> sprintf "%s" i, pos
  | `Generic(i, pos) -> sprintf "%s" i, pos
  | `Tuple(els, pos) -> sprintf "Tuple(%s)" (sci els prn_expr), pos
  | `List(els, pos)  -> sprintf "List(%s)" (sci els prn_expr), pos
  | `Set(els, pos)   -> sprintf "Set(%s)" (sci els prn_expr), pos
  | `Dict(els, pos)  -> sprintf "Dict(%s)" (sci els (fun (a, b) -> 
                        sprintf "%s:%s" (prn_expr a) (prn_expr b))), 
                       pos

  | `IfExpr(conde, ife, ele, pos) -> 
    sprintf "If(%s; %s; %s)" (prn_expr conde) (prn_expr ife) (prn_expr ele), pos
  | `Pipe(els, pos) -> 
    sprintf "Pipe(%s)" (sci els prn_expr), pos
  | `Binary(lhs, (op, pos), rhs) -> 
    sprintf "%s(%s; %s)" op (prn_expr lhs) (prn_expr rhs), pos
  | `Unary((op, pos), rhs) -> 
    sprintf "%s(%s)" op (prn_expr rhs), pos
  | `Index(arre, indices, pos) -> 
    sprintf "Index(%s; %s)" (prn_expr arre) (sci indices prn_expr), pos
  | `Dot(me, (dote, _), pos) -> 
    sprintf "Dot(%s; %s)" (prn_expr me) dote, pos
  | `Call(calee, args, pos) -> 
    sprintf "Call(%s; %s)" (prn_expr calee) (sci args prn_expr), pos
  | `TypeOf(whate, pos) -> 
    sprintf "TypeOf(%s)" (prn_expr whate), pos
  | `Slice(a, b, c, pos) ->
    let a = Option.value_map a ~default:"" ~f:prn_expr in
    let b = Option.value_map b ~default:"" ~f:prn_expr in
    let c = Option.value_map c ~default:"" ~f:prn_expr in
    sprintf "Slice(%s; %s; %s)" a b c, pos
  in
  sprintf "%s%s" (prn_pos pos) repr

and prn_vararg ?prn_pos vararg = 
  let prn_pos = Option.value prn_pos ~default:(fun _ -> "") in
  let prn_expr = prn_expr ~prn_pos in 
  let repr, pos = match vararg with
  | `Arg((p, pos), o) -> 
    let o = Option.value_map o ~default:"any" ~f:prn_expr in
    sprintf "%s of %s" p o, pos
  in
  sprintf "%s%s" (prn_pos pos) repr

let rec prn_stmt ?prn_pos level st = 
  let pad l = String.make (l * 2) ' ' in

  let prn_pos = Option.value prn_pos ~default:(fun _ -> "") in
  let prn_stmt = prn_stmt ~prn_pos (level + 1) in
  let prn_expr = prn_expr ~prn_pos in
  let prn_vararg = prn_vararg ~prn_pos in

  let rec prn_pat = function 
  | `WildcardPattern(None) -> "DEFAULT"
  | `WildcardPattern(Some(o, _)) -> sprintf "DEFAULT(%s)" o 
  | `BoundPattern((i, _), p) -> sprintf "%s AS %s" (prn_pat p) i
  | `IntPattern i -> sprintf "%d" i
  | `BoolPattern b -> sprintf "%b" b
  | `StrPattern s -> sprintf "'%s'" s
  | `SeqPattern s -> sprintf "s'%s'" s
  | `TuplePattern tl -> sprintf "(%s)" (sci tl prn_pat)
  | `ListPattern tl -> sprintf "[%s]" (sci tl prn_pat)
  | `RangePattern (i, j) -> sprintf "%d...%d" i j
  | `OrPattern pl -> sci ~sep:" | " pl prn_pat 
  | `GuardedPattern(p, expr) -> sprintf "%s IF %s" (prn_pat p) (prn_expr expr)
  | `StarPattern -> "..." in

  let repr, pos = match st with
  | `Pass pos     -> sprintf "Pass", Some pos
  | `Break pos    -> sprintf "Break", Some pos
  | `Continue pos -> sprintf "Continue", Some pos

  | `Statements(stmts) -> 
    sprintf "Statements[\n%s]" (sci ~sep:"\n" stmts prn_stmt), None
  | `Exprs(exprs) -> 
    sprintf "Exprs[%s]" (sci [exprs] prn_expr), None
  | `Assign(lhs, rhs, shadow, pos) -> 
    sprintf "Asgn%s[%s; %s]" (if shadow then "!" else "") 
      (sci lhs prn_expr) 
      (sci rhs prn_expr), Some pos
  | `Print(exprs, pos) -> 
    sprintf "Print[%s]" (sci exprs prn_expr), Some pos
  | `Del(exprs, pos) -> 
    sprintf "Del[%s]" (sci exprs prn_expr), Some pos
  | `Yield(expr, pos) -> 
    sprintf "Yield[%s]" (Option.value_map expr ~default:"" ~f:prn_expr), Some pos
  | `Return(expr, pos) -> 
    sprintf "Return[%s]" (Option.value_map expr ~default:"" ~f:prn_expr), Some pos
  | `Type((typ, _), args, pos) -> 
    sprintf "Type[%s; %s]" typ (sci args prn_vararg), Some pos
  | `While(conde, stmts, pos) ->
    sprintf "While[%s;\n%s]" (prn_expr conde) (sci ~sep:"\n" stmts prn_stmt), Some pos
  | `For(var, itere, stmts, pos) -> 
    sprintf "For[%s; %s;\n%s]" (prn_expr var) (prn_expr itere) (sci ~sep:"\n" stmts prn_stmt), Some pos
  | `If(conds, pos) -> 
    sprintf "If[\n%s]" @@ sci ~sep:"\n" conds (
      fun (cond, stmts, _) -> 
        let cond = Option.value_map cond ~default:"_" ~f:prn_expr in
        sprintf "%s%s -> [\n%s]" (pad (level + 1)) cond 
                                 (sci ~sep:"\n" stmts prn_stmt)), Some pos
  | `Match(var, cases, pos) -> 
    sprintf "Match[%s;\n%s]" (prn_expr var) @@ sci ~sep:"\n" cases (
      fun (pat, stmts, _) -> 
        sprintf "%s%s -> [\n%s]" (pad (level + 1)) (prn_pat pat)
                                 (sci ~sep:"\n" stmts prn_stmt)), Some pos
  | `Function(name, generics, params, stmts, pos) -> 
    sprintf "Def<%s>[%s; %s;\n%s]" (sci generics prn_expr) (prn_vararg name) (sci params prn_vararg) 
                                   (sci ~sep:"\n" stmts prn_stmt), Some pos
  | `Extern(lang, dylib, name, params, pos) -> 
    let dylib = Option.value_map dylib ~default:"" ~f:(fun s -> ", " ^ s) in
    sprintf "Extern<%s%s>[%s; %s]" lang dylib (prn_vararg name) (sci params prn_vararg), Some pos
  | `Class((name, _), generics, params, stmts, pos) -> 
    sprintf "Class<%s>[%s; %s;\n%s]" (sci generics prn_expr ) name (sci params prn_vararg) 
                                     (sci ~sep:"\n" stmts prn_stmt), Some pos
  | `Extend((name, _), stmts, pos) -> 
    sprintf "Extend[%s;\n%s]" name (sci ~sep:"\n" stmts prn_stmt), Some pos
  | `Import(libraries, pos) -> 
    sprintf "Import[%s]" @@ sci libraries (fun ((a, _), b) ->
      Option.value_map b ~default:a ~f:(fun (b, _) -> a ^ " as " ^ b)), Some pos
  in 
  sprintf "%s%s%s" (pad level) (Option.value_map pos ~default:"" ~f:prn_pos) repr

let prn_ast ?prn_pos ast =
  let prn_pos = Option.value prn_pos ~default:(fun x -> "") in
  match ast with 
  | Module stmts -> sci ~sep:"\n" stmts (prn_stmt ~prn_pos 0)
