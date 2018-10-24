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
  ref_class: seq_type option;
  filename: string;
  base: seq_func;
  mdl: seq_func;

  map: (string, assignable list) Hashtbl.t;
  stack: ((string) Hash_set.t) Stack.t
}

let context_add_var ctx key var =
  begin match Hashtbl.find ctx.map key with 
  | None -> Hashtbl.set ctx.map ~key ~data:[var]
  | Some lst -> Hashtbl.set ctx.map ~key ~data:(var::lst)
  end;
  Hash_set.add (Stack.top_exn ctx.stack) key
let context_clear_block ctx =
  Hash_set.iter (Stack.pop_exn ctx.stack) ~f:(fun key ->
    let items = Hashtbl.find_exn ctx.map key in
    if List.length items = 1 then
      Hashtbl.remove ctx.map key
    else
      Hashtbl.set ctx.map ~key ~data:(List.tl_exn items))
let context_in_block ctx key =
  if Stack.length ctx.stack = 0 then None
  else if Hash_set.exists (Stack.top_exn ctx.stack) ~f:((=)key) then 
    Some (List.hd_exn @@ Hashtbl.find_exn ctx.map key)
  else None

  
let dummy_pos: pos_t = {pos_fname=""; pos_cnum=0; pos_lnum=0; pos_bol=0}

let init_context fn mdl filename =
  let ctx = {base = fn; 
             ref_class = None; 
             mdl; 
             map = String.Table.create (); 
             stack = Stack.create (); 
             prefix = ""; 
             filename} in
  (* initialize POD types *)
  Hashtbl.set ctx.map ~key:"void"  ~data:([Type (void_type ())]);
  Hashtbl.set ctx.map ~key:"int"   ~data:([Type (int_type ())]);
  Hashtbl.set ctx.map ~key:"str"   ~data:([Type (str_type ())]);
  Hashtbl.set ctx.map ~key:"seq"   ~data:([Type (str_seq_type ())]);
  Hashtbl.set ctx.map ~key:"bool"  ~data:([Type (bool_type ())]);
  Hashtbl.set ctx.map ~key:"float" ~data:([Type (float_type ())]);
  Hashtbl.set ctx.map ~key:"file"  ~data:([Type (file_type ())]);
  Hashtbl.set ctx.map ~key:"byte"  ~data:([Type (byte_type ())]);
  ctx

(* placeholder for NotImplemented *)
let noimp s =
  raise (NotImplentedError ("Not yet implemented: " ^ s))
let seq_error msg pos =
  raise (SeqCamlError (msg, pos))

