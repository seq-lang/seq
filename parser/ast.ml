(******************************************************************************
 *
 * Seq OCaml 
 * ast.ml: AST type definitions
 *
 * Author: inumanag
 *
 ******************************************************************************)

open Core
open Util

(** File position descriptor for an AST node. Sexpable. *)
module Pos = 
struct
  type t = 
    { file: string;
      line: int;
      col: int;
      len: int }

  (** Creates dummy position for internal classes *)
  let dummy =
    { file = ""; line = -1; col = -1; len = 0 }
end

(** Expression AST node.
    Each node [t] is a 2-tuple that stores 
    (1) position [Pos] within a file, and
    (2) node data [node].
    Sexpable. *)
module ExprNode = 
struct 
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
    | Unpack         of string
    | Tuple          of t list
    | List           of t list
    | Set            of t list
    | Dict           of (t * t) list
    | Generator      of (t * comprehension tt)
    | ListGenerator  of (t * comprehension tt)
    | SetGenerator   of (t * comprehension tt)
    | DictGenerator  of ((t * t) * comprehension tt)
    | IfExpr         of (t * t * t)
    | Unary          of (string * t)
    | Binary         of (t * string * t)
    | Pipe           of t list
    | Index          of (t * t list)
    | Call           of (t * call tt list)
    | Slice          of (t option * t option * t option)
    | Dot            of (t * string)
    | Ellipsis       of unit
    | TypeOf         of t
    | Ptr            of t
    | Lambda         of (string tt list * t)
  and generic = 
    string
  and call =
    { name: string option;
      value: t }
  and comprehension =
    { var: string list; 
      gen: t; 
      cond: t option; 
      next: (comprehension tt) option }

  let rec to_string (_, el) =
    match el with 
    | Empty _ -> ""
    | Ellipsis _ -> "..."
    | Bool x -> sprintf "bool(%b)" x
    | Int x -> sprintf "%d" x
    | Float x -> sprintf "%.2f" x
    | String x -> sprintf "'%s'" (String.escaped x)
    | Seq x -> sprintf "seq('%s')" x
    | Id x -> sprintf "%s" x
    | Generic x -> sprintf "`%s" x
    | Unpack x -> sprintf "*%s" x
    | Tuple l -> sprintf "tuple(%s)" (ppl l ~f:to_string)
    | List l -> sprintf "list(%s)" (ppl l ~f:to_string)
    | Set l -> sprintf "set(%s)" (ppl l ~f:to_string)
    | Dict l -> sprintf "dict(%s)" @@ ppl l 
        ~f:(fun (a, b) -> sprintf "%s: %s" (to_string a) (to_string b))
    | IfExpr (x, i, e) -> sprintf "%s IF %s ELSE %s" 
        (to_string x) (to_string i) (to_string e)
    | Pipe l -> sprintf "%s" (ppl l ~sep:" |> " ~f:to_string)
    | Binary (l, o, r) -> sprintf "(%s %s %s)" (to_string l) o (to_string r)
    | Unary (o, x) -> sprintf "(%s %s)" o (to_string x)
    | Index (x, l) -> sprintf "%s[%s]" (to_string x) (ppl l ~f:to_string)
    | Dot (x, s) -> sprintf "%s.%s" (to_string x) s
    | Call (x, l) -> sprintf "%s(%s)" (to_string x) (ppl l ~f:call_to_string)
    | TypeOf x -> sprintf "TYPEOF(%s)" (to_string x)
    | Ptr x -> sprintf "PTR(%s)" (to_string x)
    | Slice (a, b, c) ->
      let l = List.map [a; b; c] ~f:(Option.value_map ~default:"" ~f:to_string)
      in
      sprintf "slice(%s)" (ppl l ~f:Fn.id)
    | Generator (x, c) -> 
      sprintf "(%s %s)" (to_string x) (comprehension_to_string c)
    | ListGenerator (x, c) ->
      sprintf "[%s %s]" (to_string x) (comprehension_to_string c)
    | SetGenerator (x, c) ->
      sprintf "{%s %s}" (to_string x) (comprehension_to_string c)
    | DictGenerator ((x1, x2), c) ->
      sprintf "{%s: %s %s}" (to_string x1) (to_string x2) 
        (comprehension_to_string c)
    | Lambda (l, x) -> sprintf "lambda (%s): %s" (ppl l ~f:snd) (to_string x)
  and call_to_string (_, { name; value }) =
    sprintf "%s%s" 
      (Option.value_map name ~default:"" ~f:(fun x -> x ^ " = "))
      (to_string value)
  and comprehension_to_string (_, { var; gen; cond; next }) =
    sprintf "FOR %s IN %s%s%s" (ppl var ~f:Fn.id) 
      (to_string gen)
      (Option.value_map cond ~default:"" ~f:(fun x -> 
        sprintf "IF %s" (to_string x)))
      (Option.value_map next ~default:"" ~f:(fun x ->
        " " ^ (comprehension_to_string x)))
end 

(** Statement AST node.
    Each node [t] is a 2-tuple that stores 
    (1) position [Pos] within a file, and
    (2) node data [node].
    Sexpable. *)
