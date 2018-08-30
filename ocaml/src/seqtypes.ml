(* 786 *)

open Ctypes 
open Foreign

(* Seq types *)
type seq_type = unit ptr 
let seq_type: seq_type typ = ptr void

type seq_expr = unit ptr 
let seq_expr: seq_expr typ = ptr void

type seq_stmt = unit ptr 
let seq_stmt: seq_stmt typ = ptr void

type seq_var = unit ptr 
let seq_var: seq_var typ = ptr void

type seq_func = unit ptr 
let seq_func: seq_func typ = ptr void

type seq_block = unit ptr 
let seq_block: seq_block typ = ptr void

type seq_module = unit ptr 
let seq_module: seq_module typ = ptr void

type seq_pattern = unit ptr 
let seq_pattern: seq_pattern typ = ptr void

(* Types *)

let void_type    = foreign "void_type"    (void @-> returning seq_type)
let bool_type    = foreign "bool_type"    (void @-> returning seq_type)
let int_type     = foreign "int_type"     (void @-> returning seq_type)
let float_type   = foreign "float_type"   (void @-> returning seq_type)
let str_type     = foreign "str_type"     (void @-> returning seq_type)
let generic_type = foreign "generic_type" (void @-> returning seq_type)
let array_type   = foreign "array_type"   (seq_type @-> returning seq_type)
let ref_type     = foreign "ref_type"     (string @-> seq_type @-> returning seq_type)

let record_type' = foreign "record_type" 
  (ptr string @-> ptr seq_type @-> size_t @-> returning seq_type)
let record_type names types = 
  if List.length names != List.length types then 
    Failure ("set_func_params len(names) != len(types)") |> raise;
	let n_arr = CArray.of_list string names in
	let a_arr = CArray.of_list seq_type types in
  let a_len = Unsigned.Size_t.of_int (CArray.length a_arr) in
	record_type' (CArray.start n_arr) (CArray.start a_arr) a_len

let add_ref_method = foreign "add_ref_method"
  (seq_type @-> string @-> seq_func @-> returning void)

let set_ref_done = foreign "set_ref_done" (seq_type @-> returning void)

(* Expressions *)

let bool_expr  = foreign "bool_expr"  (bool @-> returning seq_expr)
let int_expr   = foreign "int_expr"   (int @-> returning seq_expr)
let float_expr = foreign "float_expr" (float @-> returning seq_expr)
let str_expr   = foreign "str_expr"   (string @-> returning seq_expr)
let func_expr  = foreign "func_expr"  (seq_func @-> returning seq_expr)
let var_expr   = foreign "var_expr"   (seq_var @-> returning seq_expr)

let cond_expr = foreign "cond_expr" 
	(seq_expr @-> seq_expr @-> seq_expr @-> returning seq_expr)
let uop_expr = foreign "uop_expr" 
	(string @-> seq_expr @-> returning seq_expr)
let bop_expr = foreign "bop_expr" 
	(string @-> seq_expr @-> seq_expr @-> returning seq_expr)

let call_expr' = foreign "call_expr" 
	(seq_expr @-> ptr seq_expr @-> size_t @-> returning seq_expr)
let call_expr expr lst = 
	let c_arr = CArray.of_list seq_expr lst in
	let c_len = Unsigned.Size_t.of_int (CArray.length c_arr) in
	call_expr' expr (CArray.start c_arr) c_len

let get_elem_expr = foreign "get_elem_expr" 
	(seq_expr @-> string @-> returning seq_expr)
let array_expr = foreign "array_expr"
  (seq_type @-> seq_expr @-> returning seq_expr)
let array_lookup_expr = foreign "array_lookup_expr" 
	(seq_expr @-> seq_expr @-> returning seq_expr)
let array_slice_expr = foreign "array_slice_expr" 
	(seq_expr @-> seq_expr @-> seq_expr @-> returning seq_expr)

let construct_expr' = foreign "construct_expr"
  (seq_type @-> ptr seq_expr @-> size_t @-> returning seq_expr)
let construct_expr typ lst =
  let c_arr = CArray.of_list seq_expr lst in
	let c_len = Unsigned.Size_t.of_int (CArray.length c_arr) in
	construct_expr' typ (CArray.start c_arr) c_len

let record_expr' = foreign "record_expr" 
	(ptr seq_expr @-> size_t @-> returning seq_expr)
let record_expr lst = 
	let c_arr = CArray.of_list seq_expr lst in
	let c_len = Unsigned.Size_t.of_int (CArray.length c_arr) in
	record_expr' (CArray.start c_arr) c_len

let static_expr = foreign "static_expr"   
  (seq_type @-> string @-> returning seq_expr)
let method_expr = foreign "method_expr"   
  (seq_expr @-> seq_func @-> returning seq_expr)

