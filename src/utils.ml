(* 786 *)

open Core
open Llvm
open Types

let (>>.) f g = f |> ignore ; g

(******************** LLVM utils ********************)

let error x = Init.CompileError x |> raise

let constflt f = const_float float_t f
let constint i = const_int i64_t i

let alloca var ?value ?lb ?(name="") fn =
   let lpb = builder_at_end Init.llc fn.preamble in
   let vb = build_alloca var name lpb in
   match value, lb with
    | Some value', None -> build_store value' vb lpb >>. vb
    | Some value', Some lb' -> build_store value' vb lb' >>. vb
    | _ -> vb

let extract s fn ?(name="") llb = match s with
  | String s' ->
    let l = build_extractvalue s' 0 "" llb in
    let s = build_extractvalue s' 1 "" llb in
    let ll = alloca i64_t fn ~value:l ~lb:llb ~name:(name ^ "_l") in
    let ls = alloca ptr_t fn ~value:s ~lb:llb ~name:(name ^ "_s") in
    ll, ls
  | _ -> error "not implemented"

let dump () = 
  printf ">> final dump:\n%s%!" (string_of_llmodule Init.llm)

let dumpf fn llb = 
  printf ">> dump [in block %s] ******************************************\n" 
    (value_name @@ value_of_block @@ insertion_block llb);
  printf "%s\n%!" (string_of_llvalue fn)
