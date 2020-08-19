(* *****************************************************************************
 * Seqaml.Ast_pos: AST annotations
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

(** Annotated type specification. *)
type 'a ann = (Lexing.position * Lexing.position) * 'a

exception SyntaxError of string * Lexing.position

type texpr =
  | Empty of unit
  | Bool of bool
  | Int of (string * string)
  | Float of (float * string)
  | String of string
  | FString of string
  | Kmer of string
  | Seq of (string * string)
  | Id of string
  | Unpack of texpr ann
  | Tuple of texpr ann list
  | List of texpr ann list
  | Set of texpr ann list
  | Dict of (texpr ann * texpr ann) list
  | Generator of (texpr ann * tcomprehension ann)
  | ListGenerator of (texpr ann * tcomprehension ann)
  | SetGenerator of (texpr ann * tcomprehension ann)
  | DictGenerator of ((texpr ann * texpr ann) * tcomprehension ann)
  | IfExpr of (texpr ann * texpr ann * texpr ann)
  | Unary of (string * texpr ann)
  | Binary of (texpr ann * string * texpr ann)
  | Pipe of (string * texpr ann) list
  | Index of (texpr ann * texpr ann)
  | Call of (texpr ann * (string option * texpr ann) list)
  | Slice of (texpr ann option * texpr ann option * texpr ann option)
  | Dot of (texpr ann * string)
  | Ellipsis of unit
  | TypeOf of texpr ann
  | Ptr of texpr ann
  | Lambda of (string list * texpr ann)
  | YieldTo of unit

and tcomprehension =
  { var : texpr ann
  ; gen : texpr ann
  ; cond : texpr ann list
  ; next : tcomprehension ann option
  }

type tstmt =
  | Pass of unit
  | Break of unit
  | Continue of unit
  | Expr of texpr ann
  | Assign of (texpr ann * texpr ann * texpr ann option)
  | Del of texpr ann
  | Print of texpr ann
  | Return of texpr ann option
  | Yield of texpr ann option
  | Assert of texpr ann
  | TypeAlias of (string * texpr ann)
  | While of (texpr ann * tstmt ann list)
  | For of (texpr ann * texpr ann * tstmt ann list)
  | If of (texpr ann option * tstmt ann list) list
  | Match of (texpr ann * (pattern ann * tstmt ann list) list)
  | Extend of (texpr ann * tstmt ann list)
  | Import of ((string * string option) * (string * string option) list)
  | ImportExtern of eimport
  | Try of (tstmt ann list * catch ann list * tstmt ann list)
  | Global of string
  | Throw of texpr ann
  | Prefetch of texpr ann
  | Special of (string * tstmt ann list * string list)
  | Function of fn_t
  | Class of class_t
  | Type of class_t
  | Declare of param ann
  | AssignEq of (texpr ann * texpr ann * string)
  | YieldFrom of texpr ann
  | With of ((texpr ann * string option) list * tstmt ann list)
  | PyDef of (string * texpr ann option * param ann list * string)

and eimport =
  { lang : string
  ; e_from : texpr ann option
  ; e_name : string
  ; e_typ : texpr ann
  ; e_args : param ann list
  ; e_as : string option
  }

and catch =
  { exc : texpr ann option
  ; var : string option
  ; stmts : tstmt ann list
  }

and param =
  { name : string
  ; typ : texpr ann option
  ; default : texpr ann option
  }

and fn_t =
  { fn_name : string
  ; fn_rettyp : texpr ann option
  ; fn_generics : param ann list
  ; fn_args : param ann list
  ; fn_stmts : tstmt ann list
  ; fn_attrs : string ann list
  }

and class_t =
  { class_name : string
  ; generics : param ann list
  ; args : param ann list
  ; members : tstmt ann list
  ; attrs : string ann list
  }

and pattern =
  | StarPattern of unit
  | IntPattern of int64
  | BoolPattern of bool
  | StrPattern of string
  | SeqPattern of string
  | RangePattern of (int64 * int64)
  | TuplePattern of pattern ann list
  | ListPattern of pattern ann list
  | OrPattern of pattern ann list
  | WildcardPattern of string option
  | GuardedPattern of (pattern ann * texpr ann)
  | BoundPattern of (string * pattern ann)

let flat_pipe x =
  match x with
  | _, [] -> failwith "empty pipeline expression (grammar)"
  | _, [ h ] -> snd h
  | pos, l -> pos, Pipe l

(* Converts list of conditionals into the AND AST node
   (used for chained conditionals such as
   0 < x < y < 10 that becomes (0 < x) AND (x < y) AND (y < 10)) *)
type cond_t =
  | Cond of texpr
  | CondBinary of (texpr ann * string * cond_t ann)

let rec flat_cond x =
  let expr =
    match snd x with
    | CondBinary (lhs, op, ((_, CondBinary (next_lhs, _, _)) as rhs)) ->
      Binary ((fst lhs, Binary (lhs, op, next_lhs)), "&&", flat_cond rhs)
    | CondBinary (lhs, op, (pos, Cond rhs)) -> Binary (lhs, op, (pos, rhs))
    | Cond n -> n
  in
  fst x, expr

let rec flatten_dot ~sep = function
  | _, Id s -> s
  | _, Dot (d, s) -> Printf.sprintf "%s%s%s" (flatten_dot ~sep d) sep s
  | _ -> failwith "invalid import construct (grammar)"

let ppl ?(sep = ", ") ~f l =
  String.concat sep (List.map f l)

let opt_val s def =
  match s with Some s -> s | None -> def

let opt_map f def = function Some s -> f s | None -> def

let filter_opt s =
  List.rev @@ List.fold_left (fun acc i -> match i with Some s -> s::acc | None -> acc) [] s
