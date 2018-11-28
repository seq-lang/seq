(* 786 *)

open Core
open Ctypes
open Foreign

(** Hack to avoid garbage collecting of C strings *)
type cstring = unit ptr
let cstring: cstring typ = ptr void

let strdup = foreign "strdup" 
  (string @-> returning cstring)
let array_of_string_list l = 
  CArray.of_list cstring (List.map ~f:strdup l)
let list_to_carr typ lst = 
  let c_arr = CArray.of_list typ lst in
  let c_len = Unsigned.Size_t.of_int (CArray.length c_arr) in
  CArray.start c_arr, c_len 

(** C translation of [pos_t] type *)
type pos
let pos: pos structure typ = 
  structure "seq_srcinfo"
let pos_file = field pos "file" string
let pos_line = field pos "line" int
let pos_col = field pos "col" int 
let pos_len = field pos "len" int 
;;
seal pos

let get_pos_t_from_srcinfo src : Ast.Pos.t = 
  Ast.Pos.
    { file = getf src pos_file;
      line = getf src pos_line; 
      col = getf src pos_col; 
      len = getf src pos_len }

(** Basic Seq/LLVM types *)

module Types = 
struct
  type typ_t = unit ptr
  type expr_t = unit ptr
  type stmt_t = unit ptr
  type var_t = unit ptr
  type func_t = unit ptr
  type modul_t = unit ptr
  type block_t = unit ptr
  type pattern_t = unit ptr

  let typ : typ_t typ = ptr void
  let expr : expr_t typ = ptr void
  let stmt : stmt_t typ = ptr void
  let var : var_t typ = ptr void
  let func : func_t typ = ptr void
  let modul : modul_t typ = ptr void
  let block : block_t typ = ptr void
  let pattern : pattern_t typ = ptr void
end

module Type = struct
  let t = Types.typ

  let void = foreign "void_type"
    (Ctypes.void @-> returning t)
  
  let bool = foreign "bool_type"
    (Ctypes.void @-> returning t) 
  
  let byte = foreign "byte_type"
    (Ctypes.void @-> returning t)
  
  let int = foreign "int_type"
    (Ctypes.void @-> returning t)
  
  let float = foreign "float_type"
    (Ctypes.void @-> returning t)
  
  let str = foreign "str_type"
    (Ctypes.void @-> returning t)  
  
  let seq = foreign "str_seq_type"
    (Ctypes.void @-> returning t)

  let record names types = 
    let fn = foreign "record_type" 
      (ptr cstring @-> ptr t @-> size_t @-> returning t)
    in
    assert ((List.length names) = (List.length types));
    let narr = array_of_string_list names in 
    let tarr, tlen = list_to_carr t types in
    fn (CArray.start narr) tarr tlen

  let func typ args = 
    let fn = foreign "func_type" 
      (t @-> ptr t @-> size_t @-> returning t)
    in 
    let arr, len = list_to_carr t args in
    fn typ arr len
  
  let param ~name typ = 
    let fn = foreign (name ^ "_type")
      (t @-> returning t)
    in 
    fn typ

  let cls name = foreign "ref_type"
    (cstring @-> returning t) 
    (strdup name)

  let set_cls_args typ names types = 
    let rt = record names types in
    foreign "set_ref_record"
      (t @-> t @-> returning Ctypes.void) 
      typ rt

  let add_cls_method typ name func = foreign "add_ref_method"
    (t @-> cstring @-> Types.func @-> returning Ctypes.void)
    typ (strdup name) func

  let set_cls_done = foreign "set_ref_done" 
    (t @-> returning Ctypes.void)

  let get_name = foreign "get_type_name" 
    (t @-> returning string)

  let expr_type = foreign "get_type" 
    (Types.expr @-> returning t)

  let is_equal = foreign "type_eq" 
    (t @-> t @-> returning Ctypes.bool)
end 

