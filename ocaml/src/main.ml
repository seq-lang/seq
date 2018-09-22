(* 786 *)

open Core

open Ast
open Seqtypes
module T = ANSITerminal 

exception SeqCamlError of string * pos_t

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
let dummy_pos: pos_t = {pos_fname=""; pos_cnum=0; pos_lnum=0; pos_bol=0}

let init_context fn = 
  let ctx = {base = fn; map = String.Table.create (); stack = Stack.create ()} in
  (* initialize POD types *)
  Hashtbl.set ctx.map ~key:"void"  ~data:(Type (void_type ()));
  Hashtbl.set ctx.map ~key:"int"   ~data:(Type (int_type ()));
  Hashtbl.set ctx.map ~key:"str"   ~data:(Type (str_type ()));
  Hashtbl.set ctx.map ~key:"seq"   ~data:(Type (str_seq_type ()));
  Hashtbl.set ctx.map ~key:"bool"  ~data:(Type (bool_type ()));
  Hashtbl.set ctx.map ~key:"float" ~data:(Type (float_type ()));
  ctx

(* placeholder for NotImplemented *)
let noimp s = 
  raise (NotImplentedError ("Not yet implemented: " ^ s))

(* return seq_type for a string name *)
let get_type_exn ctx typ = 
  match Hashtbl.find ctx.map typ with
  | Some (Type t) -> t
  | _ -> noimp (sprintf "Type %s" typ)
let has_type ctx s = 
  match Hashtbl.find ctx.map s with 
  | Some (Type _) -> true 
  | _ -> false

let seq_error msg pos =
  raise (SeqCamlError (msg, pos))

(* let realize_type  *)

let rec get_seq_expr ctx expr = 
  let expr, pos = begin
  match expr with
  | Bool(b, pos) -> 
    bool_expr b, pos
  | Int(i, pos) -> 
    int_expr i, pos
  | Float(f, pos) -> 
    float_expr f, pos
  | String(s, pos) -> 
    str_expr s, pos
  | Seq(s, pos) -> 
    str_seq_expr s, pos
  | Id(var, pos) -> begin
    match Hashtbl.find ctx.map var with
    | Some assgn -> (match assgn with
      | Var v -> 
        var_expr v, pos
      | Func f -> 
        func_expr f, pos
      | Type _ -> 
        noimp (sprintf "Type expression %s" var))
    | None -> 
      seq_error (sprintf "Variable %s not found" var) pos
    end
  | IfExpr(cond, if_expr, else_expr) ->
    let if_expr = get_seq_expr ctx if_expr in 
    let else_expr = get_seq_expr ctx else_expr in
    let c_expr = get_seq_expr ctx cond in 
    let pos = get_pos c_expr in
    cond_expr c_expr if_expr else_expr, pos
  | Unary((op, pos), expr) -> 
    uop_expr op (get_seq_expr ctx expr), pos
  | Cond(lh_expr, (op, pos), rh_expr)
  | Binary(lh_expr, (op, pos), rh_expr) ->
    let lh_expr = get_seq_expr ctx lh_expr in
    let rh_expr = get_seq_expr ctx rh_expr in
    bop_expr op lh_expr rh_expr, pos
  | Call(callee_expr, typs, args) -> begin
    match callee_expr with 
    | Typeof(expr, pos) -> (* typeof(x) () *)
      let expr = get_seq_expr ctx expr in
      let typ = get_type expr ctx.base in
      construct_expr typ (List.map args ~f:(get_seq_expr ctx)), pos
    | Id("array", pos) -> begin
      if List.length typs <> 1 then
        seq_error "Type of array is required" pos;
      if List.length args <> 1 then
        seq_error "Size of array is required" pos;
      match parse_type ctx @@ List.hd_exn typs with
      | None ->
        seq_error (sprintf "Type %s is not declared" @@ prn_typ (fun x -> "") (List.hd_exn typs)) pos;
      | Some typ ->
        let sz = List.hd_exn args in
        array_expr typ (get_seq_expr ctx sz), pos
      end
    | Id(typ, pos) when has_type ctx typ -> (* class[int]() *)
      let typs = List.map typs ~f:(fun t ->
        match parse_type ctx t with
        | None -> 
          seq_error (sprintf "Type %s is not declared" @@ prn_typ (fun x -> "") t) pos;
        | Some t -> t) 
      in
      let typ = match parse_type ctx @@ TClass((typ, pos), []) with
        | Some typ -> realize_type typ typs 
        | None -> seq_error (sprintf "Type %s is not declared" typ) pos
      in
      construct_expr typ (List.map args ~f:(get_seq_expr ctx)), pos
    | _ -> (* fn[t](...) *)
      let callee_expr = get_seq_expr ctx callee_expr in
      let callee_expr = 
        if List.length typs > 0 then 
          let fn = realize_func callee_expr @@ List.map typs ~f:(fun t ->
            match parse_type ctx t with
            | None -> 
              seq_error (sprintf "Type %s is not declared" @@ prn_typ (fun x -> "") t) (get_pos callee_expr);
            | Some t -> t) 
          in 
          func_expr fn
        else callee_expr 
      in
      let args_exprs = List.map args ~f:(fun x ->
        match x with 
        | Ellipsis -> Ctypes.null
        | _ -> get_seq_expr ctx x) 
      in
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
    match lh_expr with
    | Id(typ, pos) when has_type ctx typ -> 
      let typ = match parse_type ctx @@ TClass((typ, pos), []) with
        | None -> 
          seq_error (sprintf "Type %s is not declared" typ) pos;
        | Some t -> t 
      in
      static_expr typ rhs, pos
    | _ -> 
      get_elem_expr (get_seq_expr ctx lh_expr) rhs, pos
    end
  | Index(lh_expr, index_expr) ->
    let lh_expr = get_seq_expr ctx lh_expr in begin
    match index_expr with
    | Slice(st, ed, step, pos) ->
      if is_some step then noimp "Step";
      let st = match st with None -> Ctypes.null | Some st -> get_seq_expr ctx st in
      let ed = match ed with None -> Ctypes.null | Some ed -> get_seq_expr ctx ed in
      array_slice_expr lh_expr st ed, pos
    | _ -> 
      let index_expr = get_seq_expr ctx index_expr in
      let pos = get_pos index_expr in
      array_lookup_expr lh_expr index_expr, pos
    end
  | Tuple(args, pos) ->
    record_expr (List.map args ~f:(get_seq_expr ctx)), pos
  | Typeof(expr, pos) ->
    seq_error "Invalid typeof" pos
  | _ -> 
    noimp "Unknown expr"
  end
  in 
  set_pos expr pos;
  expr
