(* 786 *)

let llc = Llvm.global_context () 
let llm = Llvm.create_module llc "seq"

exception CompileError of string
