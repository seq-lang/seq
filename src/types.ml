(* 786 *)

open Llvm

let void_t   = void_type Init.llc
let float_t  = double_type Init.llc
let i8_t     = i8_type Init.llc
let i64_t    = i64_type Init.llc
let ptr_t    = i8_type Init.llc |> pointer_type
let bool_t   = i1_type Init.llc
let str_t    = named_struct_type Init.llc "str_t"
let arr_t    = named_struct_type Init.llc "arr_t"

type seqtype =
  | Void
  | Int of llvalue
  | Float of llvalue
  | String of llvalue
  | Array of llvalue

let typeof = function 
  | Void -> void_t 
  | Float _ -> float_t 
  | Int _ -> i64_t
  | String _ -> str_t
  | Array _ -> arr_t

type seqfunc = {
  fn: llvalue;
  preamble: llbasicblock;
  entry: llbasicblock
}
