(* 786 *)

open Ctypes 
open Foreign

exception SeqCError of string * Ast.pos_t

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


type seq_srcinfo
let seq_srcinfo: seq_srcinfo structure typ = structure "seq_srcinfo"

let srcinfo_file = field seq_srcinfo "file" string
let srcinfo_line = field seq_srcinfo "line" int
let srcinfo_col = field seq_srcinfo "col" int ;;

seal seq_srcinfo

(* Types *)

let void_type    = foreign "void_type"    (void @-> returning seq_type)
let bool_type    = foreign "bool_type"    (void @-> returning seq_type)
let int_type     = foreign "int_type"     (void @-> returning seq_type)
let float_type   = foreign "float_type"   (void @-> returning seq_type)
let byte_type    = foreign "byte_type"    (void @-> returning seq_type)
let str_type     = foreign "str_type"     (void @-> returning seq_type)
let str_seq_type = foreign "str_seq_type" (void @-> returning seq_type)
let generic_type = foreign "generic_type" (void @-> returning seq_type)
let array_type   = foreign "array_type"   (seq_type @-> returning seq_type)
let ptr_type     = foreign "ptr_type"     (seq_type @-> returning seq_type)
let ref_type     = foreign "ref_type"     (string @-> returning seq_type)
let file_type    = foreign "source_type"  (void @-> returning seq_type)

let func_type' = foreign "func_type" 
  (seq_type @-> ptr seq_type @-> size_t @-> returning seq_type)
let func_type ret types = 
	let a_arr = CArray.of_list seq_type types in
  let a_len = Unsigned.Size_t.of_int (CArray.length a_arr) in
	func_type' ret (CArray.start a_arr) a_len

let gen_type = foreign "gen_type" 
  (seq_type @-> returning seq_type)

let record_type' = foreign "record_type" 
  (ptr string @-> ptr seq_type @-> size_t @-> returning seq_type)
let record_type names types = 
  if List.length names != List.length types then 
    Failure ("set_func_params len(names) != len(types)") |> raise;
	let n_arr = CArray.of_list string names in
	let a_arr = CArray.of_list seq_type types in
  let a_len = Unsigned.Size_t.of_int (CArray.length a_arr) in
	record_type' (CArray.start n_arr) (CArray.start a_arr) a_len

let set_ref_record = foreign "set_ref_record"
  (seq_type @-> seq_type @-> returning void)

let add_ref_method = foreign "add_ref_method"
  (seq_type @-> string @-> seq_func @-> returning void)

let set_ref_done = foreign "set_ref_done" (seq_type @-> returning void)

(* Expressions *)

let bool_expr  = foreign "bool_expr"  (bool @-> returning seq_expr)
let int_expr   = foreign "int_expr"   (int @-> returning seq_expr)
let float_expr = foreign "float_expr" (double @-> returning seq_expr)
let str_expr   = foreign "str_expr"   (string @-> returning seq_expr)
let str_seq_expr  = foreign "str_seq_expr" (string @-> returning seq_expr)
let func_expr  = foreign "func_expr"  (seq_func @-> returning seq_expr)
let var_expr   = foreign "var_expr"   (seq_var @-> returning seq_expr)

let type_expr   = foreign "type_expr" (seq_type @-> returning seq_expr)


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

let list_expr' = foreign "list_expr" 
  (seq_type @-> ptr seq_expr @-> size_t @-> returning seq_expr)
let list_expr typ lst = 
  let c_arr = CArray.of_list seq_expr lst in
  let c_len = Unsigned.Size_t.of_int (CArray.length c_arr) in
  list_expr' typ (CArray.start c_arr) c_len 

let partial_expr' = foreign "partial_expr" 
	(seq_expr @-> ptr seq_expr @-> size_t @-> returning seq_expr)
let partial_expr expr lst = 
	let c_arr = CArray.of_list seq_expr lst in
	let c_len = Unsigned.Size_t.of_int (CArray.length c_arr) in
	partial_expr' expr (CArray.start c_arr) c_len 

let pipe_expr' = foreign "pipe_expr" 
	(ptr seq_expr @-> size_t @-> returning seq_expr)
let pipe_expr lst = 
	let c_arr = CArray.of_list seq_expr lst in
	let c_len = Unsigned.Size_t.of_int (CArray.length c_arr) in
	pipe_expr' (CArray.start c_arr) c_len 


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
let str_seq_pattern  = foreign "seq_pattern" (string @-> returning seq_pattern)
let bool_pattern     = foreign "bool_pattern" (bool @-> returning seq_pattern)
let range_pattern    = foreign "range_pattern" (int @-> int @-> returning seq_pattern)
let guarded_pattern  = foreign "guarded_pattern" (seq_pattern @-> seq_expr @-> returning seq_pattern)
let star_pattern     = foreign "star_pattern" (void @-> returning seq_pattern)


let get_bound_pattern_var = foreign "get_bound_pattern_var" 
  (seq_pattern @-> returning seq_var)

let list_pattern_helper lst fn =
  let c_arr = CArray.of_list seq_pattern lst in
  let c_len = Unsigned.Size_t.of_int (CArray.length c_arr) in
  fn (CArray.start c_arr) c_len