module Expr = struct
  let t = Types.expr

  let none = foreign "none_expr" 
    (void @-> returning t)

  let bool = foreign "bool_expr" 
    (bool @-> returning t)

  let int = foreign "int_expr" 
    (int @-> returning t)

  let float = foreign "float_expr" 
    (double @-> returning t)

  let str s = foreign "str_expr" 
    (cstring @-> returning t) 
    (strdup s)

  let seq s = foreign "str_seq_expr" 
    (cstring @-> returning t)
    (strdup s)

  let var = foreign "var_expr"
    (Types.var @-> returning t)

  let typ = foreign "type_expr" 
    (Types.typ @-> returning t)

  let func = foreign "func_expr"  
    (Types.func @-> returning t)

  let tuple args = 
    let fn = foreign "record_expr" 
      (ptr t @-> size_t @-> returning t)
    in
    let arr, len = list_to_carr t args in
    fn arr len

  let list ~kind typ args = 
    let fn = foreign (kind ^ "_expr")
      (Types.typ @-> ptr t @-> size_t @-> returning t)
    in
    let arr, len = list_to_carr t args in
    fn typ arr len

  let list_comprehension ~kind typ expr = 
    let fn = foreign (kind ^ "_comp_expr")
      (Types.typ @-> t @-> returning t)
    in
    fn typ expr

  let dict_comprehension typ expr1 expr2 = 
    let fn = foreign "dict_comp_expr"
      (Types.typ @-> t @-> t @-> returning t)
    in
    fn typ expr1 expr2

  let cond = foreign "cond_expr" 
    (t @-> t @-> t @-> returning t)

  let unary op exp = foreign "uop_expr" 
    (cstring @-> t @-> returning t)
    (strdup op) exp

  let binary lh bop rh = 
    let fn = match bop with
      | "is" | "is not" | "in" | "in not" ->
        let prefix = 
          String.tr bop ~target:' ' ~replacement:'_' 
        in
        foreign (prefix ^ "_expr")
          (t @-> t @-> returning t) 
      | bop ->
        foreign ("bop_expr")
          (cstring @-> t @-> t @-> returning t) 
          (strdup bop)
    in
    fn lh rh

  let pipe exprs = 
    let fn = foreign "pipe_expr" 
      (ptr t @-> size_t @-> returning t)
    in 
    let arr, len = list_to_carr t exprs in
    fn arr len

  let slice = foreign "array_slice_expr"
    (t @-> t @-> t @-> returning t)

  let lookup = foreign "array_lookup_expr" 
    (t @-> t @-> returning t)

  let construct typ args = 
    let fn = foreign "construct_expr"  
      (Types.typ @-> ptr t @-> size_t @-> returning t)
    in
    let arr, len = list_to_carr t args in
    fn typ arr len

  let call ?(kind="call") expr args = 
    let fn = foreign (kind ^ "_expr")
      (t @-> ptr t @-> size_t @-> returning t)
    in
    let arr, len = list_to_carr t args in
    fn expr arr len

  let element elem what = foreign "get_elem_expr" 
    (t @-> cstring @-> returning t)
    elem (strdup what)

  let static typ what = foreign "static_expr"
    (Types.typ @-> cstring @-> returning t)
    typ (strdup what)

  let get_pos expr : Ast.Pos.t =
    let sp = foreign "get_pos" 
      (t @-> returning pos)
      expr
    in
    get_pos_t_from_srcinfo sp

  let set_pos expr (pos: Ast.Pos.t) = foreign "set_pos" 
    Ctypes.(t @-> cstring @-> int @-> int @-> int @-> returning void)
    expr (strdup pos.file) pos.line pos.col pos.len

  let set_comprehension_body ~kind expr body = 
    foreign (sprintf "set_%s_comp_body" kind)
      (t @-> Types.stmt @-> returning Ctypes.void)
      expr body

  let get_name = foreign "get_expr_name" 
    (t @-> returning string)

  let is_type expr = 
    match get_name expr with 
      | "type" -> true 
      | _ -> false
