(* 786 *)

open Core

module Pos = 
struct
  type t = 
    { file: string;
      line: int;
      col: int;
      len: int }
  
  let dummy =
    { file = ""; line = -1; col = -1; len = 0 }
end

let sci lst ?sep fn =
  let sep = Option.value sep ~default:", " in
  String.concat ~sep @@ List.map ~f:fn lst

module ExprNode = 
struct 
  (** Each node is a 2-tuple that stores 
      (1) position within a file and
      (2) node data *)
  type 'a tt = 
    Pos.t * 'a
  and t = 
    node tt
  and node =
    | Empty          of unit
    | Bool           of bool
    | Int            of int
    | Float          of float
    | String         of string
    | Seq            of string
    | Generic        of generic
    | Id             of string
    | Tuple          of t list
    | List           of t list
    | Set            of t list
    | Dict           of (t * t) list
    | ListGenerator  of (t * comprehension tt)
    | SetGenerator   of (t * comprehension tt)
    | DictGenerator  of ((t * t) * comprehension tt)
    | IfExpr         of (t * t * t)
    | Unary          of (string * t)
    | Binary         of (t * string * t)
    | Pipe           of t list
    | Index          of (t * t list)
    | Call           of (t * t list)
    | Slice          of (t option * t option * t option)
    | Dot            of (t * string)
    | Ellipsis       of unit
    | TypeOf         of t
    | Lambda         of (string tt list * t)
  and generic = 
    string
  and comprehension =
    { var: string; 
      gen: t; 
      cond: t option; 
      next: (comprehension tt) option }

  let rec to_string ?(prn_pos=(fun _ -> "")) (pos, node) = 
    let to_string = to_string ~prn_pos in 
    let repr = match node with
      | Empty _    -> 
        "None"
      | Ellipsis _ -> 
        "..."
      | Bool(b)    -> 
        sprintf "Bool(%b)" b
      | Int(i)     -> 
        sprintf "Int(%d)" i
      | Float(f)   -> 
        sprintf "Float(%f)" f
      | String(s)  -> 
        sprintf "String(%s)" s
      | Seq(s)     -> 
        sprintf "Seq(%s)" s
      | Id(i)      -> 
        sprintf "%s" i
      | Generic(i) -> 
        sprintf "%s" i
      | Tuple(els) -> 
        sprintf "Tuple(%s)" (sci els to_string)
      | List(els)  -> 
        sprintf "List(%s)" (sci els to_string)
      | Set(els)   -> 
        sprintf "Set(%s)" (sci els to_string)
      | Dict(els)  -> 
        sprintf "Dict(%s)" @@ sci els 
          (fun (a, b) -> sprintf "%s:%s" (to_string a) (to_string b))
      | IfExpr(conde, ife, ele) -> 
        sprintf "If(%s; %s; %s)" 
          (to_string conde) (to_string ife) (to_string ele)
      | Pipe(els) -> 
        sprintf "Pipe(%s)" (sci els to_string)
      | Binary(lhs, (op), rhs) -> 
        sprintf "%s(%s; %s)" 
          op (to_string lhs) (to_string rhs)
      | Unary((op), rhs) -> 
        sprintf "%s(%s)" 
          op (to_string rhs)
      | Index(arre, indices) -> 
        sprintf "Index(%s; %s)" 
          (to_string arre) (sci indices to_string)
      | Dot(me, dote) -> 
        sprintf "Dot(%s; %s)" 
          (to_string me) dote
      | Call(calee, args) -> 
        sprintf "Call(%s; %s)" 
          (to_string calee) (sci args to_string)
      | TypeOf(whate) -> 
        sprintf "TypeOf(%s)" 
          (to_string whate)
      | Slice(a, b, c) ->
        let a = Option.value_map a ~default:"" ~f:to_string in
        let b = Option.value_map b ~default:"" ~f:to_string in
        let c = Option.value_map c ~default:"" ~f:to_string in
        sprintf "Slice(%s; %s; %s)" a b c
      | ListGenerator(r, c) ->
        sprintf "GenList(%s; %s)" 
          (to_string r) (to_string_comprehension c)
      | SetGenerator(r, c) ->
        sprintf "GenSet(%s; %s)" 
          (to_string r) (to_string_comprehension c)
      | DictGenerator((r1, r2), c) ->
        sprintf "GenDict(%s : %s; %s)" 
          (to_string r1) (to_string r2) (to_string_comprehension c)
      | Lambda(params, expr) ->
        sprintf "Lambda(%s; %s)" 
          (sci params snd) (to_string expr)
    in
    sprintf "%s%s" (prn_pos pos) repr
  and to_string_comprehension (pos, {var; gen; cond; next}) = 
    let cond = Option.value_map cond 
      ~default:"" ~f:(fun x -> sprintf "IF %s" (to_string x))
    in
    let next = Option.value_map next ~default:"" ~f:to_string_comprehension in
    sprintf "Comp(%s; %s%s %s)" 
      var (to_string gen) cond next
