(* 786 *)

open Core

open Ast
open Seqtypes
module T = ANSITerminal 

exception SeqCamlError of string * pos_t


(* context hashtable members *)
type assignable = 
  | Var  of seq_var
  | Func of seq_func
  | Type of seq_type

type context = { 
  prefix: string;
  filename: string;
  base: seq_func;
  mdl: seq_func; 
  map: (string, assignable) Hashtbl.t;
  stack: (string list) Stack.t 
}

let dummy_pos: pos_t = {pos_fname=""; pos_cnum=0; pos_lnum=0; pos_bol=0}


let init_context fn mdl filename = 
  let ctx = {base = fn; mdl; map = String.Table.create (); stack = Stack.create (); prefix=""; filename} in
  (* initialize POD types *)
  Hashtbl.set ctx.map ~key:"void"  ~data:(Type (void_type ()));
  Hashtbl.set ctx.map ~key:"int"   ~data:(Type (int_type ()));
  Hashtbl.set ctx.map ~key:"str"   ~data:(Type (str_type ()));
  Hashtbl.set ctx.map ~key:"seq"   ~data:(Type (str_seq_type ()));
  Hashtbl.set ctx.map ~key:"bool"  ~data:(Type (bool_type ()));
  Hashtbl.set ctx.map ~key:"float" ~data:(Type (float_type ()));
  Hashtbl.set ctx.map ~key:"file"  ~data:(Type (file_type ()));
  (* Hashtbl.set ctx.map ~key:"array" ~data:(Type (array_type (generic_type ()))); *)
  ctx

(* placeholder for NotImplemented *)
let noimp s = 
  raise (NotImplentedError ("Not yet implemented: " ^ s))
let seq_error msg pos =
  raise (SeqCamlError (msg, pos))

let rec get_seq_expr ctx expr = 
  let expr, pos = begin
  match expr with
  | Bool(b, pos)    -> bool_expr b, pos
  | Int(i, pos)     -> int_expr i, pos
  | Float(f, pos)   -> float_expr f, pos
  | String(s, pos)  -> str_expr s, pos
  | Seq(s, pos)     -> str_seq_expr s, pos
  | Generic(var, pos)
  | Id(var, pos)    -> begin
    match Hashtbl.find ctx.map var with
    | Some (Var v)  -> var_expr v, pos
    | Some (Func f) -> func_expr f, pos
    | Some (Type t) -> type_expr t, pos 
    | None -> seq_error (sprintf "%s not found" var) pos
    end
  | TypeOf(_, _) ->
    noimp "TypeOf"
    (* let expr = get_seq_expr ctx expr in 
    let typ = get_type expr ctx.base in
    type_expr typ, pos *)
  | IfExpr(cond, if_expr, else_expr) ->
    let if_expr = get_seq_expr ctx if_expr in 
    let else_expr = get_seq_expr ctx else_expr in
    let c_expr = get_seq_expr ctx cond in 
    let pos = get_pos c_expr in
    cond_expr c_expr if_expr else_expr, pos
  | Unary((op, pos), expr) -> 
    uop_expr op (get_seq_expr ctx expr), pos
  | Binary(lh_expr, (op, pos), rh_expr) ->
    let lh_expr = get_seq_expr ctx lh_expr in
    let rh_expr = get_seq_expr ctx rh_expr in
    bop_expr op lh_expr rh_expr, pos
  | Call(callee_expr, args) -> begin
    let callee_expr = get_seq_expr ctx callee_expr in
    match get_type callee_expr with
    | Some typ ->    
      construct_expr typ (List.map args ~f:(get_seq_expr ctx)), get_pos callee_expr
    | None -> (* fn[t](...) *)
      let args_exprs = List.map args ~f:(function
        | Ellipsis -> Ctypes.null
        | ex -> get_seq_expr ctx ex) in
      let pos = get_pos callee_expr in
      if List.exists args ~f:((=)(Ellipsis)) then   
        partial_expr callee_expr args_exprs, pos
      else 
        call_expr callee_expr args_exprs, pos
    end
  | Pipe exprs ->
    let exprs = List.map exprs ~f:(get_seq_expr ctx) in
    pipe_expr exprs, get_pos (List.hd_exn exprs)
  | Dot(lh_expr, (rhs, pos)) -> begin
    let lh_expr = get_seq_expr ctx lh_expr in
    match get_type lh_expr with
    | Some typ -> static_expr typ rhs, pos
    | None -> get_elem_expr lh_expr rhs, pos
    end
  | Index(lh_expr, [Slice(st, ed, step, pos)]) ->
    let lh_expr = get_seq_expr ctx lh_expr in 
    if is_some step then noimp "Step";
    let unpack st = match st with 
    | None -> Ctypes.null 
    | Some st -> get_seq_expr ctx st in
    array_slice_expr lh_expr (unpack st) (unpack ed), pos
  | Index(Id("array", pos), indices) -> 
    if List.length indices <> 1 then 
      seq_error "Array needs only one type" pos;
    let typ = get_type_from_expr_exn ctx (List.hd_exn indices) in
    type_expr (array_type typ), pos
  | Index(Id("callable", pos), indices) -> 
    let typ_exprs = List.map indices ~f:(get_type_from_expr_exn ctx) in
    let ret, args = match List.rev typ_exprs with
    | hd::tl -> hd, tl |> List.rev
    | [] -> seq_error "Callable needs at least one argument" pos in
    type_expr (func_type ret args), pos
  | Index(Id("yieldable", pos), indices) -> 
    if List.length indices <> 1 then 
      seq_error "Yieldable needs only one type" pos;
    let typ = get_type_from_expr_exn ctx (List.hd_exn indices) in
    type_expr (gen_type typ), pos
  | Index(lh_expr, indices) -> begin
    let lh_expr = get_seq_expr ctx lh_expr in
    let index_exprs = List.map indices ~f:(get_seq_expr ctx) in
    match get_type lh_expr with 
    | Some typ ->
      let typ = realize_type typ @@ List.map indices ~f:(get_type_from_expr_exn ctx) in
      type_expr typ, get_pos lh_expr
    | None when get_expr_name lh_expr = "func" ->
      let fn = realize_func lh_expr @@ List.map indices ~f:(get_type_from_expr_exn ctx) in
      func_expr fn, get_pos lh_expr
    | _ ->
      if List.length index_exprs <> 1 then 
        seq_error "Index needs only one item" (get_pos lh_expr);
      array_lookup_expr lh_expr (List.hd_exn index_exprs), get_pos lh_expr
    end
  | Tuple(args, pos) ->
    record_expr (List.map args ~f:(get_seq_expr ctx)), pos
  | _ -> noimp "Unknown expr"
  end
  in 
  set_pos expr pos;
  expr