and parse_type ctx = function
  | TGeneric(typ, pos) -> begin
    match Hashtbl.find ctx.map typ with 
    | Some (Type t) -> Some t
    | _ -> None
    end
  | TClass((typ, pos), param_typs) -> begin
    match Hashtbl.find ctx.map typ with 
    | Some (Type t) -> Some (realize_type t @@ List.map param_typs ~f:(fun t ->
      match parse_type ctx t with
      | None -> 
        seq_error (sprintf "Type %s is not declared" @@ prn_typ (fun x -> "") t) pos;
      | Some t -> t))
    | _ -> None
    end
  | TTypeof(expr) ->
    let expr = get_seq_expr ctx expr in
    Some (get_type expr ctx.base)

let match_arg = function
  | Arg((n, _), None) -> (n, (TGeneric("`", dummy_pos)))
  | Arg((n, _), Some t) -> (n, t)
  
let set_generics ctx pos types args set_generics get_generic =
  let generic_types = String.Table.create () in
  let arg_names, arg_types = List.unzip @@ List.map args ~f:(function 
    | Arg((n, _), None) -> (n, None)
    | Arg((n, _), Some t) -> (n, Some(t))) 
  in 
  List.iteri types ~f:(fun i -> function 
    | TGeneric(t, _) -> Hashtbl.set generic_types ~key:t ~data:i
    | _ -> seq_error "Generic needed (forgot `?)" pos);
  let arg_types, generic_count = 
    List.fold arg_types ~init:([], 0) ~f:(fun (acc_types, acc_count) -> function
      | None -> 
        let t = (sprintf "``%d" acc_count) in (* in-code genrics can only have one quote *)
        Hashtbl.set generic_types ~key:t ~data:acc_count;
        TGeneric(t, dummy_pos)::acc_types, acc_count + 1
      | Some (TGeneric (gen, _) as t) -> begin
        match Hashtbl.find generic_types gen with 
        | Some _ -> t::acc_types, acc_count
        | None -> 
          Hashtbl.set generic_types ~key:gen ~data:acc_count;
          t::acc_types, acc_count + 1
        end
      | Some t -> begin 
        match parse_type ctx t with
        | Some _ -> t::acc_types, acc_count
        | None -> noimp (sprintf "Type %s" @@ prn_typ (fun x -> "") t)
        end)
  in
  let generic_count = generic_count + (List.length types) in
  eprintf "> %d\n%!" generic_count;
  set_generics generic_count;
  let arg_types = List.map (List.rev arg_types) ~f:(function 
    | TGeneric(gen, _) -> 
      let idx = Hashtbl.find_exn generic_types gen in
      (* set_ref_generic_name typ idx t;  *)
      get_generic idx gen
    | _ as t -> Option.value_exn (parse_type ctx t))
  in
  arg_names, arg_types

