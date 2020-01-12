(* *****************************************************************************
 * Seqaml.Ast_pos: AST annotations
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

(** File position annotation for an AST node. *)
(* type tpos =
  { file : string
  ; line : int
  ; col : int
  ; len : int
  } *)

type tpos = (Lexing.position * Lexing.position)

(** Annotated type specification. *)
type 'a ann = tpos * 'a

exception SyntaxError of string * Lexing.position
exception GrammarError of string * Lexing.position

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
  | Unpack of string
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
  | Yield of unit

and tcomprehension =
  { var : string list
  ; gen : texpr ann
  ; cond : texpr ann option
  ; next : tcomprehension ann option
  }

type tstmt =
  | Pass of unit
  | Break of unit
  | Continue of unit
  | Expr of texpr ann
  | Assign of (texpr ann * texpr ann * int * texpr ann option)
  | Del of texpr ann
  | Print of (texpr ann list * string)
  | Return of texpr ann option
  | Yield of texpr ann option
  | Assert of texpr ann
  | TypeAlias of (string * texpr ann)
  | While of (texpr ann * tstmt ann list)
  | For of (string list * texpr ann * tstmt ann list)
  | If of (texpr ann option * tstmt ann list) list
  | Match of (texpr ann * (pattern ann * tstmt ann list) list)
  | Extend of (texpr ann * tstmt ann list)
  | Import of ((string * string option) * (string * string option) list)
  | ImportExtern of eimport
  | Try of (tstmt ann list * catch ann list * tstmt ann list)
  | Global of string
  | Throw of texpr ann
  | Prefetch of texpr ann list
  | Special of (string * tstmt ann list * string list)
  | Function of fn_t
  | Class of class_t
  | Type of class_t
  | Declare of param ann

and eimport =
  { lang: string
  ; e_from: texpr ann option
  ; e_name: string
  ; e_typ: texpr ann
  ; e_args: param ann list
  ; e_as: string option
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
  ; fn_rettyp: texpr ann option
  ; fn_generics : string list
  ; fn_args : param ann list
  ; fn_stmts : tstmt ann list
  ; fn_attrs : string ann list
  }

and class_t =
  { class_name : string
  ; generics : string list
  ; args : param ann list
  ; members : tstmt ann list
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

let assign_cnt = ref 0

let new_assign () =
  incr assign_cnt;
  Printf.sprintf "$A%d" @@ pred !assign_cnt
let flat_pipe x =
  match x with
  | _, []  -> failwith "empty pipeline expression (grammar)"
  | _, [h] -> snd h
  | pos, l -> pos, Pipe l

(* Converts list of conditionals into the AND AST node
   (used for chained conditionals such as
   0 < x < y < 10 that becomes (0 < x) AND (x < y) AND (y < 10)) *)
type cond_t =
  | Cond of texpr
  | CondBinary of (texpr ann * string * cond_t ann)

let rec flat_cond x =
  let expr = match snd x with
    | CondBinary (lhs, op, (_, CondBinary (next_lhs, _, _) as rhs)) ->
      Binary ((fst lhs, Binary (lhs, op, next_lhs)), "&&", flat_cond rhs)
    | CondBinary (lhs, op, (pos, Cond (rhs))) -> Binary (lhs, op, (pos, rhs))
    | Cond n -> n
  in
  fst x, expr

let rec flatten_dot ~sep = function
  | _, Id s -> s
  | _, Dot (d, s) -> Printf.sprintf "%s%s%s" (flatten_dot ~sep d) sep s
  | _ -> failwith "invalid import construct (grammar)"

let rec parse_assign pos lhs rhs shadow =
  (* wrap RHS in tuple for consistency (e.g. x, y -> (x, y)) *)
  let init_exprs, rhs =
    if List.length rhs > 1 then (
      let var = pos, Id (new_assign ()) in
      [pos, Assign (var, (pos, Tuple rhs), 1, None)], var
    ) else if List.length lhs > 1 then (
      let var = pos, Id (new_assign ()) in
      [pos, Assign (var, List.hd rhs, 1, None)], var
    ) else ([], List.hd rhs)
  in
  (* wrap LHS in tuple as well (e.g. x, y -> (x, y)) *)
  let lhs = match lhs with [_, (Tuple l | List l)] -> l | l -> l in
  let exprs = match lhs with
    | [lhs] -> [pos, Assign (lhs, rhs, shadow, None)]
    | lhs ->
      let len = List.length lhs in
      let unpack_i = ref (-1) in
      List.concat (List.mapi
        (fun i expr ->
          match snd expr with
          | Unpack var when !unpack_i = -1 ->
            unpack_i := i;
            let start = Some (pos, Int (string_of_int i, "")) in
            let eend =
              if i = len - 1 then None
              else Some (pos, Int (string_of_int @@ i +  1 - len, ""))
            in
            let slice = pos, Slice (start, eend, None) in
            let rhs = pos, Index (rhs, slice) in
            [pos, Assign ((pos, Id var), rhs, shadow, None)]
          | Unpack _ when !unpack_i > -1 ->
            raise (GrammarError ("cannot have two tuple unpackings on LHS", fst pos))
          | _ ->
            (* left of unpack: a, b, *c = x <=> a = x[0]; b = x[1] *)
            (* right of unpack: *c, b, a = x <=> a = x[-1]; b = x[-2] *)
            let i = if !unpack_i = -1 then i else i - len in
            parse_assign pos [expr] [pos, Index (rhs, (pos, Int (string_of_int i, "")))] shadow)
        lhs) (* TODO: add assertes *)
  in
  init_exprs @ exprs