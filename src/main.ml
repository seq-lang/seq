(* 786 *)

open Core

open Ast
open Seqtypes

exception SeqCamlError of string

(* 
TODO: 
- matches
- type shadowing
- ast/error
*)

type assignable = 
  | Var of seq_var
  | Func of seq_func
  | Type of seq_type
type context = { 
  base: seq_func; 
  map: (string, assignable) Hashtbl.t;
  stack: (string list) Stack.t 
}

let init_context fn = 
  let ctx = {base = fn; map = String.Table.create (); stack = Stack.create ()} in
  (* initialize POD types *)
  Hashtbl.set ctx.map ~key:"void"  ~data:(Type (void_type ()));
  Hashtbl.set ctx.map ~key:"int"   ~data:(Type (int_type ()));
  Hashtbl.set ctx.map ~key:"str"   ~data:(Type (str_type ()));
  Hashtbl.set ctx.map ~key:"bool"  ~data:(Type (bool_type ()));
  Hashtbl.set ctx.map ~key:"float" ~data:(Type (float_type ()));
  ctx

(* placeholder for NotImplemented *)
let noimp s = 
  raise (NotImplentedError ("Not yet implemented: " ^ s))

(* return seq_type for a string name *)
let get_type_exn ctx (typ: string) = 
  match Hashtbl.find ctx.map typ with
  | Some (Type t) -> t
  | _ -> noimp (sprintf "Type %s" typ)
let has_type ctx s = 
  match Hashtbl.find ctx.map s with Some (Type _) -> true | _ -> false

let rec get_seq_expr ctx = function
  | Array -> raise (SeqCamlError "Array needs to be part of the CallExpr")
  | Bool(b) -> bool_expr b
  | Int(i) -> int_expr i
  | Float(f) -> float_expr f
  | String(s) -> str_expr s
  | Id(var) -> (match Hashtbl.find ctx.map var with
    | Some assgn -> (match assgn with
      | Var v -> var_expr v
      | Func f -> func_expr f
      | Type _ -> noimp (sprintf "Type expression %s" var))
    | None -> raise (SeqCamlError (sprintf "Variable %s not found" var)))
  | IfExpr(cond, if_expr, else_expr) ->
    let if_expr = get_seq_expr ctx if_expr in 
    let else_expr = get_seq_expr ctx else_expr in
    cond_expr (get_seq_expr ctx cond) if_expr else_expr
  | Not(expr) -> uop_expr "!" (get_seq_expr ctx expr)
  | Cond(lh_expr, op, rh_expr)
  | Binary(lh_expr, op, rh_expr) ->
    let lh_expr = get_seq_expr ctx lh_expr in
    let rh_expr = get_seq_expr ctx rh_expr in
    bop_expr op lh_expr rh_expr
  | Call(callee_expr, args) -> begin
    match callee_expr with 
    | Array -> (match args with
      | [Id(typ); size_expr] -> array_expr (get_type_exn ctx typ) (get_seq_expr ctx size_expr)
      | _ -> raise (SeqCamlError "Array constructor requires 2 arguments: one string and one int"))
    | Id(typ) when has_type ctx typ ->
      let typ = get_type_exn ctx typ in
      construct_expr typ @@ List.map args ~f:(get_seq_expr ctx)  
    | _ ->
      let callee_expr = get_seq_expr ctx callee_expr in
      let args_exprs = List.map args ~f:(get_seq_expr ctx) in
      call_expr callee_expr args_exprs
    end
  | Dot(lh_expr, rhs) -> begin
    match lh_expr with
    | Id(typ) when has_type ctx typ -> static_expr (get_type_exn ctx typ) rhs
    | _ -> get_elem_expr (get_seq_expr ctx lh_expr) rhs
    end
  | Index(lh_expr, index_expr) ->
    let lh_expr = get_seq_expr ctx lh_expr in begin
    match index_expr with
    | Slice(st, ed, step) ->
      if is_some step then noimp "Step";
      let st = match st with None -> Ctypes.null | Some st -> get_seq_expr ctx st in
      let ed = match ed with None -> Ctypes.null | Some ed -> get_seq_expr ctx ed in
      array_slice_expr lh_expr st ed
    | _ -> array_lookup_expr lh_expr (get_seq_expr ctx index_expr)
    end
  | Tuple(args) ->
    record_expr @@ List.map args ~f:(get_seq_expr ctx) 
  | _ -> noimp "Unknown expr"

