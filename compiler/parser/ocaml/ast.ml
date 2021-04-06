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
  | String of (string * string)
  | Id of string
  | Star of texpr ann
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
  | Binary of (texpr ann * string * texpr ann * bool)
  | Pipe of (string * texpr ann) list
  | Index of (texpr ann * texpr ann)
  | Call of (texpr ann * (string option * texpr ann) list)
  | Slice of (texpr ann option * texpr ann option * texpr ann option)
  | Dot of (texpr ann * string)
  | Ellipsis of unit
  | TypeOf of texpr ann
  | Lambda of (string list * texpr ann)
  | YieldTo of unit
  | AssignExpr of (texpr ann * texpr ann)
  | Range of (texpr ann * texpr ann)
  | KwStar of texpr ann
  | ChainBinary of (string * texpr ann) list

and tcomprehension =
  { var : texpr ann
  ; gen : texpr ann
  ; cond : texpr ann list
  ; next : tcomprehension ann option
  }

and param =
  { name : string ann
  ; typ : texpr ann option
  ; default : texpr ann option
  }

type tstmt =
  | Pass of unit
  | Break of unit
  | Continue of unit
  | Expr of texpr ann
  | Assign of (texpr ann * texpr ann option * texpr ann option)
  | Del of texpr ann
  | Print of (texpr ann list * bool)
  | Return of texpr ann option
  | Yield of texpr ann option
  | Assert of (texpr ann * texpr ann option)
  | While of (texpr ann * tstmt ann list * tstmt ann list)
  | For of (texpr ann * texpr ann * tstmt ann list * tstmt ann list)
  | If of (texpr ann option * tstmt ann list) list
  | Match of (texpr ann * pattern_t list)
  | Import of import
  | Try of (tstmt ann list * catch ann list * tstmt ann list)
  | Global of string
  | Throw of texpr ann
  | Function of fn_t
  | Class of class_t
  | YieldFrom of texpr ann
  | With of ((texpr ann * string option) list * tstmt ann list)
  | Custom of (texpr ann * tstmt ann list)

and import =
  { imp_from: texpr ann
  ; imp_what: texpr ann option
  ; imp_args: param ann list
  ; imp_ret: texpr ann option
  ; imp_as: string option
  ; imp_dots: int
  }

and catch =
  { exc : texpr ann option
  ; var : string option
  ; stmts : tstmt ann list
  }

and fn_t =
  { fn_name : string
  ; fn_rettyp : texpr ann option
  ; fn_generics : param ann list
  ; fn_args : param ann list
  ; fn_stmts : tstmt ann list
  ; fn_attrs : texpr ann list
  }

and class_t =
  { class_name : string
  ; generics : param ann list
  ; args : param ann list
  ; members : tstmt ann list
  ; attrs : texpr ann list
  }

and pattern_t =
  { pattern: texpr ann
  ; guard: texpr ann option
  ; pat_stmts: tstmt ann list
  }

let flat_pipe x =
  match x with
  | _, [] -> failwith "empty pipeline expression (grammar)"
  | _, [ h ] -> snd h
  | pos, l -> pos, Pipe l

let ppl ?(sep = ", ") ~f l =
  String.concat sep (List.map f l)

let opt_val s def =
  match s with Some s -> s | None -> def

let opt_map f def = function Some s -> f s | None -> def

let filter_opt s =
  List.rev @@ List.fold_left (fun acc i -> match i with Some s -> s::acc | None -> acc) [] s