end

module Stmt = struct
  let t = Types.stmt
  
  module Pattern = struct
    let t = Types.pattern

    let star = foreign "star_pattern" 
      (void @-> returning t)

    let int = foreign "int_pattern" 
      (int @-> returning t)

    let bool = foreign "bool_pattern" 
      (bool @-> returning t)

    let str s = foreign "str_pattern" 
      (cstring @-> returning t)
      (strdup s)

    let seq s = foreign "seq_pattern" 
      (cstring @-> returning t)
      (strdup s)

    let record pats = 
      let fn = foreign "record_pattern" 
        (ptr t @-> size_t @-> returning t)
      in 
      let arr, len = list_to_carr t pats in
      fn arr len
    
    let range = foreign "range_pattern" 
      (Ctypes.int @-> Ctypes.int @-> returning t)
    
    let array pats = 
      let fn = foreign "array_pattern" 
        (ptr t @-> size_t @-> returning t)
      in 
      let arr, len = list_to_carr t pats in
      fn arr len
    
    let orp pats = 
      let fn = foreign "or_pattern" 
        (ptr t @-> size_t @-> returning t)
      in 
      let arr, len = list_to_carr t pats in
      fn arr len
    
    let wildcard = foreign "wildcard_pattern" 
      (void @-> returning t)
    
    let guarded = foreign "guarded_pattern" 
      (t @-> Types.expr @-> returning t)
    
    let bound = foreign "bound_pattern" 
      (t @-> returning t)
  end

  module Block = struct
    let t = Types.block

    let while_loop = foreign "get_while_block"  
      (Types.stmt @-> returning t)
    
    let loop = foreign "get_for_block"  
      (Types.stmt @-> returning t)

    let elseb = foreign "get_else_block"   
      (Types.stmt @-> returning t)

    let elseif = foreign "get_elif_block"   
      (Types.stmt @-> Types.expr @-> returning t)

    let case = foreign "add_match_case" 
      (Types.stmt @-> Types.pattern @-> returning t)

    let func = foreign "get_func_block" 
      (Types.func @-> returning t)

    let add_stmt block stmt = foreign "add_stmt"
      (Types.stmt @-> t @-> returning void) 
      stmt block
  end

  let pass = foreign "pass_stmt" 
    (void @-> returning t) 

  let break = foreign "break_stmt" 
    (void @-> returning t)

  let continue = foreign "continue_stmt" 
    (void @-> returning t)

  let expr = foreign "expr_stmt" 
    (Types.expr @-> returning t)

  let var = foreign "var_stmt" 
    (Types.var @-> returning t)

  let assign = foreign "assign_stmt" 
    (Types.var @-> Types.expr @-> returning t)

  let assign_member cls memb what = foreign "assign_member_stmt" 
    (Types.expr @-> cstring @-> Types.expr @-> returning t)
    cls (strdup memb) what

  let assign_index = foreign "assign_index_stmt" 
    (Types.expr @-> Types.expr @-> Types.expr @-> returning t)

  let del_index = foreign "del_index_stmt"
    (Types.expr @-> Types.expr @-> returning t)

  let print = foreign "print_stmt" 
    (Types.expr @-> returning t)

  let return = foreign "return_stmt" 
    (Types.expr @-> returning t)

  let yield = foreign "yield_stmt" 
    (Types.expr @-> returning t)

  let assrt = foreign "assert_stmt"
    (Types.expr @-> returning t)

  let while_loop = foreign "while_stmt" 
    (Types.expr @-> returning t)

  let loop = foreign "for_stmt" 
    (Types.expr @-> returning t)

  let cond = foreign "if_stmt" 
    (void @-> returning t)

  let matchs = foreign "match_stmt" 
    (Types.expr @-> returning t)

  let func = foreign "func_stmt"
    (Types.func @-> returning t)

  let set_base = foreign "set_base" 
    (t @-> Types.func @-> returning void)

  let set_pos stmt (pos: Ast.Pos.t) = foreign "set_pos" 
    Ctypes.(t @-> cstring @-> int @-> int @-> int @-> returning void)
    stmt (strdup pos.file) pos.line pos.col pos.len