let match_arg = function
  | PlainArg n -> (n, "'") 
  | TypedArg(n, t) -> (n, match t with Some t -> t | None -> "")

let rec get_seq_stmt ctx block stmt = 
  let stmt = match stmt with
  | Pass -> pass_stmt ()
  | Break -> break_stmt ()
  | Continue -> continue_stmt ()
  | Statements stmts ->
    List.iter stmts ~f:(get_seq_stmt ctx block);
    pass_stmt ()
  | Assign(lh_expr, rh_expr) ->
    let rh_expr = get_seq_expr ctx rh_expr in begin 
    match lh_expr with
    | Id(var) -> begin
      match Hashtbl.find ctx.map var with
      | Some v -> (match v with 
        | Var v | Func v -> assign_stmt v rh_expr 
        | Type _ -> noimp "Type assignment (should it be?)")
      | None -> 
        let var_stmt = var_stmt rh_expr in
        Hashtbl.set ctx.map ~key:var ~data:(Var (var_stmt_var var_stmt));
        Stack.push ctx.stack (var::Stack.pop_exn ctx.stack);
        var_stmt
      end
    | Dot(lh_expr, rhs) ->
      assign_member_stmt (get_seq_expr ctx lh_expr) rhs rh_expr
    | Index(var_expr, index_expr) -> 
      assign_index_stmt (get_seq_expr ctx var_expr) (get_seq_expr ctx index_expr) rh_expr
    | _ -> raise (SeqCamlError "Assignment requires Id / Dot / Index on LHS")
    end
  | Exprs expr ->
    expr_stmt @@ get_seq_expr ctx expr
  | Print print_expr ->
    print_stmt @@ get_seq_expr ctx print_expr
  | Return ret_expr ->
    let ret_stmt = return_stmt @@ get_seq_expr ctx ret_expr in
    set_func_return ctx.base ret_stmt; 
    ret_stmt
  | Yield yield_expr ->
    let yield_stmt = yield_stmt @@ get_seq_expr ctx yield_expr in
    set_func_yield ctx.base yield_stmt; 
    yield_stmt
  | Type(name, args) ->
    let arg_names, arg_types = List.unzip @@ List.map args ~f:match_arg in
    if is_some (Hashtbl.find ctx.map name) then
      raise (SeqCamlError (sprintf "Type %s already defined" name));
    let typ = record_type arg_names (List.map arg_types ~f:(get_type_exn ctx)) in
    Hashtbl.set ctx.map ~key:name ~data:(Type typ);
    pass_stmt ()
  | If(if_blocks) -> 
    let if_stmt = if_stmt () in
    List.iter if_blocks ~f:(fun (cond_expr, stmts) ->
      let if_block = match cond_expr with 
      | None -> get_else_block if_stmt
      | Some cond_expr -> get_elif_block if_stmt @@ get_seq_expr ctx cond_expr
      in
      Stack.push ctx.stack [];
      List.iter stmts ~f:(get_seq_stmt ctx if_block);
      Stack.pop_exn ctx.stack |> List.iter ~f:(Hashtbl.remove ctx.map));
    if_stmt
  | While(cond_expr, stmts) ->
    let cond_expr = get_seq_expr ctx cond_expr in
    let while_stmt = while_stmt(cond_expr) in
    let while_block = get_while_block while_stmt in
    Stack.push ctx.stack [];
    List.iter stmts ~f:(get_seq_stmt ctx while_block);
    Stack.pop_exn ctx.stack |> List.iter ~f:(Hashtbl.remove ctx.map);
    while_stmt
  | For(for_var, gen_expr, stmts) ->
    let gen_expr = get_seq_expr ctx gen_expr in
    let for_stmt = for_stmt gen_expr in

    let for_var_name, for_var = match for_var with
    | Id for_var_name -> (for_var_name, get_for_var for_stmt)
    | _ -> noimp "For non-ID variable"
    in
    (* for variable shadows the original variable if it exists *)
    let prev_var = Hashtbl.find ctx.map for_var_name in
    let for_block = get_for_block for_stmt in
    Stack.push ctx.stack [for_var_name]; 
    Hashtbl.set ctx.map ~key:for_var_name ~data:(Var for_var);
    List.iter stmts ~f:(get_seq_stmt ctx for_block);
    Stack.pop_exn ctx.stack |> List.iter ~f:(Hashtbl.remove ctx.map);
    begin match prev_var with 
    | Some prev_var -> Hashtbl.set ctx.map ~key:for_var_name ~data:prev_var
    | _ -> ()
    end;
    for_stmt
  | Match(what_expr, cases) ->
    let match_stmt = match_stmt (get_seq_expr ctx what_expr) in
    List.iter cases ~f:(fun (cond, bound, stmts) -> 
      Stack.push ctx.stack [];
      let pat, bound_var_name, prev_var = match bound with 
      | Some bound_var_name ->
        let prev_var = Hashtbl.find ctx.map bound_var_name in
        Stack.push ctx.stack [bound_var_name];
        
        let pat = bound_pattern @@ get_seq_case_pattern ctx cond in
        Hashtbl.set ctx.map ~key:bound_var_name ~data:(Var (get_bound_pattern_var pat));
        pat, bound_var_name, prev_var
      | None -> 
        let pat = get_seq_case_pattern ctx cond in
        pat, "", None
      in
      let case_block = add_match_case match_stmt pat in
      List.iter stmts ~f:(get_seq_stmt ctx case_block);
      Stack.pop_exn ctx.stack |> List.iter ~f:(Hashtbl.remove ctx.map);
      begin match prev_var with 
      | Some prev_var -> Hashtbl.set ctx.map ~key:bound_var_name ~data:prev_var
      | _ -> ()
      end);
    match_stmt
  | Function(_, _, _) as fn ->
    let _, fn = get_seq_fn ctx fn in 
    func_stmt fn
  | Class(class_name, args, functions) ->
    if is_some (Hashtbl.find ctx.map class_name) then
      raise (SeqCamlError (sprintf "Class %s already defined" class_name));
    
    let arg_names, arg_types = List.unzip @@ List.map args ~f:match_arg in
    let record_typ = record_type arg_names (List.map arg_types ~f:(get_type_exn ctx)) in
    let typ = ref_type class_name record_typ in
    Hashtbl.set ctx.map ~key:class_name ~data:(Type typ);
    
    List.iter functions ~f:(fun f -> 
      let name, fn = get_seq_fn ctx f ~parent_class:class_name in 
      add_ref_method typ name fn);
    set_ref_done typ;
    pass_stmt ()
  | _ -> noimp "Unknown stmt"
  in 
  set_base stmt ctx.base;
  add_stmt stmt block