and get_type_from_expr_exn ctx (t:Ast.expr) = 
  let typ_expr = get_seq_expr ctx t in
  match get_type typ_expr with
  | Some typ -> typ
  | _ -> seq_error "Not a type" (get_pos typ_expr)

let set_generics ctx types args set_generic_count get_generic =
  let arg_names, arg_types = List.unzip @@ List.map args ~f:(function 
    | Arg((n, pos), None) -> (n, Generic(sprintf "``%s" n, pos))
    | Arg((n, _), Some t) -> (n, t)) in 
  
  let generics = List.append types arg_types  
    |> List.filter_map ~f:(function Generic(g, _) -> Some g | _ -> None) 
    |> List.dedup_and_sort ~compare in
  set_generic_count (List.length generics);
  List.iteri generics ~f:(fun cnt key ->
    let data = Type (get_generic cnt key) in
    Hashtbl.set ctx.map ~key ~data);

  let arg_types = List.map arg_types ~f:(get_type_from_expr_exn ctx) in
  arg_names, arg_types

let rec get_seq_stmt ctx block stmt : unit = 
  let stmt, pos = begin 
  match stmt with
  | Pass pos     -> pass_stmt (), pos
  | Break pos    -> break_stmt (), pos
  | Continue pos -> continue_stmt (), pos
  | Statements stmts ->
    List.iter stmts ~f:(get_seq_stmt ctx block);
    pass_stmt (), dummy_pos
  | Assign(Id(var, pos), rh_expr, shadow) -> begin
    let rh_expr = get_seq_expr ctx rh_expr in 
    match Hashtbl.find ctx.map var with
    | Some (Var _) when shadow ->  
      let var_stmt = var_stmt rh_expr in
      Hashtbl.set ctx.map ~key:var ~data:(Var (var_stmt_var var_stmt));
      Stack.push ctx.stack (var::Stack.pop_exn ctx.stack);
      var_stmt, pos
    | Some (Var v) ->
      assign_stmt v rh_expr, pos
    | Some (Func v) -> 
      assign_stmt v rh_expr, pos
    | Some (Type _) -> 
      noimp "Type assignment (should it be?)"
    | None -> 
      let var_stmt = var_stmt rh_expr in
      Hashtbl.set ctx.map ~key:var ~data:(Var (var_stmt_var var_stmt));
      Stack.push ctx.stack (var::Stack.pop_exn ctx.stack);
      var_stmt, pos
    end
  | Assign(Dot(lh_expr, (rhs, pos)), rh_expr, _) -> (* a = b *)
    let rh_expr = get_seq_expr ctx rh_expr in 
    assign_member_stmt (get_seq_expr ctx lh_expr) rhs rh_expr, pos
  | Assign(Index(var_expr, [index_expr]), rh_expr, _) -> (* a = b *)
    let index_expr = get_seq_expr ctx index_expr in
    let rh_expr = get_seq_expr ctx rh_expr in 
    let pos = get_pos index_expr in
    assign_index_stmt (get_seq_expr ctx var_expr) index_expr rh_expr, pos
  | Assign(e, _, _) ->
    let expr = get_seq_expr ctx e in
    seq_error "Assignment requires Id / Dot / Index on LHS" (get_pos expr)
  | Exprs expr ->
    let expr = get_seq_expr ctx expr in
    expr_stmt expr, (get_pos expr)
  | Print (print_exprs, pos) ->
    (* TODO: fix pos arguments *)
    List.iteri print_exprs ~f:(fun i ps -> 
      (* eprintf "~~ %s%!\n" @@ prn_expr (fun x -> "") ps; *)
      if i > 0 then begin
        let stmt = String(" ", pos) |> get_seq_expr ctx |> print_stmt in
        set_base stmt ctx.base;
        set_pos stmt pos;
        add_stmt stmt block
      end;
      let stmt = ps |> get_seq_expr ctx |> print_stmt in
      set_base stmt ctx.base;
      set_pos stmt pos;
      add_stmt stmt block
    );
    String("\n", pos) |> get_seq_expr ctx |> print_stmt, pos
  | Return (ret_expr, pos) ->
    let ret_stmt = return_stmt (get_seq_expr ctx ret_expr) in
    set_func_return ctx.base ret_stmt; 
    ret_stmt, pos
  | Yield (yield_expr, pos) ->
    let yield_stmt = yield_stmt (get_seq_expr ctx yield_expr) in
    set_func_yield ctx.base yield_stmt; 
    yield_stmt, pos
  | Type((name, pos), args, _) ->
    let arg_names, arg_types = List.unzip @@ List.map args ~f:(function
      | Arg((_, p), None) -> seq_error "Type with generic argument" p
      | Arg((n, _), Some t) -> (n, t)) in
    if is_some (Hashtbl.find ctx.map name) then
      raise (SeqCamlError (sprintf "Type %s already defined" name, pos));
    let typ = record_type arg_names @@ List.map arg_types ~f:(get_type_from_expr_exn ctx) in
    Hashtbl.set ctx.map ~key:name ~data:(Type typ);
    pass_stmt (), pos
  | If(if_blocks) -> 
    let if_stmt = if_stmt () in
    let positions = List.map if_blocks ~f:(fun (cond_expr, stmts, pos) ->
      let if_block = match cond_expr with 
      | None -> 
        get_else_block if_stmt
      | Some cond_expr -> 
        get_elif_block if_stmt @@ get_seq_expr ctx cond_expr in
      Stack.push ctx.stack [];
      List.iter stmts ~f:(get_seq_stmt ctx if_block);
      Stack.pop_exn ctx.stack |> List.iter ~f:(Hashtbl.remove ctx.map);
      pos) in
    if_stmt, (List.hd_exn positions)
  | While(cond_expr, stmts, pos) ->
    let cond_expr = get_seq_expr ctx cond_expr in
    let while_stmt = while_stmt(cond_expr) in
    let while_block = get_while_block while_stmt in
    Stack.push ctx.stack [];
    List.iter stmts ~f:(get_seq_stmt ctx while_block);
    Stack.pop_exn ctx.stack |> List.iter ~f:(Hashtbl.remove ctx.map);
    while_stmt, pos
  | For(for_var, gen_expr, stmts, pos) ->
    let gen_expr = get_seq_expr ctx gen_expr in
    let for_stmt = for_stmt gen_expr in

    let for_var_name, for_var = match for_var with
    | Id (for_var_name, _) -> 
      (for_var_name, get_for_var for_stmt)
    | _ -> noimp "For non-ID variable" in
    (* for variable shadows the original variable if it exists *)
    let prev_var = Hashtbl.find ctx.map for_var_name in
    let for_block = get_for_block for_stmt in
    Stack.push ctx.stack [for_var_name]; 
    Hashtbl.set ctx.map ~key:for_var_name ~data:(Var for_var);
    List.iter stmts ~f:(get_seq_stmt ctx for_block);
    Stack.pop_exn ctx.stack |> List.iter ~f:(Hashtbl.remove ctx.map);
    begin match prev_var with 
    | Some prev_var -> 
      Hashtbl.set ctx.map ~key:for_var_name ~data:prev_var
    | _ -> () 
    end;
    for_stmt, pos
  | Match(what_expr, cases, pos) ->
    let match_stmt = match_stmt (get_seq_expr ctx what_expr) in
    List.iter cases ~f:(fun (cond, bound, stmts, _) -> 
      Stack.push ctx.stack [];
      let pat, bound_var_name, prev_var = match bound with 
      | Some (bound_var_name, _) ->
        let prev_var = Hashtbl.find ctx.map bound_var_name in
        Stack.push ctx.stack [bound_var_name];
        
        let pat = bound_pattern @@ get_seq_case_pattern ctx cond in
        Hashtbl.set ctx.map ~key:bound_var_name ~data:(Var (get_bound_pattern_var pat));
        pat, bound_var_name, prev_var
      | None -> 
        let pat = get_seq_case_pattern ctx cond in
        pat, "", None in
      let case_block = add_match_case match_stmt pat in
      List.iter stmts ~f:(get_seq_stmt ctx case_block);
      Stack.pop_exn ctx.stack |> List.iter ~f:(Hashtbl.remove ctx.map);
      match prev_var with 
      | Some prev_var -> 
        Hashtbl.set ctx.map ~key:bound_var_name ~data:prev_var
      | _ -> ());
    match_stmt, pos
  | Function(_, _, _, _, pos) as fn ->
    let _, fn = get_seq_fn ctx fn in 
    func_stmt fn, pos
  | Class((class_name, name_pos), types, args, functions, pos) ->
    if is_some (Hashtbl.find ctx.map class_name) then
      seq_error (sprintf "Class %s already defined" class_name) name_pos;
    
    let typ = ref_type class_name in
    Hashtbl.set ctx.map ~key:class_name ~data:(Type typ);
    let ref_ctx = {ctx with map=Hashtbl.copy ctx.map} in
    let arg_names, arg_types = set_generics ref_ctx types args 
      (set_ref_generics typ) 
      (fun idx name -> 
        set_ref_generic_name typ idx name;
        get_ref_generic typ idx) in
    set_ref_record typ @@ record_type arg_names arg_types;

    (* functions inherit types and functions; variables are off-limits *)
    List.iter functions ~f:(fun f -> 
      let name, fn = get_seq_fn ref_ctx f ~parent_class:class_name in 
      add_ref_method typ name fn);
    set_ref_done typ;
    pass_stmt (), pos
  | Extend((class_name, _), functions, pos) ->
    let typ = match Hashtbl.find ctx.map class_name with
    | Some (Type t) -> t 
    | _ -> seq_error (sprintf "Cannot extend non-existing class %s" class_name) pos in
    (* functions inherit types and functions; variables are off-limits *)
    let ref_ctx = {ctx with map=Hashtbl.copy ctx.map} in
    List.iter functions ~f:(fun f -> 
      let name, fn = get_seq_fn ref_ctx f ~parent_class:class_name in 
      add_ref_method typ name fn);
    pass_stmt (), pos
  | Import(il, pos) ->
    List.iter il ~f:(fun ((what, _), _) ->
      let import_ctx = parse_file ctx.mdl (sprintf "%s/%s.seq" (Filename.dirname ctx.filename) what) in 
      Hashtbl.iteri import_ctx.map ~f:(Hashtbl.set ctx.map));
    pass_stmt (), pos
  | _ -> noimp "Unknown stmt"
  end
  in 
  set_base stmt ctx.base;
  set_pos stmt pos;
  add_stmt stmt block
