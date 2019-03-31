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
    | Int            of string
    | IntS           of (string * string)
    | Float          of float
    | FloatS         of (float * string)
    | String         of string
    | Seq            of string
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
    | Pipe           of (string * t) list
    | Index          of (t * t)
    | Call           of (t * call tt list)
    | Slice          of (t option * t option * t option)
    | Dot            of (t * string)
    | Ellipsis       of unit
    | TypeOf         of t
    | Ptr            of t
    | Lambda         of (string tt list * t)
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
    | Bool x -> if x then "True" else "False"
    | Int x -> sprintf "%s" x
    | IntS (x, k) -> sprintf "%s%s" x k
    | Float x -> sprintf "%f" x
    | FloatS (x, k) -> sprintf "%f%s" x k
    | String x -> sprintf "'%s'" (String.escaped x)
    | Seq x -> sprintf "s'%s'" x
    | Id x -> sprintf "%s" x
    | Unpack x -> sprintf "*%s" x
    | Tuple l -> sprintf "(%s)" (ppl l ~f:to_string)
    | List l -> sprintf "[%s]" (ppl l ~f:to_string)
    | Set l -> sprintf "{%s}" (ppl l ~f:to_string)
    | Dict l -> sprintf "{%s}" @@ ppl l 
        ~f:(fun (a, b) -> sprintf "%s: %s" (to_string a) (to_string b))
    | IfExpr (x, i, e) -> sprintf "%s if %s else %s" 
        (to_string x) (to_string i) (to_string e)
    | Pipe l -> sprintf "%s" (ppl l ~sep:"" ~f:(fun (p, e) -> 
        sprintf "%s %s" p @@ to_string e))
    | Binary (l, o, r) -> sprintf "(%s %s %s)" (to_string l) o (to_string r)
    | Unary (o, x) -> sprintf "(%s %s)" o (to_string x)
    | Index (x, l) -> sprintf "%s[%s]" (to_string x) (to_string l)
    | Dot (x, s) -> sprintf "%s.%s" (to_string x) s
    | Call (x, l) -> sprintf "%s(%s)" (to_string x) (ppl l ~f:call_to_string)
    | TypeOf x -> sprintf "typeof(%s)" (to_string x)
    | Ptr x -> sprintf "ptr(%s)" (to_string x)
    | Slice (a, b, c) ->
      let l = List.map [a; b; c] ~f:(Option.value_map ~default:"" ~f:to_string)
      in
      sprintf "%s" (ppl l ~sep:":" ~f:Fn.id)
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
    sprintf "for %s in %s%s%s" (ppl var ~f:Fn.id) 
      (to_string gen)
      (Option.value_map cond ~default:"" ~f:(fun x -> 
        sprintf "if %s" (to_string x)))
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
    | Pass        of unit
    | Break       of unit
    | Continue    of unit
    | Expr        of ExprNode.t
    (* lhs, rhs, type*)
    | Assign      of (ExprNode.t * ExprNode.t * bool * ExprNode.t option) 
    | Del         of ExprNode.t
    | Print       of (ExprNode.t list * string)
    | Return      of ExprNode.t option
    | Yield       of ExprNode.t option
    | Assert      of ExprNode.t
    | TypeAlias   of (string * ExprNode.t)
    | While       of (ExprNode.t * t list)
    | For         of (string list * ExprNode.t * t list)
    | If          of (if_case tt) list
    | Match       of (ExprNode.t * (match_case tt) list)
    | Extend      of (ExprNode.t * generic tt list)
    | Extern      of (string * string option * string * param tt * param tt list)
    | Import      of import list 
    | ImportPaste of string
    | Generic     of generic 
    | Try         of (t list * catch tt list * t list option)
    | Global      of string
    | Throw       of ExprNode.t
    | Prefetch    of ExprNode.t list
    | Special     of (string * t list * string list)
  and generic =
    | Function of fn_t
    | Class    of class_t 
    | Type     of class_t
    | Declare  of param
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
      generics: (string tt) list;
      args: param tt list option;
      members: generic tt list }
  and fn_t = 
    { fn_name: param;
      fn_generics: (string tt) list;
      fn_args: param tt list;
      fn_stmts: t list;
      fn_attrs: string tt list }
  and import = 
    (* import <from> as <import_as> | from <from> import <what> 
       <what> = [<what> as <import_as>]+
       import! := import + <stdlib> = true *)
    { from: string tt; 
      (* what:  *)
      what: (string * string option) tt list option;
      import_as: string option }
  and pattern = 
    | StarPattern
    | IntPattern      of int64
    | BoolPattern     of bool
    | StrPattern      of string
    | SeqPattern      of string
    | RangePattern    of (int64 * int64)
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
    | Assign(l, r, s, q) -> 
      begin match q with 
        | Some q ->
          sprintf "%s : %s %s %s"
            (ExprNode.to_string l) (ExprNode.to_string q) 
            (if s then ":=" else "=") (ExprNode.to_string r)
        | None -> 
          sprintf "%s %s %s"
            (ExprNode.to_string l) (if s then ":=" else "=") (ExprNode.to_string r)
      end
    | Print(x, n) -> sprintf "PRINT %s, %s" 
        (ppl x ~f:ExprNode.to_string) 
        (String.escaped n)
    | Del x -> sprintf "DEL %s" (ExprNode.to_string x)
    | Assert x -> sprintf "ASSERT %s" (ExprNode.to_string x)
    | Yield x -> sprintf "YIELD %s"
        (Option.value_map x ~default:"" ~f:ExprNode.to_string)
    | Return x -> sprintf "RETURN %s"
        (Option.value_map x ~default:"" ~f:ExprNode.to_string)
    | TypeAlias (x, l) -> 
      sprintf "TYPE %s = %s" x (ExprNode.to_string l)
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
end

(** Module AST node.
    Currently just a list of statements. Sexpable. *)
type t = 
  | Module of StmtNode.t list

let to_string = function 
  | Module l ->
    ppl l ~sep:"\n" ~f:StmtNode.to_string
