(* 786 *)

open Core
open Llvm
open Init
open Types
open Utils

(**********************************************************************)

let main =
  let fn = define_function "main" (function_type void_t [| |]) llm in
  let entry = entry_block fn in
  let preamble = insert_block llc "preamble" entry in
  struct_set_body str_t [| i64_t; ptr_t |] true;
  struct_set_body arr_t [| i64_t; pointer_type str_t |] true;
  {fn; preamble; entry}

let rec codegen_expr expr ?(name="") (block, input) = 
  let llb = builder_at_end llc block in
  match expr with  
  | Ast.Int i -> 
    let a = alloca i64_t main ~name:name in
    let v = constint i in
    build_store v a llb >>. block, Int a
  | Ast.Float f -> 
    let a = alloca float_t main ~name:name in
    let v = constflt f in
    build_store v a llb >>. block, Float a
  | Ast.String s -> (* TODO: cast as str_t & check with Ariya *)
    let llb = builder_at_end llc block in
    let gs = define_global "str" (const_string llc s) llm in
    set_linkage Linkage.Private gs;
    set_alignment 1 gs;
    let a = alloca ptr_t main ~name:name in
    let sp = build_gep gs [| constint 0; constint 0 |] "" llb in
    build_store sp a llb >>. block, String a
  | Ast.Pipe p -> 
    let pipe = append_block llc "pipeline" main.fn in
    build_br pipe llb |> ignore;
    codegen_pipe (pipe, input) p
  | _ -> 
    error "Not implemented"

and codegen_pipe (block, input) = function
  | [] -> (block, Void)
  | Ast.Identifier(name, args) :: tl -> 
    let llb = builder_at_end llc block in 
    let args = List.map args ~f:(fun x -> 
      let name = sprintf "%s_arg" name in
      let block, ret = codegen_expr x (block, Void) ~name:(name ^ "_v") in
      let v = match ret with
        | Void -> error "Argument cannot be void"
        | Int v | Float v | String v | Array v -> v
      in
      build_load v name llb) 
    in
    codegen_func (name, Array.of_list args) (block, input) tl
  | hd :: tl -> 
    let ret = codegen_expr hd (block, input) in
    codegen_pipe ret tl