and parse_file ?execute mdl infile = 
  let print_error kind lines ?msg (pos: pos_t) = 
    let line, col = pos.pos_lnum, pos.pos_cnum - pos.pos_bol in
    let style = [T.Bold; T.red] in
    eprintf "%s%!" @@ T.sprintf style "[ERROR] %s error: (line %d) %s\n" kind line (match msg with 
      | Some m -> sprintf "%s" m | None -> "");
    eprintf "%s%!" @@ T.sprintf style "[ERROR] %3d: %s" line (String.prefix lines.(line - 1) col);
    eprintf "%s%!" @@ T.sprintf [T.Bold; T.white; T.on_red] "%s\n%!" (String.drop_prefix lines.(line - 1) col)
  in
  let lines = In_channel.read_lines infile in
  let code = (String.concat ~sep:"\n" lines) ^ "\n" in
  let lines = Array.of_list lines in
  let lexbuf = Lexing.from_string code in
  let state = Lexer.stack_create () in
  try
    let ctx = init_context mdl mdl (Filename.realpath infile) in
    Stack.push ctx.stack [];
    let module_block = get_module_block mdl in

    let at = array_type (str_type ()) in
    let ae = construct_expr at [int_expr (Array.length Sys.argv)] in
    let av = var_stmt ae in
    set_base av ctx.base;
    add_stmt av module_block;
    let av = var_stmt_var av in
    Array.iteri Sys.argv ~f:(fun idx s ->
      (* eprintf "--> %d:%s\n" idx s; *)
      let st = assign_index_stmt (var_expr av) (int_expr idx) (str_expr s) in
      set_base st ctx.base;
      add_stmt st module_block
    ); 
    Hashtbl.set ctx.map ~key:"__argv__" ~data:(Var av);
    Stack.push ctx.stack ("__argv__"::Stack.pop_exn ctx.stack);

    let ast = Parser.program (Lexer.token state) lexbuf in  
    (*eprintf "%s%!" @@ T.sprintf [T.Bold; T.green] "|> AST of %s ==> \n" infile;
    eprintf "%s%!" @@ T.sprintf [T.green] "%s\n%!" @@ Ast.prn_ast (fun _ -> "") ast;*)
    match ast with Module stmts -> 
      List.iter stmts ~f:(get_seq_stmt ctx module_block);
    (match execute with
     | Some (true) -> exec_module ctx.mdl false;
     | _ -> ());
    ctx
  with 
  | Lexer.SyntaxError (msg, pos) ->
    print_error "Lexer" lines pos ~msg:msg;
    exit 1
  | Parser.Error ->
    (* check https://github.com/dbp/funtal/blob/e9a9b9d/parse.ml#L64-L89 *)
    print_error "Parser" lines lexbuf.lex_start_p;
    exit 1
  | SeqCamlError (msg, pos) ->
    print_error "CamlAst" lines pos ~msg:msg;
    exit 1
  | SeqCError (msg, pos) ->
    print_error "Compiler" lines pos ~msg:msg;
    exit 1
  
and get_seq_fn ctx ?parent_class = function 
  | Function(return_typ, types, args, stmts, pos) ->
    let fn_name, ret_typ = match return_typ with Arg((n, _), typ) -> n, typ in

    if is_some @@ Hashtbl.find ctx.map fn_name then 
      seq_error (sprintf "Cannot define function %s as the variable with same name exists" fn_name) pos;
    
    let fn = func fn_name in
    (* add it to the table only if it is "pure" function *)
    if is_none parent_class then 
      Hashtbl.set ctx.map ~key:fn_name ~data:(Func fn);
    
    if is_some ret_typ then begin
      let typ = get_type_from_expr_exn ctx (Option.value_exn ret_typ) in
      set_func_out fn typ
    end;

    (* handle statements *)
    let fn_ctx = {(init_context fn ctx.mdl ctx.filename) 
      with map=Hashtbl.filter ctx.map ~f:(fun v -> 
        match v with Func _ | Type _ -> true | _ -> false)} in
    let arg_names, arg_types = set_generics fn_ctx types args 
      (set_func_generics fn) 
      (fun idx name -> 
        set_func_generic_name fn idx name;
        get_func_generic fn idx) in
    set_func_params fn arg_names arg_types;

    List.iter arg_names ~f:(fun arg_name -> 
      Hashtbl.set fn_ctx.map ~key:arg_name ~data:(Var (get_func_arg fn arg_name)));
    Stack.push fn_ctx.stack arg_names;
    
    let fn_block = get_func_block fn in
    List.iter stmts ~f:(get_seq_stmt fn_ctx fn_block);
    (fn_name, fn)
  | _ -> 
    seq_error "get_seq_func MUST HAVE Function as an input" dummy_pos

and get_seq_case_pattern _ = function
  (*  condition, guard, statements *)
  | None -> wildcard_pattern ()
  | Some (Int (i, _)) -> int_pattern i
  | Some (String (s, _)) -> str_pattern s
  | Some (Seq (s, _)) -> str_seq_pattern s
  | Some (Bool (b, _)) -> bool_pattern b
  | _ -> noimp "Match condition"

let () = 
  if Array.length Sys.argv < 2 then begin
    noimp "No arguments"
  end;
  let seq_module = init_module () in
  ignore @@ parse_file seq_module Sys.argv.(1) ~execute:true
  