and get_seq_fn ctx ?parent_class = function 
  | Function(return_typ, args, stmts) ->
    let fn_name, _ = match_arg return_typ in
    if is_some @@ Hashtbl.find ctx.map fn_name then 
      SeqCamlError (sprintf "Cannot define function %s as the variable with same name exists" fn_name) |> raise;
    
    let fn = func fn_name in
    (* add it to the table only if it is "pure" function *)
    if is_none parent_class then 
      Hashtbl.set ctx.map ~key:fn_name ~data:(Func fn);

    let arg_names, arg_types = List.unzip @@ List.map args ~f:match_arg in 
    (* Handle "self" reference *)
    let arg_types = (match arg_names, parent_class with 
      | hd::_, Some parent_class when hd = "self" -> parent_class::(List.tl_exn arg_types)
      | _ -> arg_types) 
    in
    (* handle generics *)
    let generic_types = String.Table.create () in
    let arg_types, generic_count = List.fold arg_types ~init:([], 0) ~f:(fun (acc_types, acc_count) t -> 
      match Hashtbl.find ctx.map t, String.length t with 
      | Some _, _ -> (t::acc_types, acc_count)
      | _, 0 -> 
        let t = (sprintf "''%d" acc_count) in (* in-code genrics can only have one quote *)
        Hashtbl.set generic_types ~key:t ~data:acc_count;
        (t::acc_types, acc_count + 1)
      | _, _ when t.[0] = '\'' -> (match Hashtbl.find generic_types t with
        | Some _ -> (t::acc_types, acc_count)
        | None ->
          Hashtbl.set generic_types ~key:t ~data:acc_count;
          (t::acc_types, acc_count + 1))
      | _, _ -> noimp (sprintf "Type %s" t))
    in
    set_func_generics fn generic_count;
    let arg_types = List.map (List.rev arg_types) ~f:(fun t ->
      match Hashtbl.find generic_types t with 
      | Some generic_index -> set_func_generic_name fn generic_index t; get_func_generic fn generic_index
      | _ -> get_type_exn ctx t)
    in
    set_func_params fn arg_names arg_types;

    (* handle statements *)
    let fn_ctx = init_context fn in
    (* functions inherit types and functions; variables are off-limits *)
    Hashtbl.iteri ctx.map ~f:(fun ~key ~data -> match data with
      Type _ | Func _ -> Hashtbl.set fn_ctx.map ~key ~data | _ -> ());
    List.iter arg_names ~f:(fun arg_name -> 
      Hashtbl.set fn_ctx.map ~key:arg_name ~data:(Var (get_func_arg fn arg_name)));
    Stack.push fn_ctx.stack arg_names;
    
    let fn_block = get_func_block fn in
    List.iter stmts ~f:(get_seq_stmt fn_ctx fn_block);
    (fn_name, fn)
  | _ -> raise (SeqCamlError "get_seq_func MUST HAVE Function as an input")