let rec get_seq_expr ctx expr =
  let get_seq_expr = get_seq_expr ctx in
  let expr, pos = begin
  match expr with
  | `Ellipsis _      -> noimp "random ellipsis"
  | `Slice _         -> noimp "random slice"
  | `Dict _          -> noimp "random dict"
  | `List _          -> noimp "random list"

  | `Bool(b, pos)    -> bool_expr b, pos
  | `Int(i, pos)     -> int_expr i, pos
  | `Float(f, pos)   -> float_expr f, pos
  | `String(s, pos)  -> str_expr s, pos
  | `Seq(s, pos)     -> str_seq_expr s, pos
  | `Generic(var, pos)
  | `Id(var, pos)    -> begin
    match Hashtbl.find ctx.map var with
    | Some (Var v::_)  -> var_expr v, pos
    | Some (Func f::_) -> func_expr f, pos
    | Some (Type t::_) -> type_expr t, pos
    | _ -> seq_error (sprintf "%s not found" var) pos
    end
  | `TypeOf(_, _) ->
    noimp "TypeOf"
    (* let expr = get_seq_expr expr in
    let typ = get_type expr ctx.base in
    type_expr typ, pos *)
  | `IfExpr(cond, if_expr, else_expr, pos) ->
    let if_expr = get_seq_expr if_expr in 
    let else_expr = get_seq_expr else_expr in
    let c_expr = get_seq_expr cond in 
    cond_expr c_expr if_expr else_expr, pos
  | `Unary((op, pos), expr) -> 
    uop_expr op (get_seq_expr expr), pos
  | `Binary(lh_expr, (op, pos), rh_expr) ->
    let lh_expr = get_seq_expr lh_expr in
    let rh_expr = get_seq_expr rh_expr in
    bop_expr op lh_expr rh_expr, pos
  | `Call(callee_expr, args, pos) -> begin
    let callee_expr = get_seq_expr callee_expr in
    match get_type callee_expr with
    | Some typ ->
      construct_expr typ (List.map args ~f:get_seq_expr), get_pos callee_expr
    | None -> (* fn[t](...) *)
      let args_exprs = List.map args ~f:(function
        | `Ellipsis(_) -> Ctypes.null
        | ex -> get_seq_expr ex) in
      if List.exists args_exprs ~f:((=)(Ctypes.null)) then   
        partial_expr callee_expr args_exprs, pos
      else
        call_expr callee_expr args_exprs, pos
    end
  | `Pipe(exprs, pos) ->
    let exprs = List.map exprs ~f:get_seq_expr in
    pipe_expr exprs, pos
  | `Dot(lh_expr, (rhs, _), pos) -> begin
    let lh_expr = get_seq_expr lh_expr in
    match get_type lh_expr with
    | Some typ -> static_expr typ rhs, pos
    | None -> get_elem_expr lh_expr rhs, pos
    end
  | `Index(lh_expr, [`Slice(st, ed, step, _)], pos) ->
    let lh_expr = get_seq_expr lh_expr in 
    if is_some step then noimp "Step";
    let unpack st = Option.value_map st ~f:get_seq_expr ~default:Ctypes.null in
    array_slice_expr lh_expr (unpack st) (unpack ed), pos
  | `Index(`Id("array", _), indices, pos) -> 
    if List.length indices <> 1 then 
      seq_error "Array needs only one type" pos;
    let typ = get_type_from_expr_exn ctx (List.hd_exn indices) in
    type_expr (array_type typ), pos
  | `Index(`Id("ptr", _), indices, pos) -> 
    if List.length indices <> 1 then 
      seq_error "Pointer needs only one type" pos;
    let typ = get_type_from_expr_exn ctx (List.hd_exn indices) in
    type_expr (ptr_type typ), pos
  | `Index(`Id("callable", _), indices, pos) -> 
    let typ_exprs = List.map indices ~f:(get_type_from_expr_exn ctx) in
    let ret, args = match List.rev typ_exprs with
    | hd::tl -> hd, tl |> List.rev
    | [] -> seq_error "Callable needs at least one argument" pos in
    type_expr (func_type ret args), pos
  | `Index(`Id("yieldable", _), indices, pos) -> 
    if List.length indices <> 1 then 
      seq_error "Yieldable needs only one type" pos;
    let typ = get_type_from_expr_exn ctx (List.hd_exn indices) in
    type_expr (gen_type typ), pos
  | `Index(lh_expr, indices, pos) -> begin
    let lh_expr = get_seq_expr lh_expr in
    let index_exprs = List.map indices ~f:get_seq_expr in
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
  | `Tuple(args, pos) ->
    record_expr (List.map args ~f:get_seq_expr), pos
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
    | `Arg((n, pos), None) -> (n, `Generic(sprintf "``%s" n, pos))
    | `Arg((n, _), Some t) -> (n, t)) in 
  
  let generics = List.append types arg_types  
    |> List.filter_map ~f:(function `Generic(g, _) -> Some g | _ -> None) 
    |> List.dedup_and_sort ~compare in
  set_generic_count (List.length generics);
  
  let arg_types = List.map arg_types ~f:(get_type_from_expr_exn ctx) in
  arg_names, arg_types, List.mapi generics ~f:(fun cnt key -> key, Type (get_generic cnt key))


type extended_statement =
  [ statement
  | `AssignExpr of expr * seq_expr * bool * pos_t ]


let rec get_seq_stmt ctx block parsemod (stmt: extended_statement) = 
  let get_seq_stmt ?bl ?ct s = 
    get_seq_stmt (Option.value ct ~default:ctx) 
                 (Option.value bl ~default:block) 
                 parsemod 
                 (s :> extended_statement) in 
  let finalize_stmt stmt pos = 
    set_base stmt ctx.base;
    set_pos stmt pos;
    add_stmt stmt block;
    stmt in
  let add_block ?ct ?init bl stmts = 
    let ctx = Option.value ct ~default:ctx in
    Stack.push ctx.stack (String.Hash_set.create ());
    (Option.value init ~default:(fun _ -> ())) ctx;
    List.iter stmts ~f:(get_seq_stmt ~bl:bl);
    context_clear_block ctx in
  let stmt, pos = begin 
  match stmt with
  | `AssignExpr(`Id(var, _), rh_expr, shadow, pos) -> begin
    match Hashtbl.find ctx.map var with
    | Some (Var _::_) when shadow ->
      let var_stmt = var_stmt rh_expr in
      context_add_var ctx var (Var (var_stmt_var var_stmt));
      var_stmt, pos
    | Some (Var v::_) ->
      assign_stmt v rh_expr, pos
    | Some (Func v::_) ->
      assign_stmt v rh_expr, pos
    | Some (Type _::_) ->
      noimp "Type assignment (should it be?)"
    | _ ->
      let var_stmt = var_stmt rh_expr in
      context_add_var ctx var (Var (var_stmt_var var_stmt));
      var_stmt, pos
    end
  | `AssignExpr(`Dot(lh_expr, (rhs, _), _), rh_expr, _, pos) -> (* a.x = b *)
    assign_member_stmt (get_seq_expr ctx lh_expr) rhs rh_expr, pos
  | `AssignExpr(`Index(var_expr, [index_expr], _), rh_expr, _, pos) -> (* a[x] = b *)
    let index_expr = get_seq_expr ctx index_expr in
    assign_index_stmt (get_seq_expr ctx var_expr) index_expr rh_expr, pos
  | `AssignExpr(_, _, _, pos) ->
    seq_error "Assignment requires Id / Dot / Index on LHS" pos
  | `Pass pos     -> pass_stmt (), pos
  | `Break pos    -> break_stmt (), pos
  | `Continue pos -> continue_stmt (), pos
  | `Statements stmts ->
    List.iter stmts ~f:get_seq_stmt;
    pass_stmt (), dummy_pos
  | `Assign([lhs], [rhs], shadow, pos) ->
    let rh_expr = get_seq_expr ctx rhs in 
    let stmt = `AssignExpr(lhs, rh_expr, shadow, pos) in
    get_seq_stmt stmt;
    pass_stmt (), pos
  | `Assign(lh, rh, shadow, pos) ->
    if List.length lh <> List.length rh then
      seq_error "RHS must match LHS" pos;      
    let var_stmts = List.map rh ~f:(fun rhs ->
      let rh_expr = get_seq_expr ctx rhs in
      finalize_stmt (var_stmt rh_expr) pos) in
    List.iter (List.zip_exn lh var_stmts) ~f:(fun (lhs, var_stmt) ->
      let rh_expr = var_expr (var_stmt_var var_stmt) in
      let stmt = `AssignExpr(lhs, rh_expr, shadow, pos) in
      get_seq_stmt stmt);
    pass_stmt (), pos
  | `Exprs expr ->
    let expr = get_seq_expr ctx expr in
    expr_stmt expr, (get_pos expr)
  | `Print (print_exprs, pos) ->
    List.iteri print_exprs ~f:(fun i ps ->
      if i > 0 then begin
        let stmt = `String(" ", pos) |> get_seq_expr ctx |> print_stmt in
        ignore @@ finalize_stmt stmt pos
      end;
      let stmt = ps |> get_seq_expr ctx |> print_stmt in
      ignore @@ finalize_stmt stmt pos);
    `String("\n", pos) |> get_seq_expr ctx |> print_stmt, pos
  | `Return (ret_expr, pos) ->
    let ret_stmt = return_stmt (get_seq_expr ctx ret_expr) in
    set_func_return ctx.base ret_stmt;
    ret_stmt, pos
  | `Yield (yield_expr, pos) ->
    let yield_stmt = yield_stmt (get_seq_expr ctx yield_expr) in
    set_func_yield ctx.base yield_stmt;
    yield_stmt, pos
  | `Type((name, pos), args, _) ->
    let arg_names, arg_types = List.unzip @@ List.map args ~f:(function
      | `Arg((_, p), None) -> seq_error "Type with generic argument" p
      | `Arg((n, _), Some t) -> (n, t)) in
    if is_some @@ context_in_block ctx name then
      raise (SeqCamlError (sprintf "Type %s already defined" name, pos));
    let typ = record_type arg_names @@ List.map arg_types ~f:(get_type_from_expr_exn ctx) in
    context_add_var ctx name (Type typ);
    pass_stmt (), pos
  | `If(if_blocks, pos) -> 
    let if_stmt = if_stmt () in
    List.iter if_blocks ~f:(fun (cond_expr, stmts, _) ->
      let if_block = match cond_expr with 
      | None -> 
        get_else_block if_stmt
      | Some cond_expr ->
        get_elif_block if_stmt @@ get_seq_expr ctx cond_expr in
      add_block if_block stmts);
    if_stmt, pos
  | `While(cond_expr, stmts, pos) ->
    let cond_expr = get_seq_expr ctx cond_expr in
    let while_stmt = while_stmt(cond_expr) in
    let while_block = get_while_block while_stmt in
    add_block while_block stmts;
    while_stmt, pos
  | `For(for_var, gen_expr, stmts, pos) ->
    let gen_expr = get_seq_expr ctx gen_expr in
    let for_stmt = for_stmt gen_expr in

    let for_var_name, for_var = match for_var with
    | `Id (for_var_name, _) -> 
      (for_var_name, get_for_var for_stmt)
    | _ -> noimp "For non-ID variable" in

    let for_block = get_for_block for_stmt in
    add_block for_block stmts ~init:(fun ctx -> 
      context_add_var ctx for_var_name (Var for_var));
    for_stmt, pos
  | `Match(what_expr, cases, pos) ->
    let rec match_pattern ctx = function 
      | `IntPattern i -> int_pattern i
      | `BoolPattern b -> bool_pattern b
      | `StrPattern s -> str_pattern s
      | `SeqPattern s -> str_seq_pattern s
      | `RangePattern(i, j) -> range_pattern i j
      | `StarPattern -> star_pattern ()
      | `WildcardPattern None -> wildcard_pattern ()
      | `WildcardPattern (Some (id, _)) ->
        let pat = wildcard_pattern () in
        let var = get_bound_pattern_var pat in
        context_add_var ctx id (Var var);
        pat
      | `TuplePattern tl -> 
        record_pattern @@ List.map tl ~f:(match_pattern ctx)
      | `ListPattern tl -> 
        array_pattern @@ List.map tl ~f:(match_pattern ctx)
      | `OrPattern tl -> 
        or_pattern  @@ List.map tl ~f:(match_pattern ctx)
      | `GuardedPattern(pat, expr) -> 
        guarded_pattern (match_pattern ctx pat) (get_seq_expr ctx expr)
      | `BoundPattern ((_, pos), _) -> 
        seq_error "Invalid bound pattern" pos
    in
    let match_stmt = match_stmt (get_seq_expr ctx what_expr) in
    List.iter cases ~f:(fun (pattern, stmts, _) -> 
      let pat, var = match pattern with 
      | `BoundPattern((name, _), pat) -> 
        Stack.push ctx.stack (String.Hash_set.create ());
        let pat = bound_pattern (match_pattern ctx pat) in
        context_clear_block ctx;
        pat, Some(name, get_bound_pattern_var pat)
      | _ as p -> 
        Stack.push ctx.stack (String.Hash_set.create ());
        let pat = match_pattern ctx p in
        context_clear_block ctx;
        pat, None
      in
      let case_block = add_match_case match_stmt pat in
      add_block case_block stmts ~init:(fun ctx ->
        match var with Some(n, v) -> context_add_var ctx n (Var v) | None -> ()));
    match_stmt, pos
   | `Function(return_typ, types, args, stmts, pos) ->
    let fn_name, ret_typ = match return_typ with `Arg((n, _), typ) -> n, typ in

    if is_some @@ context_in_block ctx fn_name then 
      seq_error (sprintf "Cannot define function %s as the variable with same name exists" fn_name) pos;
    let fn = func fn_name in    
    begin match ctx.ref_class with 
    | Some(typ) -> add_ref_method typ fn_name fn
    | None -> context_add_var ctx fn_name (Func fn)
    end;
    if is_some ret_typ then begin
      let typ = get_type_from_expr_exn ctx (Option.value_exn ret_typ) in
      set_func_out fn typ
    end;
    (* handle statements *)
    let fn_ctx = {(init_context fn ctx.mdl ctx.filename) with 
                  map = Hashtbl.map ctx.map ~f:(List.filter ~f:(fun v ->
                    match v with Func _ | Type _ -> true | _ -> false))} in
    let arg_names, arg_types, generics = set_generics fn_ctx types args 
      (set_func_generics fn) 
      (fun idx name -> 
        set_func_generic_name fn idx name;
        get_func_generic fn idx) in
    set_func_params fn arg_names arg_types;

    let fn_block = get_func_block fn in
    add_block fn_block stmts ~ct:fn_ctx ~init:(fun ctx ->
      List.iter arg_names ~f:(fun arg_name -> 
        context_add_var ctx arg_name (Var (get_func_arg fn arg_name)));
      List.iter generics ~f:(fun (key, data) -> 
        context_add_var ctx key data));
    func_stmt fn, pos
  | `Extern("c", _, ret, args, pos) ->
    let fn_name, ret_typ = match ret with 
    | `Arg((n, _), Some typ) -> n, typ 
    | _ -> seq_error "Return type must be defined for extern functions" pos in
    if is_some @@ context_in_block ctx fn_name then 
      seq_error (sprintf "Cannot define function %s as the variable with same name exists" fn_name) pos;

    let fn = func fn_name in
    context_add_var ctx fn_name (Func fn);
    let typ = get_type_from_expr_exn ctx ret_typ in
    set_func_out fn typ;

    let arg_names, arg_types = List.unzip @@ List.map args ~f:(function 
      | `Arg((n, _), Some t) -> (n, get_type_from_expr_exn ctx t) 
      | _ -> seq_error "Argument type must be defined for extern functions" pos) in 
    set_func_params fn arg_names arg_types;
    set_func_extern fn;
    func_stmt fn, pos
  | `Extern(_, _, _, _, _) -> noimp "Non-c externs"
  | `Class((class_name, name_pos), types, args, functions, pos) ->
    if is_some @@ context_in_block ctx class_name then 
      seq_error (sprintf "Cannot define class %s as the variable with same name exists" class_name) name_pos;

    let typ = ref_type class_name in
    context_add_var ctx class_name (Type typ);
    let ref_ctx = {ctx with 
                   map = Hashtbl.copy ctx.map;
                   ref_class = Some(typ)} in
    let arg_names, arg_types, generics = set_generics ref_ctx types args 
      (set_ref_generics typ) 
      (fun idx name -> 
        set_ref_generic_name typ idx name;
        get_ref_generic typ idx) in
    set_ref_record typ @@ record_type arg_names arg_types;

    Stack.push ctx.stack (String.Hash_set.create ());
    List.iter generics ~f:(fun (key, data) -> 
        context_add_var ctx key data);
    List.iter functions ~f:(fun f -> 
      ignore @@ get_seq_stmt ~ct:ref_ctx (f :> extended_statement));
    context_clear_block ctx;
    set_ref_done typ;
    pass_stmt (), pos
  | `Extend((class_name, _), functions, pos) ->
    let typ = match context_in_block ctx class_name with
    | Some (Type t) -> t
    | _ -> seq_error (sprintf "Cannot extend non-existing class %s" class_name) pos in
    let ref_ctx = {ctx with 
                   map = Hashtbl.copy ctx.map; 
                   ref_class = Some(typ)} in
    List.iter functions ~f:(fun f -> 
      ignore @@ get_seq_stmt ~ct:ref_ctx (f :> extended_statement));
    pass_stmt (), pos
  | `Import(il, pos) ->
    noimp "www"
    (* List.iter il ~f:(fun ((what, _), _) ->
      let import_ctx = parsemod ctx.mdl (sprintf "%s/%s.seq" (Filename.dirname ctx.filename) what) in
      Hashtbl.iteri import_ctx.map ~f:(Hashtbl.set ctx.map)); *)
    (* pass_stmt (), pos *)
  end in
  ignore @@ finalize_stmt stmt pos
  
and parse_module ?execute ?print_ast mdl infile error_handler = 

  let preamble = [| sprintf  "__argv__ = array[str](%d)\n" (Array.length Sys.argv) |] in
  let preamble = Array.append preamble @@ Array.mapi Sys.argv ~f:(fun idx s ->
    sprintf "__argv[%d] = \"%s\"\n" idx s) in
  ignore preamble;
  (* Array.append preamble ("from base import *\n"); *)

  let lines = In_channel.read_lines infile in
  let code = (String.concat ~sep:"\n" lines) ^ "\n" in
  let lines = Array.of_list lines in
  let lexbuf = Lexing.from_string code in
  let state = Lexer.stack_create () in
  let onerror kind msg (pos: Lexing.position) =
    let line, col = pos.pos_lnum, pos.pos_cnum - pos.pos_bol in
    error_handler kind msg line col lines.(line - 1) in
  try
    let ctx = init_context mdl mdl (Filename.realpath infile) in
    Stack.push ctx.stack (String.Hash_set.create ());
    let module_block = get_module_block mdl in

    let ast = Parser.program (Lexer.token state) lexbuf in
    if is_some print_ast then (
      eprintf "%s%!" @@ T.sprintf [T.Bold; T.green] "|> AST of %s ==> \n" infile;
      eprintf "%s%!" @@ T.sprintf [T.green] "%s\n%!" @@ Ast.prn_ast ast);

    let parsemod mdl fl =
      parse_module mdl fl error_handler in
    match ast with Module stmts -> 
      List.iter stmts ~f:(fun x -> get_seq_stmt ctx module_block parsemod (x :> extended_statement));
    (match execute with
     | Some (true) -> exec_module ctx.mdl false;
     | _ -> ());
    ctx
  with
  | Lexer.SyntaxError(msg, pos) ->
    onerror "Lexer" msg pos
  | Parser.Error ->
    (* check https://github.com/dbp/funtal/blob/e9a9b9d/parse.ml#L64-L89 *)
    let pos = lexbuf.lex_start_p in
    onerror "Parser" "Parser error" pos
  | SeqCamlError(msg, pos) ->
    onerror "ASTGenerator" msg pos
  | SeqCError(msg, pos) ->
    onerror "SeqLib" msg pos

let parse fname =
  let error_handler _ _ _ _ _ =
    eprintf ">> OCaml exception died! <<\n%!";
    exit 1 in
  let seq_module = init_module () in
  ignore @@ parse_module seq_module fname error_handler ~execute:false;
  let adr = Ctypes.raw_address_of_ptr (Ctypes.to_voidp seq_module) in
  (* eprintf "[Ocaml] %nx\n%!" adr; *)
  adr

let () =
  let _ = Callback.register "parse_c" parse in

  if Array.length Sys.argv >= 2 then begin
    let seq_module = init_module () in
    let error_handler kind msg line col file_line =
      let style = [T.Bold; T.red] in
      eprintf "%s%!" @@ T.sprintf style "[ERROR] %s error: (line %d) %s\n" kind line msg;
      eprintf "%s%!" @@ T.sprintf style "[ERROR] %3d: %s" line (String.prefix file_line col);
      eprintf "%s%!" @@ T.sprintf [T.Bold; T.white; T.on_red] "%s" (String.drop_prefix file_line col);
      eprintf "%s%!" @@ T.sprintf [] "\n";
      exit 1
    in

    ignore @@ parse_module seq_module Sys.argv.(1) error_handler ~execute:true
  end