let rec get_seq_stmt ctx block stmt : unit = 
  let stmt, pos = begin 
  match stmt with
  | Pass pos -> 
    pass_stmt (), pos
  | Break pos -> 
    break_stmt (), pos
  | Continue pos -> 
    continue_stmt (), pos
  | Statements stmts ->
    List.iter stmts ~f:(get_seq_stmt ctx block);
    pass_stmt (), dummy_pos
  | Assign(lh_expr, rh_expr) -> (* a = b *)
    (* fprintf stderr ">> %s\n%!" @@ prn_expr (fun x->sprintf "<<%d>> " x.pos_lnum) lh_expr; *)
    let rh_expr = get_seq_expr ctx rh_expr in 
    let rh_type = get_type rh_expr ctx.base in 
    begin
    match lh_expr with
    | Id(var, pos) -> begin
      match Hashtbl.find ctx.map var with
      | Some v -> (match v with 
        | Var v ->
          let lh_type = get_var_type v in
          if lh_type <> rh_type then begin
          (* if false then begin *)
            T.eprintf [T.black; T.on_yellow] "[WARN] shadowing variable %s\n" var;
            let var_stmt = var_stmt rh_expr in
            Hashtbl.set ctx.map ~key:var ~data:(Var (var_stmt_var var_stmt));
            Stack.push ctx.stack (var::Stack.pop_exn ctx.stack);
            var_stmt, pos
          end else 
            assign_stmt v rh_expr, pos
        | Func v -> 
          assign_stmt v rh_expr, pos
        | Type _ -> 
          noimp "Type assignment (should it be?)")
      | None -> 
        let var_stmt = var_stmt rh_expr in
        Hashtbl.set ctx.map ~key:var ~data:(Var (var_stmt_var var_stmt));
        Stack.push ctx.stack (var::Stack.pop_exn ctx.stack);
        var_stmt, pos
      end
    | Dot(lh_expr, (rhs, pos)) ->
      assign_member_stmt (get_seq_expr ctx lh_expr) rhs rh_expr, pos
    | Index(var_expr, index_expr) -> 
      let index_expr = get_seq_expr ctx index_expr in
      let pos = get_pos index_expr in
      assign_index_stmt (get_seq_expr ctx var_expr) index_expr rh_expr, pos
    | _ -> 
      seq_error "Assignment requires Id / Dot / Index on LHS" (get_pos rh_expr)
    end
  | Exprs expr ->
    let expr = get_seq_expr ctx expr in
    expr_stmt expr, (get_pos expr)
  | Print (print_exprs, pos) ->
    (* TODO: fix pos arguments *)
    List.iteri print_exprs ~f:(fun i ps -> 
      if i > 0 then begin
        let stmt = print_stmt @@ get_seq_expr ctx @@ String(" ", pos) in
        set_base stmt ctx.base;
        set_pos stmt pos;
        add_stmt stmt block
      end;
      let stmt = print_stmt @@ get_seq_expr ctx ps in
      set_base stmt ctx.base;
      set_pos stmt pos;
      add_stmt stmt block
    );
    print_stmt (get_seq_expr ctx @@ String("\n", pos)), pos
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
      | Arg((n, p), None) -> seq_error "Type with generic argument" p
      | Arg((n, _), Some t) -> (n, t)
    ) in
    if is_some (Hashtbl.find ctx.map name) then
      raise (SeqCamlError (sprintf "Type %s already defined" name, pos));
    let typ = record_type arg_names (List.map arg_types ~f:(fun t ->
      match parse_type ctx t with
      | None -> 
        seq_error (sprintf "Type %s is not declared" @@ prn_typ (fun x -> "") t) pos;
      | Some t -> t)) 
    in
    Hashtbl.set ctx.map ~key:name ~data:(Type typ);
    pass_stmt (), pos
  | If(if_blocks) -> 
    let if_stmt = if_stmt () in
    let positions = List.map if_blocks ~f:(fun (cond_expr, stmts, pos) ->
      let if_block = match cond_expr with 
      | None -> 
        get_else_block if_stmt
      | Some cond_expr -> 
        get_elif_block if_stmt @@ get_seq_expr ctx cond_expr
      in
      Stack.push ctx.stack [];
      List.iter stmts ~f:(get_seq_stmt ctx if_block);
      Stack.pop_exn ctx.stack |> List.iter ~f:(Hashtbl.remove ctx.map);
      pos)
    in
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
    | _ -> 
      noimp "For non-ID variable"
    in
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
        pat, "", None
      in
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
    let ref_ctx_map = Hashtbl.copy ctx.map in
    let arg_names, arg_types = 
      set_generics ctx pos types args (set_ref_generics typ) (fun idx name -> 
        set_ref_generic_name typ idx name;
        let t = get_ref_generic typ idx in
        Hashtbl.set ref_ctx_map ~key:name ~data:(Type t);
        t) 
    in
    Hashtbl.set ctx.map ~key:class_name ~data:(Type typ);
    Hashtbl.set ref_ctx_map ~key:class_name ~data:(Type typ);
    set_ref_record typ @@ record_type arg_names arg_types;

    (* functions inherit types and functions; variables are off-limits *)
    List.iter functions ~f:(fun f -> 
      let name, fn = get_seq_fn {ctx with map=ref_ctx_map} f ~parent_class:class_name in 
      add_ref_method typ name fn);
    set_ref_done typ;
    pass_stmt (), pos
  | _ -> 
    noimp "Unknown stmt"
  end
  in 
  set_base stmt ctx.base;
  set_pos stmt pos;
  add_stmt stmt block

