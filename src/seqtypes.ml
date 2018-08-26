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

let getelem_expr = foreign "getelem_expr" 
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
  (seq_var @-> string @-> returning seq_expr)
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

(* Functions *)

let func_stmt = foreign "func_stmt" (string @-> seq_type @-> returning seq_func)
let get_func_block = foreign "get_func_block" (seq_func @-> returning seq_block)
let get_func_arg = foreign "get_func_arg" (seq_func @-> string @-> returning seq_var)
let set_func_gen = foreign "set_func_gen" (seq_func @-> returning void)
let set_func_params' = foreign "set_func_params"
  (seq_func @-> ptr string @-> ptr seq_type @-> size_t @-> returning void)
let set_func_params fn names types = 
  if List.length names != List.length types then 
    Failure ("set_func_params len(names) != len(types)") |> raise;
	let n_arr = CArray.of_list string names in
	let a_arr = CArray.of_list seq_type types in
  let a_len = Unsigned.Size_t.of_int (CArray.length a_arr) in
	set_func_params' fn (CArray.start n_arr) (CArray.start a_arr) a_len

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
  (seq_module @-> bool @-> returning void)



