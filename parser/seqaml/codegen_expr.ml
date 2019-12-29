(* ****************************************************************************
 * Seqaml.Codegen_expr: Generate Seq code from expression AST
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Err

open Ast
open Ast.Expr
module C = Codegen_ctx

(* ***************************************************************
    Public interface
    *************************************************************** *)

(** [parse ~ctx expr] dispatches an expression AST node [expr] to the proper code generation function. *)
let rec parse ~(ctx : C.t) (ann, node) =
  C.push_ann ~ctx ann;
  let expr =
    match node with
    | Empty p -> parse_none ctx p
    | Bool p -> parse_bool ctx p
    | Int p -> parse_int ctx p
    | Float p -> parse_float ctx p
    | String p -> parse_str ctx p
    | Seq p -> parse_seq ctx p
    | Id p -> parse_id ctx p
    | Tuple p -> parse_tuple ctx p
    | IfExpr p -> parse_if ctx p
    | Unary p -> parse_unary ctx p
    | Binary p -> parse_binary ctx p
    | Pipe p -> parse_pipe ctx p
    | Index p -> parse_index ctx p
    | Call p -> parse_call ctx p
    | Dot p -> parse_dot ctx p
    | Method p -> parse_method ctx p
    | TypeOf p -> parse_typeof ctx p
    | Ptr p -> parse_ptr ctx p
    | Ellipsis p -> Ctypes.null
    | _ -> C.err ~ctx "not yet supported (parse)"
  in
  (* Update C++ bookkeeping members *)
  Llvm.Expr.set_pos expr ann;
  Llvm.Expr.set_trycatch expr ctx.env.trycatch;
  ignore @@ C.pop_ann ~ctx;
  expr

(* ***************************************************************
    Node code generators
    ***************************************************************
    Each AST node is dispatched to the proper codegen function.
    Each codegen function [f] is called as [f context position data]
    where [data] is a node-specific type defined in [Ast_expr]. *)

and parse_none _ _ =
  Llvm.Expr.none ()

and parse_bool _ b =
  Llvm.Expr.bool b

and parse_int ctx (i, _) =
  Llvm.Expr.int (Caml.Int64.of_string i)

and parse_float ctx ?(kind = "") (f, _) =
  Llvm.Expr.float (float_of_string f)

and parse_str _ s = Llvm.Expr.str s

and parse_seq _ s = Llvm.Expr.seq s

and parse_id ?map ctx var =
  let map = Option.value map ~default:ctx.map in
  match Hashtbl.find map var with
  (* Make sure that a variable is either accessible within
    the same base (function) or that it is global variable  *)
  | Some ((C.Type t, _) :: _) -> Llvm.Expr.typ t
  | Some ((C.Var v, vann) :: _) when C.is_accessible ~ctx vann ->
    Llvm.Expr.var v
  | Some ((C.Func (t, _), _) :: _) -> Llvm.Expr.func t
  | _ -> C.err ~ctx "identifier %s not found or realized" var

and parse_tuple ctx args =
  let args = List.map args ~f:(parse ~ctx) in
  Llvm.Expr.tuple args

and parse_if ctx (cond, if_expr, else_expr) =
  let if_expr = parse ~ctx if_expr in
  let else_expr = parse ~ctx else_expr in
  let c_expr = parse ~ctx cond in
  Llvm.Expr.cond c_expr if_expr else_expr

and parse_unary ctx (op, expr) =
  let expr = parse ~ctx expr in
  Llvm.Expr.unary op expr

and parse_binary ctx (lh_expr, bop, rh_expr) =
  let lh_expr = parse ~ctx lh_expr in
  let rh_expr = parse ~ctx rh_expr in
  Llvm.Expr.binary lh_expr bop rh_expr

and parse_pipe ctx exprs =
  let exprs' = List.map exprs ~f:(fun (_, e) -> parse ~ctx e) in
  let ret = Llvm.Expr.pipe exprs' in
  List.iteri exprs ~f:(fun i (pipe, _) -> if pipe = "||>" then Llvm.Expr.set_parallel ret i);
  ret

and parse_index ?(is_type = false) ctx (lh_expr, indices) =
  match Ann.real_t (C.ann ctx).typ, indices with
  | Some (Type t), _ -> Llvm.Expr.typ (C.get_realization ~ctx t)
  | Some (Var t), [idx] -> (* tuple expression *)
    Llvm.Expr.lookup (parse ~ctx lh_expr) (parse ~ctx idx)
  | t, _ -> ierr ~ann:(C.ann ctx) "invalid index expression [%s]" (Ann.t_to_string t)

and parse_call ctx (callee_expr, args) =
  match Ann.real_t (fst callee_expr).typ, args with
  | Some (Type (Class ({ cache = "__array__", _; generics = [_, (_, t)]; _}, _))),
    [ { name = _; value } ] ->
    let arg = parse ~ctx value in
    Llvm.Expr.alloc_array (C.get_realization ~ctx t) arg
  | Some (Type t), _ ->
    let args = List.map args ~f:(fun x -> parse ~ctx x.value) in
    Llvm.Expr.construct (C.get_realization ~ctx t) args
  | Some (Var t), _ ->
    let callee_expr = parse ~ctx callee_expr in
    let args = List.map args ~f:(fun x -> parse ~ctx x.value) in
    Util.A.db "call: lh %s | ark len = %d" (Ann.var_to_string ~useds:true t) (List.length args);
    if List.exists args ~f:(( = ) Ctypes.null)
    then Llvm.Expr.call ~kind:"partial" callee_expr args
    else Llvm.Expr.call ~kind:"call" callee_expr args
  | Some (Import s), _ -> C.err ~ctx "cannot call import %s" s
  | None, _ -> ierr ~ann:(C.ann ctx) "type not determined"

and parse_dot ctx (lh_expr, rhs) =
  (* Imports should be avoided here... put to assign special checks! *)
  match Ann.real_t (C.ann ctx).typ with
  | Some (Type t) ->
    Llvm.Expr.typ (C.get_realization ~ctx t)
  | Some (Var (Func _ as t)) ->
    Llvm.Expr.func (C.get_realization ~ctx t)
  | Some (Var t) ->
    let lh_expr = parse ~ctx lh_expr in
    Llvm.Expr.element lh_expr rhs
  | _ -> ierr ~ann:(C.ann ctx) "bad dot"

and parse_method ctx (lh_expr, rhs) =
  (* Imports should be avoided here... put to assign special checks! *)
  match Ann.real_t (C.ann ctx).typ with
  | Some (Var (Func _ as t)) ->
    let lh_expr = parse ~ctx lh_expr in
    let f = Llvm.Expr.func (C.get_realization ~ctx t) in
    Llvm.Expr.call ~kind:"partial" f [lh_expr]
  | _ -> ierr ~ann:(C.ann ctx) "bad method"

and parse_typeof ctx expr =
  match Ann.real_t (fst expr).typ with
  | Some (Type t) -> C.get_realization ~ctx t
  | _ -> ierr ~ann:(C.ann ctx) "not a type"

and parse_ptr ctx expr =
  match snd expr with
  | Id var ->
    (match Hashtbl.find ctx.map var with
    | Some ((C.Var v, vann) :: _) when C.is_accessible ~ctx vann ->
      Llvm.Expr.ptr v
    | _ -> C.err ~ctx "symbol %s not found" var)
  | _ -> C.err ~ctx "ptr requires an identifier as a parameter"