and get_seq_case_pattern ctx = function
  (*  condition, guard, statements *)
  | None -> wildcard_pattern ()
  | Some (Int i) -> int_pattern i
  | Some (String s) -> str_pattern s
  | Some (Bool b) -> bool_pattern b
    (* | Seq s -> seq_pattern s *)
    (* | List l -> list_pattern l *)
  | _ -> noimp "Match condition"

let seq_exec ast = 
  let seq_module = init_module () in
  let module_block = get_module_block seq_module in
  let ctx = init_context seq_module in
  Stack.push ctx.stack [];
  match ast with Module stmts -> 
    List.iter stmts ~f:(get_seq_stmt ctx module_block);
  exec_module seq_module false
  
let () = 
  fprintf stderr "behold the ocaml-seq!\n";
  if Array.length Sys.argv < 2 then begin
    noimp "No arguments"
  end;
  let infile = Sys.argv.(1) in
  let lines = In_channel.read_lines infile in
  let code = (String.concat ~sep:"\n" lines) ^ "\n" in
  let lexbuf = Lexing.from_string code in
  let state = Lexer.stack_create () in
  try
    (* fprintf stderr "|> Code ==> \n%s\n" code; *)
    let ast = Parser.program (Lexer.token state) lexbuf in  
    fprintf stderr "|> AST::Caml ==> \n%s\n" @@ Ast.prn_ast ast;
    fprintf stderr "|> C++ ==>\n%!";
    seq_exec ast
  with 
  | Lexer.SyntaxError msg ->
    fprintf stderr "!! Lexer error: %s\n%!" msg
  | Parser.Error ->
    let print_position lexbuf =
      let pos = lexbuf.Lexing.lex_curr_p in
      sprintf "%s;line %d;pos %d" pos.pos_fname pos.pos_lnum (pos.pos_cnum - pos.pos_bol + 1)
    in
    fprintf stderr "!! Menhir error %s: %s\n%!" (print_position lexbuf) (Lexing.lexeme lexbuf)
  | SeqCamlError msg ->
    fprintf stderr "!! OCaml/Seq error: %s\n%!" msg
  | Failure msg ->
    fprintf stderr "!! C++/JIT error: %s\n%!" msg