end 

module StmtNode = 
struct
  (** Each node is a 2-tuple that stores 
      (1) position within a file and
      (2) node data *)
  type et = ExprNode.t

  type 'a tt = 
    Pos.t * 'a
  and t =
    node tt
  and node = 
    | Pass     of unit
    | Break    of unit
    | Continue of unit
    | Expr     of et
    | Assign   of (et list * et list * bool)
    | Del      of et list
    | Print    of et list
    | Return   of et option
    | Yield    of et option
    | Assert   of et list
    | Type     of (string * param tt list)
    | While    of (et * t list)
    | For      of (string list * et * t list)
    | If       of (if_case tt) list
    | Match    of (et * (match_case tt) list)
    | Extend   of (string * generic tt list)
    | Extern   of (string * string option * param tt * param tt list)
    | Import   of (string tt * string option) list 
    | Generic  of generic 
  and generic =
    | Function of 
        (param tt * (ExprNode.generic tt) list * param tt list * t list)
    | Class of 
        (string * (ExprNode.generic tt) list * param tt list * generic tt list)
  and if_case = 
    { cond: et option; 
      stmts: t list }
  and match_case = 
    { pattern: pattern;
      stmts: t list }
  and param = 
    { name: string; 
      typ: et option; }
  and pattern = 
    | StarPattern
    | IntPattern      of int
    | BoolPattern     of bool
    | StrPattern      of string
    | SeqPattern      of string
    | RangePattern    of (int * int)
    | TuplePattern    of pattern list
    | ListPattern     of pattern list
    | OrPattern       of pattern list
    | WildcardPattern of string option
    | GuardedPattern  of (pattern * et)
    | BoundPattern    of (string * pattern)

  let rec to_string ?(level=0) ?(prn_pos=(fun _ -> "")) (pos, node) = 
    let pad l = String.make (l * 2) ' ' in
    let prn_expr = ExprNode.to_string ~prn_pos in
    let prn_stmt = to_string ~prn_pos ~level:(level + 1) in
    let prn_expr = ExprNode.to_string ~prn_pos in
    let rec prn_generic (_, g) =
      let prg = fun (_, x) -> x in
      match g with 
      | Function(name, generics, params, stmts) -> 
        sprintf "Def<%s>[%s; %s;\n%s]" 
          (sci generics prg) (prn_param name) 
          (sci params prn_param) (sci ~sep:"\n" stmts prn_stmt)
      | Class(name, generics, params, stmts) -> 
        sprintf "Class<%s>[%s; %s;\n%s]" 
          (sci generics prg) name (sci params prn_param) 
          (sci ~sep:"\n" stmts prn_generic)
    and prn_param (_, {name; typ}) = 
      let o = Option.value_map typ ~default:"ANY" ~f:prn_expr in
      sprintf "%s : %s" name o
    in
    let repr = match node with
      | Pass _ -> 
        sprintf "Pass"
      | Break _ -> 
        sprintf "Break"
      | Continue _ -> 
        sprintf "Continue"
      | Expr(expr) -> 
        sprintf "Expr[%s]" (prn_expr expr)
      | Assign(lhs, rhs, shadow) -> 
        sprintf "Assign%s[%s; %s]" 
          (if shadow then "!" else "") (sci lhs prn_expr) (sci rhs prn_expr)
      | Print(exprs) -> 
        sprintf "Print[%s]" (sci exprs prn_expr)
      | Del(exprs) -> 
        sprintf "Del[%s]" (sci exprs prn_expr)
      | Assert(exprs) -> 
        sprintf "Assert[%s]" (sci exprs prn_expr)
      | Yield(expr) -> 
        sprintf "Yield[%s]" 
          (Option.value_map expr ~default:"" ~f:prn_expr)
      | Return(expr) -> 
        sprintf "Return[%s]" 
          (Option.value_map expr ~default:"" ~f:prn_expr)
      | Type(typ, args) -> 
        sprintf "Type[%s; %s]" 
          typ (sci args prn_param)
      | While(conde, stmts) ->
        sprintf "While[%s;\n%s]" 
          (prn_expr conde) (sci ~sep:"\n" stmts prn_stmt)
      | For(var, itere, stmts) -> 
        sprintf "For[%s; %s;\n%s]" 
          (sci var Fn.id) (prn_expr itere) (sci ~sep:"\n" stmts prn_stmt)
      | If(conds) -> 
        sprintf "If[\n%s]" @@ sci ~sep:"\n" conds (fun (_, {cond; stmts}) ->
          let cond = Option.value_map cond ~default:"_" ~f:prn_expr in
          sprintf "%s%s -> [\n%s]" 
            (pad (level + 1)) cond (sci ~sep:"\n" stmts prn_stmt))
      | Match(var, cases) -> 
        sprintf "Match[%s;\n%s]" (prn_expr var) @@ 
          sci ~sep:"\n" cases (fun (_, {pattern; stmts}) ->
            sprintf "%s%s -> [\n%s]" 
              (pad (level + 1)) (to_string_pattern pattern) 
              (sci ~sep:"\n" stmts prn_stmt))
      | Extern(lang, dylib, name, params) -> 
        let dylib = 
          Option.value_map dylib ~default:"" ~f:(fun s -> ", " ^ s) 
        in
        sprintf "Extern<%s%s>[%s; %s]" 
          lang dylib (prn_param name) (sci params prn_param)
      | Extend(name, stmts) -> 
        sprintf "Extend[%s;\n%s]" 
          name (sci ~sep:"\n" stmts prn_generic)
      | Import(libraries) -> 
        sprintf "Import[%s]" @@ sci libraries (fun ((_, a), b) ->
          Option.value_map b ~default:a ~f:(fun b -> a ^ " as " ^ b))
      | Generic gen -> prn_generic (pos, gen)
    in 
    sprintf "%s%s%s" (pad level) (prn_pos pos) repr
  and to_string_pattern = function 
    | WildcardPattern(None) -> 
      "DEFAULT"
    | WildcardPattern(Some(o)) -> 
      sprintf "DEFAULT(%s)" o 
    | BoundPattern(i, p) -> 
      sprintf "%s AS %s" (to_string_pattern p) i
    | IntPattern i -> 
      sprintf "%d" i
    | BoolPattern b -> 
      sprintf "%b" b
    | StrPattern s -> 
      sprintf "'%s'" s
    | SeqPattern s -> 
      sprintf "s'%s'" s
    | TuplePattern tl -> 
      sprintf "(%s)" (sci tl to_string_pattern)
    | ListPattern tl -> 
      sprintf "[%s]" (sci tl to_string_pattern)
    | RangePattern (i, j) -> 
      sprintf "%d...%d" i j
    | OrPattern pl -> 
      sci ~sep:" | " pl to_string_pattern 
    | GuardedPattern(p, expr) -> 
      sprintf "%s IF %s" 
        (to_string_pattern p) (ExprNode.to_string expr)
    | StarPattern -> 
      "..." 
end

type t = 
  | Module of StmtNode.t list

(* let prn_ast ?prn_pos ast =
  let prn_pos = Option.value prn_pos ~default:(fun _ -> "") in
  match ast with Module stmts -> sci ~sep:"\n" stmts (prn_stmt ~prn_pos 0)
 *)