module StmtNode = 
struct
  type 'a tt = 
    Pos.t * 'a
  and t =
    node tt
  and node = 
    | Pass       of unit
    | Break      of unit
    | Continue   of unit
    | Expr       of ExprNode.t
    | Assign     of (ExprNode.t * ExprNode.t * bool)
    | Del        of ExprNode.t
    | Print      of ExprNode.t
    | Return     of ExprNode.t option
    | Yield      of ExprNode.t option
    | Assert     of ExprNode.t
    | Type       of (string * param tt list)
    | While      of (ExprNode.t * t list)
    | For        of (string list * ExprNode.t * t list)
    | If         of (if_case tt) list
    | Match      of (ExprNode.t * (match_case tt) list)
    | Extend     of (string * generic tt list)
    | Extern     of (string * string option * string * param tt * param tt list)
    | Import     of import list 
    | Generic    of generic 
    | Try        of (t list * catch tt list * t list option)
    | Global     of string
    | Throw      of ExprNode.t
  and generic =
    | Function of 
        (param tt * (ExprNode.generic tt) list * param tt list * t list)
    | Class of class_t 
  and if_case = 
    { cond: ExprNode.t option; 
      cond_stmts: t list }
  and match_case = 
    { pattern: pattern;
      case_stmts: t list }
  and param = 
    { name: string; 
      typ: ExprNode.t option; }
  and catch = 
    { exc: string option;
      var: string option;
      stmts: t list }
  and class_t = 
    { class_name: string;
      generics: (ExprNode.generic tt) list;
      args: param tt list option;
      members: generic tt list }
  and import = 
    (* import <from> as <import_as> | from <from> import <what> 
       <what> = [<what> as <import_as>]+
       import! := import + <stdlib> = true *)
    { from: string tt; 
      (* what:  *)
      what: (string * string option) tt list option;
      import_as: string option;
      stdlib: bool }
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
    | GuardedPattern  of (pattern * ExprNode.t)
    | BoundPattern    of (string * pattern)
  
  let rec to_string ?(indent=0) (_, s) = 
    let s = match s with
    | Pass _ -> sprintf "PASS"
    | Break _ -> sprintf "BREAK"
    | Continue _ -> sprintf "CONTINUE"
    | Expr x -> 
      ExprNode.to_string x
    | Assign (l, r, s) -> 
      sprintf "%s %s %s" 
        (ExprNode.to_string l) (if s then ":=" else "=") (ExprNode.to_string r)
    | Print x -> sprintf "PRINT %s" (ExprNode.to_string x)
    | Del x -> sprintf "DEL %s" (ExprNode.to_string x)
    | Assert x -> sprintf "ASSERT %s" (ExprNode.to_string x)
    | Yield x -> sprintf "YIELD %s"
        (Option.value_map x ~default:"" ~f:ExprNode.to_string)
    | Return x -> sprintf "RETURN %s"
        (Option.value_map x ~default:"" ~f:ExprNode.to_string)
    | Type (x, l) -> 
      sprintf "TYPE %s (%s)" x (ppl l ~f:param_to_string)
    | While (x, l) ->
      sprintf "WHILE %s:\n%s" 
        (ExprNode.to_string x) 
        (ppl l ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))
    | For (v, x, l) -> 
      sprintf "FOR %s IN %s:\n%s" 
        (ppl v ~f:Fn.id) 
        (ExprNode.to_string x) 
        (ppl l ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))
    | If l -> 
      String.concat ~sep:"\n" @@ List.mapi l 
        ~f:(fun i (_, { cond; cond_stmts }) ->
          let cond = Option.value_map cond ~default:"" ~f:ExprNode.to_string in
          let case = 
            if i = 0 then "IF " else if cond = "" then "ELSE" else "ELIF " 
          in
          sprintf "%s%s:\n%s"
            case cond
            (ppl cond_stmts ~sep:"\n" ~f:(to_string ~indent:(indent + 1))))
    | Match (x, l) -> 
      sprintf "MATCH %s:\n%s" 
        (ExprNode.to_string x) 
        (ppl l ~sep:"\n" ~f:(fun (_, { pattern; case_stmts }) -> 
          sprintf "case <?>:\n%s" 
            (ppl case_stmts ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))))
    | _ -> "?"
    in
    let pad l = String.make (l * 2) ' ' in
    sprintf "%s%s" (pad indent) s
  and param_to_string (_, { name; typ }) =
    let typ = Option.value_map typ ~default:"" ~f:(fun x ->
      " : " ^ (ExprNode.to_string x))
    in
    sprintf "%s%s" name typ
    (* | `Function(name, generics, params, stmts) -> 
      sprintf "Def<%s>[%s; %s;\n%s]" (ppl generics ExprNode.to_string) (prn_vararg name) (ppl params prn_vararg) 
                                     (ppl ~sep:"\n" stmts prn_stmt)
    | `Extern(lang, dylib, name, params) -> 
      let dylib = Option.value_map dylib ~default:"" ~f:(fun s -> ", " ^ s) in
      sprintf "Extern<%s%s>[%s; %s]" lang dylib (prn_vararg name) (ppl params prn_vararg)
    | `Class((name, _), generics, params, stmts) -> 
      sprintf "Class<%s>[%s; %s;\n%s]" (ppl generics ExprNode.to_string ) name (ppl params prn_vararg) 
                                       (ppl ~sep:"\n" stmts prn_stmt)
    | `Extend((name, _), stmts) -> 
      sprintf "Extend[%s;\n%s]" name (ppl ~sep:"\n" stmts prn_stmt)
    | `Import(libraries) -> 
      sprintf "Import[%s]" @@ ppl libraries (fun ((a, _), b) ->
        Option.value_map b ~default:a ~f:(fun (b, _) -> a ^ " as " ^ b)) *)
end

(** Module AST node.
    Currently just a list of statements. Sexpable. *)
type t = 
  | Module of StmtNode.t list

let to_string = function 
  | Module l ->
    ppl l ~sep:"\n" ~f:StmtNode.to_string