and get_seq_fn ctx ?parent_class = function 
  | Function(return_typ, types, args, stmts, pos) ->
    let fn_name, _ = match_arg return_typ in
    if is_some @@ Hashtbl.find ctx.map fn_name then 
      seq_error (sprintf "Cannot define function %s as the variable with same name exists" fn_name) pos;
    
    let fn = func fn_name in
    (* add it to the table only if it is "pure" function *)
    if is_none parent_class then 
      Hashtbl.set ctx.map ~key:fn_name ~data:(Func fn);

    (* handle statements *)
    let fn_ctx = {(init_context fn) with map=(Hashtbl.copy ctx.map)} in
    let arg_names, arg_types = 
      set_generics ctx pos types args (set_func_generics fn) (fun idx name -> 
        set_func_generic_name fn idx name;
        let t = get_func_generic fn idx in
        Hashtbl.set fn_ctx.map ~key:name ~data:(Type t);
        t) 
    in
    set_func_params fn arg_names arg_types;

    List.iter arg_names ~f:(fun arg_name -> 
      Hashtbl.set fn_ctx.map ~key:arg_name ~data:(Var (get_func_arg fn arg_name)));
    Stack.push fn_ctx.stack arg_names;
    
    let fn_block = get_func_block fn in
    List.iter stmts ~f:(get_seq_stmt fn_ctx fn_block);
    (fn_name, fn)
  | _ -> 
    seq_error "get_seq_func MUST HAVE Function as an input" dummy_pos

and get_seq_case_pattern ctx = function
  (*  condition, guard, statements *)
  | None -> wildcard_pattern ()
  | Some (Int (i, _)) -> int_pattern i
  | Some (String (s, _)) -> str_pattern s
  | Some (Bool (b, _)) -> bool_pattern b
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
  let lines = Array.of_list lines in
  let lexbuf = Lexing.from_string code in
  let state = Lexer.stack_create () in
  let print_error kind ?msg (pos: pos_t) = 
    let line, col = pos.pos_lnum, pos.pos_cnum - pos.pos_bol in
    let style = [T.Bold; T.red] in
    eprintf "%s%!" @@ T.sprintf style "[ERROR] %s error: line %d%s\n" kind line (match msg with 
      | Some m -> sprintf " with %s" m | None -> "");
    eprintf "%s%!" @@ T.sprintf style "[ERROR] %3d: %s" line (String.prefix lines.(line - 1) col);
    eprintf "%s%!" @@ T.sprintf [T.Bold; T.white; T.on_red] "%s\n%!" (String.drop_prefix lines.(line - 1) col);
  in
  try
    let ast = Parser.program (Lexer.token state) lexbuf in  
    let prn_pos (pos: pos_t) = "" in
    eprintf "%s%!" @@ T.sprintf [T.Bold; T.green] "|> AST ==> \n";
    eprintf "%s%!" @@ T.sprintf [T.green] "%s\n%!" @@ Ast.prn_ast prn_pos ast;
    eprintf "%s%!" @@ T.sprintf [T.Bold] "|> Output ==>\n%!";
    seq_exec ast
  with 
  | Lexer.SyntaxError (msg, pos) ->
    print_error "Lexer" pos ~msg:msg
  | Parser.Error ->
    (* check https://github.com/dbp/funtal/blob/e9a9b9d/parse.ml#L64-L89 *)
    print_error "Parser" lexbuf.lex_start_p
  | SeqCamlError (msg, pos) ->
    print_error "CamlAst" pos ~msg:msg
  | SeqCError (msg, pos) ->
    print_error "Compiler" pos ~msg:msg
  