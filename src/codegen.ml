(* 786 *)

open Ast
open Core
open Llvm

(* s | split(32,32) | revcomp() | print(); *)

exception CompileError of string
let error str =
  raise (CompileError str)

let llc = global_context () 
let llm = create_module llc "seq"
(* let named_values = String.Table.create () ~size:10 *)

let (>>.) f g = f |> ignore ; g
let (>>) f g x = g f x
let (<<) f g x = f g x

let void_t   = void_type llc
let float_t  = double_type llc
let i64_t    = i64_type llc
let ptr_t    = i8_type llc |> pointer_type
let bool_t   = i1_type llc
let block    = insert_block llc

(* let dump m = 
  printf "****************\n%s\n****************\n" 
    (string_of_llmodule m) *)
let dump f = 
  string_of_llvalue f |> printf "****************\n%s\n****************\n"

let alloca v ?st bl =
   let lbb = builder_at_end llc bl in
   let vb = build_alloca v "" lbb in
   match st with
    | Some s -> build_store vb s lbb >>. vb
    | None -> vb

(**********************************************************************)

type seqtype =
  | Void
  | Any
  | Int of llvalue
  | Float of llvalue
  | String of llvalue * llvalue
  | Array of llvalue * seqtype
let rec lltype = function
  | Void -> void_t 
  | Int i -> i64_t
  | Float f -> float_t 
  | String (l, ptr) -> 
    let str_t = named_struct_type llc "str_t" in
    struct_set_body str_t [| i64_t; ptr_t |] true;
    str_t
  | Array (l, ptr) ->
    let arr_t = named_struct_type llc "arr_t" in
    struct_set_body arr_t [| i64_t; pointer_type (lltype t) |] true;
    arr_t

(* let llsig sg = 
  let in_t = lltype sg.i in
  let ou_t = lltype sg.o in
  (in_t, ou_t) *)

(* let var_map = Hashtbl.create () *)
(* let reserved_map:(string, seqsig * llvalue) Hashtbl.t = Hashtbl.create () *)
let initialize_reserved () = 
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
let var_map:(seqtype * seqtype * llvalue) String.Table.t = String.Table.create ()

let fmain = define_function "main" (function_type void_t [| |]) llm
let llb = builder_at_end llc (entry_block fmain)

type fpos_t = Begin | End (* Where to start? *)

type ret_t = Type of seqtype | Loop of llbasicblock * llvalue

let llpreamble = 
  llbasicblock

let llmain = 
  lltype

(*
id:string args...:expr? inp:type/val llfn:llvalue  -> inp:type
*)
let codegen_func id args input ret llfn ?(pf=Begin) = 
  match id with
  | "split" -> begin
    let k, step = match args with
      | [| k; step |] -> const_int i64_t k, const_int i64_t step
      | _ -> "args fail" |> error
    in
    let seq, len = match input with
      | String(l, s) -> s, l
      | _ -> "Wrong input type to split" |> error
    in
    match pf with 
    | Begin -> begin
      let entry = entry_block llfn in
      let llb = builder_at_end llc entry in

      let seq = build_load seq "seq" llb in
      let len = build_load len "len" llb in
      let max = build_sub len k "sub" llb in

      let loop = append_block llc "split" llfn in
      build_br loop >>. 
      position_at_end loop llb >>.

      let phi = build_phi [] "i" llb in
      let _next = build_add phi step "next" llb in
      let cond = build_icmp Icmp.Sle phi max "" llb in
      add_incoming [ctx, entry] phi >>.
      
      let body = append_block llc "body_split" llfn in
      let _branch = build_cond_br cond body body llb in

      position_at_end body llb >>.
      let subseq = build_gep seq [| phi |] "" llb in
      let subseq_v = alloca ptr_t ~st:(const_pointer_null ptr_t) llpreamble in
      let sublen_v = alloca i64_t ~st:(const_int i64_t 0) llpreamble in
      build_store subseq subseq_v >>.
      build_store k sublen_v >>.
      
      Loop loop
    end
    | End -> begin
      let last = after in
      let llb = builder_at_end llc last in
      let loop, phi = match ret with
        | Loop (l, n) -> l, n
        | _ -> "whoops" |> error 
      in
      build_br loop llb >>.

      add_incoming [next, last] phi >>.

      let exit = append_block llc "exit_split" llfn in
      


      List.append outs (subsqv, sublnv);
      codegen mdl;

      position_at_end after lbb;
      build_br loop lbb |> ignore;
    end
    | End -> ??
  end
  | "split", End ->
  | "print" ->
    ??
  | _ -> sprintf "%s not implemented" id |> error

(* returns (type * type) * llval *)
let rec codegen = function 
  | Eof -> ({i=Void; o=Void}, mdnull llc)
  | Expr e -> codegen_expr e
  | Definition(prot, exps) -> codegen_def prot exps
and codegen_expr ?(pf=Begin) = function 
  | Int i -> ({i=Void; o=Int}, const_int i64_t i)
  | Float f -> ({i=Void; o=Float}, const_float float_t f)
  | Identifier(id, args) -> 
    (* either function call or variable *)
    begin
    match Hashtbl.find reserved_map id with
    | Some (sg, ll) -> "not implemented" |> error
    | None -> "not implemented" |> error
      (* match Hashtbl.find var_map with
      (* check variables; TODO: userdef functions *)
      | Some (t, v) -> (t, v) 
      | None -> sprintf "Unknown identifier %s" id |> error *)
    end
  | Pipe pl ->
    codegen_pipe pl
  | Binary(e1, op, e2) -> 
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
    | "|>" -> 
      if lt.o <> rt.i then
        sprintf "Pipeline types do not match" |> error
      else 
        "not implemented" |> error 
    | _ -> (* missing: branch, leq *)
      sprintf "Unknown operator %s" op |> error
and codegen_pipe = function 
  | [] -> assert false
  | [hd] -> codegen_expr hd
  | hd :: tl ->
    codegen_expr hd |> ignore;
    codegen_pipe tl |> ignore;
    codegen_expr hd ~pf:End
and codegen_call id args =
  "ooops" |> error
and codegen_def prot exps = 
  "ooops" |> error 


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