(* Statements *)

let pass_stmt = foreign "pass_stmt" 
	(void @-> returning seq_stmt)
let break_stmt = foreign "break_stmt" 
	(void @-> returning seq_stmt)
let continue_stmt = foreign "continue_stmt" 
	(void @-> returning seq_stmt)
let expr_stmt = foreign "expr_stmt" 
	(seq_expr @-> returning seq_stmt)
let var_stmt = foreign "var_stmt" 
	(seq_var @-> returning seq_stmt)
let var_stmt_var = foreign "var_stmt_var"
  (seq_stmt @-> returning seq_var)
let assign_stmt = foreign "assign_stmt" 
	(seq_var @-> seq_expr @-> returning seq_stmt)
let assign_member_stmt = foreign "assign_member_stmt" 
	(seq_expr @-> string @-> seq_expr @-> returning seq_stmt)
let assign_index_stmt = foreign "assign_index_stmt" 
	(seq_expr @-> seq_expr @-> seq_expr @-> returning seq_stmt)
let print_stmt = foreign "print_stmt" 
	(seq_expr @-> returning seq_stmt)
let if_stmt = foreign "if_stmt" 
	(void @-> returning seq_stmt)
let while_stmt = foreign "while_stmt" 
	(seq_expr @-> returning seq_stmt)
let for_stmt = foreign "for_stmt" 
	(seq_expr @-> returning seq_stmt)
let return_stmt = foreign "return_stmt" 
	(seq_expr @-> returning seq_stmt)
let yield_stmt = foreign "yield_stmt" 
	(seq_expr @-> returning seq_stmt)
let func_stmt = foreign "func_stmt" 
  (seq_func @-> returning seq_stmt)

let match_stmt = foreign "match_stmt" 
  (seq_expr @-> returning seq_stmt)
let add_match_case = foreign "add_match_case" 
  (seq_stmt @-> seq_pattern @-> returning seq_block)

let bound_pattern    = foreign "bound_pattern" (seq_pattern @-> returning seq_pattern)
let wildcard_pattern = foreign "wildcard_pattern" (void @-> returning seq_pattern)
let int_pattern      = foreign "int_pattern" (int @-> returning seq_pattern)
let str_pattern      = foreign "str_pattern" (string @-> returning seq_pattern)
let bool_pattern     = foreign "bool_pattern" (bool @-> returning seq_pattern)

let get_bound_pattern_var = foreign "get_bound_pattern_var" 
  (seq_pattern @-> returning seq_var)

(* Functions *)

let func = foreign "func" (string @-> returning seq_func)
let get_func_block = foreign "get_func_block" (seq_func @-> returning seq_block)
let get_func_arg = foreign "get_func_arg" (seq_func @-> string @-> returning seq_var)
let set_func_return = foreign "set_func_return" (seq_func @-> seq_stmt @-> returning void)
let set_func_yield = foreign "set_func_yield" (seq_func @-> seq_stmt @-> returning void)
let set_func_params' = foreign "set_func_params"
  (seq_func @-> ptr string @-> ptr seq_type @-> size_t @-> returning void)
let set_func_params fn names types = 
  if List.length names != List.length types then 
    Failure ("set_func_params len(names) != len(types)") |> raise;
	let n_arr = CArray.of_list string names in
	let a_arr = CArray.of_list seq_type types in
  let a_len = Unsigned.Size_t.of_int (CArray.length a_arr) in
	set_func_params' fn (CArray.start n_arr) (CArray.start a_arr) a_len

let set_func_generics = foreign "set_func_generics" (seq_func @-> int @-> returning void)
let get_func_generic = foreign "get_func_generic" (seq_func @-> int @-> returning seq_type)
let set_func_generic_name = foreign "set_func_generic_name" (seq_func @-> int @-> string @-> returning void)

(* Utils *)

let get_module_block = foreign "get_module_block" (seq_module @-> returning seq_block)
let get_while_block  = foreign "get_while_block"  (seq_stmt @-> returning seq_block)
let get_for_block    = foreign "get_for_block"    (seq_stmt @-> returning seq_block)
let get_else_block   = foreign "get_else_block"   (seq_stmt @-> returning seq_block)
let get_elif_block   = foreign "get_elif_block"   (seq_stmt @-> seq_expr @-> returning seq_block)

let get_for_var = foreign "get_for_var" 
  (seq_stmt @-> returning seq_var)

let set_base = foreign "set_base"
  (seq_stmt @-> seq_func @-> returning void)

let add_stmt = foreign "add_stmt"
  (seq_stmt @-> seq_block @-> returning void)

let init_module = foreign "init_module"
  (void @-> returning seq_module)

let exec_module = foreign "exec_module" 
  (seq_module @-> bool @-> returning string)