and codegen_func (name, args) (block, input) tl = match name with
  | "load" -> begin
    if Array.length args <> 1 then error "load needs 1 argument" else
    assert (1 = match input with Void -> 1 | _ -> 0);
    let llb = builder_at_end llc block in 
    
    let fn sfx ret arg = 
      let fty = function_type ret arg in
      let func = declare_function ("seq_" ^ sfx) fty llm in
      set_function_call_conv CallConv.c func; 
      func
    in
    (* void *seqSourceInit(char *sources) *)
    let finit = fn "init" ptr_t [| ptr_t |] in
    (* seq_int_t seqSourceRead(void *state) *)
    let fread = fn "read" i64_t [| ptr_t |] in
    (* seq_t seqSourceGetSingle(void *state, seq_int_t idx) *)
    let fget = fn "get" str_t [| ptr_t; i64_t |] in
    (* void seqSourceDealloc(void *state) *)
    let ffree = fn "free" void_t [| ptr_t |] in

    (* TODO: cast char* to str_t *)
    let state = build_call finit args "state" llb in

    let repeat = append_block llc "load_repeat" main.fn in
    build_br repeat llb |> ignore;
    position_at_end repeat llb;

    let limit = build_call fread [| state |] "read" llb in

    let loop = append_block llc "load_loop" main.fn in
    build_br loop llb |> ignore;
    position_at_end loop llb;
    
    let control = build_phi [(constint 0, repeat)] "i" llb in
    let next = build_add control (constint 1) "next" llb in
    let cond = build_icmp Icmp.Slt control limit "" llb in
    
    let body = append_block llc "load_body" main.fn in
    let branch = build_cond_br cond body body llb in

    position_at_end body llb;
    let result = build_call fget [| state; control |] "get" llb in
    
    (*  continue in body block with param result; get final block *)
    let after, _ = codegen_pipe (body, String result) tl in
    
    position_at_end after llb;
    build_br loop llb |> ignore;
    add_incoming (next, after) control;

    let exit_loop = append_block llc "load_exit_loop" main.fn in
    let exit_rep = append_block llc "load_exit_repeat" main.fn in

    set_successor branch 1 exit_loop;
    position_at_end exit_loop llb;
    let d = build_icmp Icmp.Eq limit (constint 0) "" llb in
    build_cond_br d exit_rep repeat llb |> ignore;

    position_at_end exit_rep llb;
    build_call ffree [| state |] "" llb |> ignore;

    (exit_rep, Void)
  end
  | "print" -> begin 
    if Array.length args <> 0 then error "print needs 0 arguments" else
    let llb = builder_at_end llc block in
    let arg, fn = match input with
      | Int i    -> 
        let arg = [| build_load i "arg" llb |] in
        let fty = function_type void_t [| i64_t |] in
        let fn = declare_function "print_i" fty llm in
        arg, fn
      | Float f  -> 
        let arg = [| build_load f "arg" llb |] in
        let fty = function_type void_t [| float_t |] in
        let fn = declare_function "print_f" fty llm in
        arg, fn
      | String s -> 
        let arg = [| s |] in
        let fty = function_type void_t [| str_t |] in
        let fn = declare_function "print_s" fty llm in
        arg, fn
      | _ -> "Unknown print type" |> error
    in
    set_function_call_conv CallConv.c fn;
    build_call fn arg "" llb |> ignore;
    (block, Void)
  end
  | "split" -> begin
    if Array.length args <> 2 then error "split needs 2 arguments" else
    let sublen, inc = args.(0), args.(1) in
    let llb = builder_at_end llc block in

    let len, seq = extract input main ~name:"split_input" llb  in
    let seq = build_load seq "" llb in
    let len = build_load len "" llb in
    let max = build_sub len sublen "sub" llb in

    let loop = append_block llc "split" main.fn in
    build_br loop llb |> ignore;
    position_at_end loop llb;

    let control = build_phi [(constint 0, block)] "i" llb in
    let next = build_add control inc "next" llb in
    let cond = build_icmp Icmp.Sle control max "" llb in
    
    let body = append_block llc "split_body" main.fn in
    let branch = build_cond_br cond body body llb in

    position_at_end body llb;
    let subseq = build_gep seq [| control |] "" llb in
    let subseq_v = alloca ptr_t main ~name:"subseq_v" ~value:(const_pointer_null ptr_t) in
    let sublen_v = alloca i64_t main ~name:"sublen_v" ~value:(constint 0) in
    build_store subseq subseq_v llb |> ignore;
    build_store sublen sublen_v llb |> ignore;
    
    let b = build_load sublen_v "" llb in
    let result = build_insertvalue (undef str_t) b 0 "" llb in
    let b = build_load subseq_v "" llb in
    let result = build_insertvalue result b 1 "" llb in

    let after, _ = codegen_pipe (body, String result) tl in

    position_at_end after llb;
    build_br loop llb |> ignore;
    add_incoming (next, after) control;

    let exit = append_block llc "split_exit" main.fn in
    set_successor branch 1 exit;
    
    (exit, Void)
  end
  | id -> 
    sprintf "%s not implemented" id |> error

let codegen = function 
  | Ast.Expr e -> 
    let final, _ = codegen_expr e (main.entry, Void) in
    let llb = builder_at_end llc main.preamble in
    build_br main.entry llb |> ignore;
    let exit = append_block llc "exit" main.fn in
    let llb = builder_at_end llc exit in
    build_ret_void llb |> ignore;
    position_at_end final llb;
    build_br exit llb |> ignore
  | _ -> 
    error "N/A !!!"
