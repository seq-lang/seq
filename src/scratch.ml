(* ------- END ------- *)

(* let var_map = Hashtbl.create () *)
(* let reserved_map:(string, seqsig * llvalue) Hashtbl.t = Hashtbl.create () *)
(* let initialize_reserved () = 
  let fns = [
    ("printi", {i=Int; o=Void}); 
    ("printf", {i=Float; o=Void});
    ("prints", {i=String; o=Void});
  ] in
  List.map fns ~f:(fun (name, sg) ->
    let fty = function_type (lltype sg.o) [| lltype sg.i |] in
    let llf = declare_function name fty llm in
    (name, (sg, llf))
  ) |> String.Table.of_alist_exn

let reserved_map = initialize_reserved ()
let var_map:(seqtype * seqtype * llvalue) String.Table.t = String.Table.create () *)

  (* | Ast.Identifier(name, args) ->  *)
    (* let args = List.map args ~f:(fun x -> 
      let ret = codegen_expr expr Void program.preamble in
      let v = match ret with
        | Void -> error "whoops"
        | Int v | Float v | String v | Array v -> v
      in
      let t = typeof ret in 
      let a = alloca t program.preamble in
      let s = build_store a v llb in
      let l = build_load s "arg" llb in
      l
    ) in *)
    (* codegen_func name args (program.entry, input) *)


    (* begin
    match Hashtbl.find reserved_map id with
    | Some (sg, ll) -> "not implemented" |> error
    | None -> "not implemented" |> error
      (* match Hashtbl.find var_map with
      (* check variables; TODO: userdef functions *)
      | Some (t, v) -> (t, v) 
      | None -> sprintf "Unknown identifier %s" id |> error *)
    end *)
    
  (*| Binary(e1, op, e2) -> 
    let (lt, lhs) = codegen_expr e1 in
    let (rt, rhs) = codegen_expr e2 in 
    let ari_op f = f lhs rhs "ari" llb in
    let int_s = {i=Void; o=Int} in
    let flt_s = {i=Void; o=Float} in
    match op with
    | "+" ->
      if lt = int_s && rt = int_s then 
        (int_s, ari_op build_add)
      else if lt = flt_s && rt = flt_s then 
        (flt_s, ari_op build_fadd)
      else 
        sprintf "Types in + do not match" |> error
    | "-" -> 
      if lt = int_s && rt = int_s then 
        (int_s, ari_op build_sub)
      else if lt = flt_s && rt = flt_s then 
        (flt_s, ari_op build_fsub)
      else 
        sprintf "Types in - do not match" |> error
    | "*" -> 
      "not implemented" |> error
    | _ -> (* missing: branch, leq *)
      sprintf "Unknown operator %s" op |> error*)

(* returns (type * type) * llval *)


(*
let codegen_init () = 
  printf  "hello\n";

  let fty = function_type void_t [| |] in
  let init_f = define_function "init" fty llm in
  
  let init_g = define_global "init_g" (const_null bool_t) llm in
  
  let init_b = append_block llc "init" init_f in
  let exit_b = append_block llc "exit" init_f in
  
  let llb = builder_at_end llc (entry_block init_f) in
  let init_v = build_load init_g "" llb in
  build_cond_br init_v exit_b init_b llb |> ignore;
  
  position_at_end init_b llb;
  build_store (const_int bool_t 1) init_g llb |> ignore;
  build_ret_void llb |> ignore;
  
  position_at_end exit_b llb;
  build_ret_void llb |> ignore;

  dump llm

let codegen_split k step fn input = 
  let preamble_b = ?? in

  let k = const_int i64_t k in
  let step = const_int i64_t step in

  let llb = builder_at_end llc (entry_block fn) in
  let seq = build_load input.seq "" llb in
  let len = build_load input.len "" llb in
  let max = build_sub len k "sub" llb in

  let loop = append_block llc "split" fn in
  build_br loop |> ignore;
  position_at_end loop llb;

  let ctrl = build_phi [??] "i" llb in
  let next = build_add ctrl step "next" llb in
  let cond = build_icmp ?? ctrl max "" llb in
  let body = append_block llc "body_split" fn in
  let instr = build_cond_br cond body body llb in

  position_at_end body llb;
  let subseq = build_gep seq [ctrl] "" llb in
  let subsqv = alloca ptr_t ~st:(const_pointer_null ptr_t) preamble in
  let sublnv = alloca i64_t ~st:(const_int i64_t 0) preamble in
  build_store subseq subsqv |> ignore;  
  build_store k sublnv |> ignore;  

  List.append outs (subsqv, sublnv);
  codegen mdl;

  position_at_end after lbb;
  build_br loop lbb |> ignore;

let codegen_module pipelines =
  let seq_t = named_struct_type llc "seq_t" in
  struct_set_body seq_t [| i64_t; ptr_t |] true ;

  let str_t = named_struct_type llc "str_t" in
  struct_set_body str_t [| i64_t; ptr_t |] true ;

  let arr_t = named_struct_type llc "arr_t" in
  struct_set_body arr_t [| i64_t; pointer_type str_t |] true ;

  let fty = function_type void_t [| arr_t |] in
  let main_f = define_function "main" fty llm in

  let preamble_b = append_block llc "preamble" main_f in
  let llb = builder_at_end llc preamble_b in
  build_br (entry_block main_f) llb |> ignore;

  (* unpack *)
  let args = param_begin main_f in
  let seq = build_extractvalue args 1 "seq" llb in
  let seq_v = build_alloca str_t "seq_v" llb in  
  build_store seq seq_v llb |> ignore;

  let len = build_extractvalue args 0 "len" llb in
  let len_v = build_alloca i64_t "len_v" llb in 
  build_store len len_v llb |> ignore;


  match lookup_function "init" llm with
    | Some f -> build_call f [| |] "init" llb |> ignore 
    | None -> CompileError "Can't find init function" |> raise 
  ;
  
  (* base stage *)

  position_at_end (entry_block main_f) llb;
  pipelines |> List.iter ~f:(fun x->
    (* validate x; *)
    let pip_b = append_block llc "pipeline" main_f in
    position_at_end pip_b llb;
    build_br pip_b llb |> ignore;
    (* codegen x mdl pip_b; *)
  );

  let exit_b = append_block llc "exit" main_f in
  position_at_end exit_b llb;
  build_ret_void llb |> ignore;
  
  (* jump from last pipeline block to exit *)
  let last_b = basic_blocks main_f |> Array.last in
  position_at_end last_b llb;
  build_br exit_b llb |> ignore;
*)