end

module Var = struct
  let t = Types.var

  let var_of_stmt = foreign "var_stmt_var"
    (Types.stmt @-> returning t)

  let loop = foreign "get_for_var" 
    (Types.stmt @-> returning t)

  let bound_pattern = foreign "get_bound_pattern_var" 
    (Types.pattern @-> returning t)
end

module Func = struct
  let t = Types.func

  let func name = foreign "func" 
    (cstring @-> returning t) 
    (strdup name)

  let set_args fn names types = 
    assert ((List.length names) = (List.length types));
    let names = array_of_string_list names in
    let arr, len = list_to_carr Types.typ types in
    foreign "set_func_params"
      (t @-> ptr cstring @-> ptr Types.typ @-> size_t @-> returning void)
      fn
      (CArray.start names)
      arr len

  let get_arg fn name = 
    foreign "get_func_arg" 
      (t @-> cstring @-> returning Types.var)
      fn 
      (strdup name)

  let set_return = foreign "set_func_return" 
    (t @-> Types.expr @-> returning void)

  let set_yield = foreign "set_func_yield" 
    (t @-> Types.expr @-> returning void)

  let set_type = foreign "set_func_out" 
    (t @-> Types.typ @-> returning void)

  let set_extern = foreign "set_func_extern" 
    (t @-> returning void)
end

module Generics = struct
  module Func = struct
    let set_number = foreign "set_func_generics" 
      (Types.func @-> int @-> returning void)

    let get = foreign "get_func_generic" 
      (Types.func @-> int @-> returning Types.typ)
    
    let set_name f idx n = foreign "set_func_generic_name" 
      (Types.func @-> int @-> cstring @-> returning void)
      f idx (strdup n)
    
    let realize fn_expr typs =
      let arr, len = list_to_carr Types.typ typs in
      foreign "realize_func"
        (Types.expr @-> ptr Types.typ @-> size_t @-> returning Types.func)
        fn_expr arr len
  end

  module Type = struct
    let set_number = foreign "set_ref_generics" 
      (Types.typ @-> int @-> returning void)

    let get = foreign "get_ref_generic" 
      (Types.typ @-> int @-> returning Types.typ)
    
    let set_name f idx n = foreign "set_ref_generic_name" 
      (Types.typ @-> int @-> cstring @-> returning void)
      f idx (strdup n)

    let realize typ typs =
      let arr, len = list_to_carr Types.typ typs in
      foreign "realize_type"
        (Types.typ @-> ptr Types.typ @-> size_t @-> returning Types.typ)
        typ arr len
  end
end

module Module = struct
  let t = Types.modul

  let init = foreign "init_module"
    (void @-> returning t)

  let block = foreign "get_module_block" 
    (t @-> returning Stmt.Block.t)

  let get_args = foreign "get_module_arg"
    (t @-> returning Types.var)

  let exec mdl args debug = 
    let fn = foreign "exec_module" 
      (t @-> ptr cstring @-> size_t 
         @-> bool @-> ptr (ptr char) 
         @-> ptr (ptr pos) 
         @-> returning bool)
    in
    let aarr = array_of_string_list args in
    let alen = Unsigned.Size_t.of_int (CArray.length aarr) in
    let err_addr = Ctypes.allocate (ptr char) (from_voidp char null) in
    let src_addr = Ctypes.allocate (ptr pos) (from_voidp pos null) in
    let ret = fn mdl (CArray.start aarr) alen debug err_addr src_addr in

    if not ret then
      let msg = coerce (ptr char) string (!@ err_addr) in
      let pos = get_pos_t_from_srcinfo (!@ !@ src_addr) in
      raise (Err.SeqCError (msg, pos))
end