let array_pattern lst = 
  list_pattern_helper lst @@ foreign "array_pattern" (ptr seq_pattern @-> size_t @-> returning seq_pattern) 

let record_pattern lst = 
  list_pattern_helper lst @@ foreign "record_pattern" (ptr seq_pattern @-> size_t @-> returning seq_pattern) 

let or_pattern lst = 
  list_pattern_helper lst @@ foreign "or_pattern" (ptr seq_pattern @-> size_t @-> returning seq_pattern) 

(* Functions *)

let func = foreign "func" (string @-> returning seq_func)
let get_func_block = foreign "get_func_block" (seq_func @-> returning seq_block)
let get_func_arg = foreign "get_func_arg" (seq_func @-> string @-> returning seq_var)
let set_func_return = foreign "set_func_return" (seq_func @-> seq_stmt @-> returning void)
let set_func_extern = foreign "set_func_extern" (seq_func @-> returning void)
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


(* Generics --- needs better typing ... *)

let set_func_out = foreign "set_func_out" (seq_func @-> seq_type @-> returning void)

let set_func_generics = foreign "set_func_generics" (seq_func @-> int @-> returning void)
let get_func_generic = foreign "get_func_generic" (seq_func @-> int @-> returning seq_type)
let set_func_generic_name = foreign "set_func_generic_name" (seq_func @-> int @-> string @-> returning void)

let set_ref_generics = foreign "set_ref_generics" (seq_type @-> int @-> returning void)
let get_ref_generic = foreign "get_ref_generic" (seq_type @-> int @-> returning seq_type)
let set_ref_generic_name = foreign "set_ref_generic_name" (seq_type @-> int @-> string @-> returning void)

let realize_type' = foreign "realize_type"
  (seq_type @-> ptr seq_type @-> size_t @-> returning seq_type)
let realize_type typ typs =
  let c_arr = CArray.of_list seq_expr typs in
	let c_len = Unsigned.Size_t.of_int (CArray.length c_arr) in
	realize_type' typ (CArray.start c_arr) c_len

let realize_func' = foreign "realize_func"
  (seq_expr @-> ptr seq_type @-> size_t @-> returning seq_func)
let realize_func fn_expr typs =
  let c_arr = CArray.of_list seq_expr typs in
	let c_len = Unsigned.Size_t.of_int (CArray.length c_arr) in
  realize_func' fn_expr (CArray.start c_arr) c_len


(* Utils *)

let get_module_block = foreign "get_module_block" (seq_module @-> returning seq_block)
let get_while_block  = foreign "get_while_block"  (seq_stmt @-> returning seq_block)
let get_for_block    = foreign "get_for_block"    (seq_stmt @-> returning seq_block)
let get_else_block   = foreign "get_else_block"   (seq_stmt @-> returning seq_block)
let get_elif_block   = foreign "get_elif_block"   (seq_stmt @-> seq_expr @-> returning seq_block)

let get_func = foreign "get_func" (seq_expr @-> returning seq_func)
let get_var_type = foreign "get_var_type" (seq_var @-> returning seq_type)

let get_expr_name = foreign "get_expr_name" (seq_expr @-> returning string)

let get_type' = foreign "get_type" (seq_expr @-> returning seq_type)
let get_type typ_expr =
  match get_expr_name typ_expr with
  | "type" -> Some (get_type' typ_expr)
  | _ -> None

let get_pos_t_from_srcinfo src: Ast.pos_t = 
  let file = getf src srcinfo_file in
  let line = getf src srcinfo_line in
  let col = getf src srcinfo_col in
  {pos_fname = file; pos_lnum = line; pos_cnum = col; pos_bol = 0}

let get_pos' = foreign "get_pos" (seq_expr @-> returning seq_srcinfo)
let get_pos expr : Ast.pos_t =
  let sp = get_pos' expr in
  get_pos_t_from_srcinfo sp
  
let set_pos' = foreign "set_pos" (seq_expr @-> string @-> int @-> int @-> returning void)
let set_pos expr (pos: Ast.pos_t) = 
  set_pos' expr pos.pos_fname pos.pos_lnum (pos.pos_cnum - pos.pos_bol)

let types_eq = foreign "type_eq" (seq_type @-> seq_type @-> returning bool)

let get_for_var = foreign "get_for_var" 
  (seq_stmt @-> returning seq_var)

let set_base = foreign "set_base"
  (seq_stmt @-> seq_func @-> returning void)

let add_stmt = foreign "add_stmt"
  (seq_stmt @-> seq_block @-> returning void)

let init_module = foreign "init_module"
  (void @-> returning seq_module)

let exec_module' = foreign "exec_module" 
  (seq_module @-> bool @-> ptr (ptr char) @-> ptr (ptr seq_srcinfo) @-> returning bool)

let exec_module mdl debug = 
  let err_addr = Ctypes.allocate (ptr char) (from_voidp char null) in
  let src_addr = Ctypes.allocate (ptr seq_srcinfo) (from_voidp seq_srcinfo null) in
  let ret = exec_module' mdl debug err_addr src_addr in

  if not ret then
    let msg = coerce (ptr char) string (!@ err_addr) in
    let pos = get_pos_t_from_srcinfo (!@ !@ src_addr) in
    raise (SeqCError (msg, pos))

