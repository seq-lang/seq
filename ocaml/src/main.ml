(* 786 *)

open Core

open Ast
open Seqtypes
module T = ANSITerminal

type seq_error =
  | Lexer of string
  | Parser
  | Descent of string
  | Compiler of string

(* Error Type, Position*)
exception CompilerError of seq_error * pos_t
exception SeqCamlError of string * pos_t

(* context hashtable members *)

module Context = struct
  type assignable =
    | Var  of seq_var
    | Func of seq_func
    | Type of seq_type

  type t = {
    (* filename; TODO: remove *)
    filename: string;

    (* part of class definition? *)
    ref_class: seq_type option;

    (* module pointer *)
    mdl: seq_func;
    (* base function *)
    base: seq_func;
    (* block pointer *)
    block: seq_block;

    (* stack of blocks and variable hashsets *)
    stack: ((string) Hash_set.t) Stack.t;
    (* vtable *)
    map: (string, assignable list) Hashtbl.t;
  }

  let add_block ctx = 
    Stack.push ctx.stack (String.Hash_set.create ())

  let init filename mdl base block =
    let ctx = {filename;
               ref_class = None;
               mdl;
               base;
               block;
               stack = Stack.create ();
               map = String.Table.create ()} in
    add_block ctx;
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

  let add ctx key var =
    begin match Hashtbl.find ctx.map key with
    | None -> Hashtbl.set ctx.map ~key ~data:[var]
    | Some lst -> Hashtbl.set ctx.map ~key ~data:(var::lst)
    end;
    Hash_set.add (Stack.top_exn ctx.stack) key


  let clear_block ctx =
    Hash_set.iter (Stack.pop_exn ctx.stack) ~f:(fun key ->
      let items = Hashtbl.find_exn ctx.map key in
      if List.length items = 1 then
        Hashtbl.remove ctx.map key
      else
        Hashtbl.set ctx.map ~key ~data:(List.tl_exn items))

  let in_scope ctx key =
    match Hashtbl.find ctx.map key with
    | Some(hd::_) -> Some(hd)
    | _ -> None

  let in_block ctx key =
    if Stack.length ctx.stack = 0 then None
    else if Hash_set.exists (Stack.top_exn ctx.stack) ~f:((=)key) then
      Some (List.hd_exn @@ Hashtbl.find_exn ctx.map key)
    else None

  let dump ctx =
    eprintf "Filename: %s\n" ctx.filename;
    eprintf "Keys: \n";
    Hashtbl.iter_keys ctx.map ~f:(fun k -> eprintf "   %s\n" k);
    eprintf "Stack: \n";
    Stack.iter ctx.stack ~f:(fun k -> Hash_set.iter k ~f:(eprintf "%s, "); eprintf "\n");
    eprintf "%!"
end

(* placeholder for NotImplemented *)
let noimp s =
  raise (NotImplentedError ("not yet implemented: " ^ s))
let seq_error msg pos =
  raise (SeqCamlError (msg, pos))

let rec get_seq_expr (ctx: Context.t) expr =
  let get_seq_expr = get_seq_expr ctx in
  let expr, pos = begin
  match expr with
  | `Ellipsis _      -> noimp "random ellipsis"
  | `Slice _         -> noimp "random slice"

  | `None(pos)       -> none_expr (), pos
  | `Bool(b, pos)    -> bool_expr b, pos
  | `Int(i, pos)     -> int_expr i, pos
  | `Float(f, pos)   -> float_expr f, pos
  | `String(s, pos)  -> str_expr s, pos
  | `Seq(s, pos)     -> str_seq_expr s, pos
  | `Generic(var, pos)
  | `Id(var, pos)    -> begin
    match Hashtbl.find ctx.map var with
    | Some (Context.Var v::_)  -> var_expr v, pos
    | Some (Context.Func f::_) -> func_expr f, pos
    | Some (Context.Type t::_) -> type_expr t, pos
    | _ -> seq_error (sprintf "symbol '%s' not found" var) pos
    end
  | `List(args, pos) ->
    let typ = match Hashtbl.find ctx.map "list" with
    | Some ([Type t]) -> t
    | _ -> seq_error "list type not found" pos in
    list_expr typ (List.map args ~f:get_seq_expr), pos
  | `Dict(args, pos) ->
    let typ = match Hashtbl.find ctx.map "dict" with
    | Some ([Type t]) -> t
    | _ -> seq_error "dict type not found" pos in
    let args = List.fold ~init:[] ~f:(fun acc (x, y) -> y::x::acc) args
      |> List.rev in
    dict_expr typ (List.map args ~f:get_seq_expr), pos
  | `Set(args, pos) ->
    let typ = match Hashtbl.find ctx.map "set" with
    | Some ([Type t]) -> t
    | _ -> seq_error "set type not found" pos in
    set_expr typ (List.map args ~f:get_seq_expr), pos
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
  | `Binary(lh_expr, ("is", pos), rh_expr) ->
    let lh_expr = get_seq_expr lh_expr in
    let rh_expr = get_seq_expr rh_expr in
    is_expr lh_expr rh_expr, pos
  | `Binary(lh_expr, ("is not", pos), rh_expr) ->
    let lh_expr = get_seq_expr lh_expr in
    let rh_expr = get_seq_expr rh_expr in
    uop_expr "!" @@ is_expr lh_expr rh_expr, pos
  | `Binary(lh_expr, ("in", pos), rh_expr) ->
    let lh_expr = get_seq_expr lh_expr in
    let rh_expr = get_seq_expr rh_expr in
    array_contains_expr lh_expr rh_expr, pos
  | `Binary(lh_expr, ("not in", pos), rh_expr) ->
    let lh_expr = get_seq_expr lh_expr in
    let rh_expr = get_seq_expr rh_expr in
    uop_expr "!" @@ array_contains_expr lh_expr rh_expr, pos
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
    if is_some step then noimp "step";
    let unpack st = Option.value_map st ~f:get_seq_expr ~default:Ctypes.null in
    array_slice_expr lh_expr (unpack st) (unpack ed), pos
  | `Index(`Id("array", _), indices, pos) ->
    if List.length indices <> 1 then
      seq_error "Array needs only one type" pos;
    let typ = get_type_from_expr_exn ctx (List.hd_exn indices) in
    type_expr (array_type typ), pos
  | `Index(`Id("ptr", _), indices, pos) ->
    if List.length indices <> 1 then
      seq_error "pointer needs only one type" pos;
    let typ = get_type_from_expr_exn ctx (List.hd_exn indices) in
    type_expr (ptr_type typ), pos
  | `Index(`Id("callable", _), indices, pos) ->
    let typ_exprs = List.map indices ~f:(get_type_from_expr_exn ctx) in
    let ret, args = match List.rev typ_exprs with
    | hd::tl -> hd, tl |> List.rev
    | [] -> seq_error "callable needs at least one argument" pos in
    type_expr (func_type ret args), pos
  | `Index(`Id("generator", _), indices, pos) ->
    if List.length indices <> 1 then
      seq_error "generator needs only one type" pos;
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
        seq_error "index needs only one item" (get_pos lh_expr);
      array_lookup_expr lh_expr (List.hd_exn index_exprs), get_pos lh_expr
    end
  | `Tuple(args, pos) ->
    record_expr (List.map args ~f:get_seq_expr), pos
  end
  in
  set_pos expr pos;
  expr

and get_type_from_expr_exn ctx t =
  let typ_expr = get_seq_expr ctx t in
  match get_type typ_expr with
  | Some typ -> typ
  | _ -> seq_error "not a type" (get_pos typ_expr)

let set_generics ctx types args set_generic_count get_generic =
  let arg_names, arg_types = List.unzip @@ List.map args ~f:(function
    | `Arg((n, pos), None) -> (n, `Generic(sprintf "``%s" n, pos))
    | `Arg((n, _), Some t) -> (n, t)) in

  let type_args = List.map types ~f:(function
    | `Generic (g, _) -> g
    (* TODO fix position *)
    | _ -> seq_error "Type not a generic" Lexing.dummy_pos) in 
  let generic_args = List.filter_map arg_types ~f:(function
    | `Generic(g, _) when (String.is_prefix g ~prefix:"``") -> Some g
    | _ -> None) in
  let generics = List.append type_args generic_args
    |> List.dedup_and_sort ~compare in
  set_generic_count (List.length generics);

  List.iteri generics ~f:(fun cnt key ->
    Context.add ctx key (Context.Type (get_generic cnt key)));
  let arg_types = List.map arg_types ~f:(get_type_from_expr_exn ctx) in
  arg_names, arg_types

type extended_statement =
  [ statement
  | `AssignExpr of expr * seq_expr * bool * pos_t ]

let rec get_seq_stmt (ctx: Context.t) parsemod (stmt: extended_statement) =
  let get_seq_stmt ?ct s =
    get_seq_stmt (Option.value ct ~default:ctx)
                 parsemod
                 (s :> extended_statement) in
  let finalize_stmt stmt pos =
    set_base stmt ctx.base;
    set_pos stmt pos;
    add_stmt stmt ctx.block;
    stmt in
  let add_block ?ct ?init bl stmts =
    let ctx = Option.value ct ~default:ctx in
    Context.add_block ctx;
    (Option.value init ~default:(fun _ -> ())) ctx;
    List.iter stmts ~f:(get_seq_stmt ~ct:{ctx with block = bl});
    Context.clear_block ctx in
  let stmt, pos = begin
  match stmt with
  | `AssignExpr(`Id(var, _), rh_expr, shadow, pos) -> begin
    match Hashtbl.find ctx.map var with
    | Some (Context.Var _::_) when shadow ->
      let var_stmt = var_stmt rh_expr in
      Context.add ctx var (Context.Var (var_stmt_var var_stmt));
      var_stmt, pos
    | Some (Context.Var v::_) ->
      assign_stmt v rh_expr, pos
    | Some (Context.Func v::_) ->
      assign_stmt v rh_expr, pos
    | Some (Context.Type _::_) ->
      noimp "Type assignment (should it be?)"
    | _ ->
      let var_stmt = var_stmt rh_expr in
      Context.add ctx var (Context.Var (var_stmt_var var_stmt));
      var_stmt, pos
    end
  | `AssignExpr(`Dot(lh_expr, (rhs, _), _), rh_expr, _, pos) -> (* a.x = b *)
    assign_member_stmt (get_seq_expr ctx lh_expr) rhs rh_expr, pos
  | `AssignExpr(`Index(var_expr, [index_expr], _), rh_expr, _, pos) -> (* a[x] = b *)
    let index_expr = get_seq_expr ctx index_expr in
    assign_index_stmt (get_seq_expr ctx var_expr) index_expr rh_expr, pos
  | `AssignExpr(_, _, _, pos) ->
    seq_error "assignment requires Id / Dot / Index on LHS" pos
  | `Pass pos     -> pass_stmt (), pos
  | `Break pos    -> break_stmt (), pos
  | `Continue pos -> continue_stmt (), pos
  | `Statements stmts ->
    List.iter stmts ~f:get_seq_stmt;
    pass_stmt (), Lexing.dummy_pos
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
  | `Del ([`Index(lhs, [rhs], _)], pos) ->
    let lhs_expr = get_seq_expr ctx lhs in
    let rhs_expr = get_seq_expr ctx rhs in
    del_index_stmt lhs_expr rhs_expr, pos
  | `Del ([_], pos) ->
    seq_error "cannot del non-index expression" pos
  | `Del (exprs, pos) ->
    List.iter exprs ~f:(fun ex -> get_seq_stmt @@ `Del([ex], pos));
    pass_stmt (), pos
  | `Return (None, pos) ->
    return_stmt (Ctypes.null), pos
  | `Return (Some ret_expr, pos) ->
    let ret_stmt = return_stmt (get_seq_expr ctx ret_expr) in
    set_func_return ctx.base ret_stmt;
    ret_stmt, pos
  | `Yield (None, pos) ->
    yield_stmt (Ctypes.null), pos
  | `Yield (Some yield_expr, pos) ->
    let yield_stmt = yield_stmt (get_seq_expr ctx yield_expr) in
    set_func_yield ctx.base yield_stmt;
    yield_stmt, pos
  | `Type((name, pos), args, _) ->
    let arg_names, arg_types = List.unzip @@ List.map args ~f:(function
      | `Arg((_, p), None) -> seq_error "type with generic argument" p
      | `Arg((n, _), Some t) -> (n, t)) in

    if is_some @@ Context.in_scope ctx name then
      raise (SeqCamlError (sprintf "type %s already defined" name, pos));
    let typ = record_type arg_names @@ List.map arg_types ~f:(get_type_from_expr_exn ctx) in
    Context.add ctx name (Context.Type typ);
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
    | _ -> noimp "for non-ID variable" in

    let for_block = get_for_block for_stmt in
    add_block for_block stmts ~init:(fun ctx ->
      Context.add ctx for_var_name (Context.Var for_var));
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
        Context.add ctx id (Context.Var var);
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
        seq_error "invalid bound pattern" pos
    in
    let match_stmt = match_stmt (get_seq_expr ctx what_expr) in
    List.iter cases ~f:(fun (pattern, stmts, _) ->
      let pat, var = match pattern with
      | `BoundPattern((name, _), pat) ->
        Context.add_block ctx;
        let pat = bound_pattern (match_pattern ctx pat) in
        Context.clear_block ctx;
        pat, Some(name, get_bound_pattern_var pat)
      | _ as p ->
        Context.add_block ctx;
        let pat = match_pattern ctx p in
        Context.clear_block ctx;
        pat, None
      in
      let case_block = add_match_case match_stmt pat in
      add_block case_block stmts ~init:(fun ctx ->
        match var with Some(n, v) -> Context.add ctx n (Context.Var v) | None -> ()));
    match_stmt, pos
  | `Function(return_typ, types, args, stmts, pos) ->
    let fn_name, ret_typ = match return_typ with `Arg((n, _), typ) -> n, typ in

    if is_some @@ Context.in_block ctx fn_name then
      seq_error (sprintf "cannot define function %s as the variable with same name exists" fn_name) pos;
    let fn = func fn_name in
    begin match ctx.ref_class with
    | Some(typ) -> add_ref_method typ fn_name fn
    | None -> Context.add ctx fn_name (Context.Func fn)
    end;
    if is_some ret_typ then begin
      let typ = get_type_from_expr_exn ctx (Option.value_exn ret_typ) in
      set_func_out fn typ
    end;
    (* handle statements *)
    let fn_block = get_func_block fn in
    let fn_ctx = {(Context.init ctx.filename ctx.mdl fn fn_block) with
                  map = Hashtbl.map ctx.map ~f:(List.filter ~f:(fun v ->
                    match v with Context.Func _ | Context.Type _ -> true | _ -> false))} in
    let arg_names, arg_types = set_generics fn_ctx types args
      (set_func_generics fn)
      (fun idx name ->
        (* eprintf "generic for %s: %d %s\n"  fn_name idx name; *)
        set_func_generic_name fn idx name;
        get_func_generic fn idx) in
    (* eprintf "-- %s\n%!" fn_name; *)
    set_func_params fn arg_names arg_types;

    add_block fn_block stmts ~ct:fn_ctx ~init:(fun ctx ->
      List.iter arg_names ~f:(fun arg_name ->
        Context.add ctx arg_name (Context.Var (get_func_arg fn arg_name))));
    func_stmt fn, pos
  | `Extern("c", _, ret, args, pos) ->
    let fn_name, ret_typ = match ret with
    | `Arg((n, _), Some typ) -> n, typ
    | _ -> seq_error "return type must be defined for extern functions" pos in
    if is_some @@ Context.in_block ctx fn_name then
      seq_error (sprintf "cannot define function %s as the variable with same name exists" fn_name) pos;

    let fn = func fn_name in
    Context.add ctx fn_name (Context.Func fn);
    let typ = get_type_from_expr_exn ctx ret_typ in
    set_func_out fn typ;

    let arg_names, arg_types = List.unzip @@ List.map args ~f:(function
      | `Arg((n, _), Some t) -> (n, get_type_from_expr_exn ctx t)
      | _ -> seq_error "argument type must be defined for extern functions" pos) in
    set_func_params fn arg_names arg_types;
    set_func_extern fn;
    func_stmt fn, pos
  | `Extern(_, _, _, _, _) -> noimp "non-c externs"
  | `Class((class_name, name_pos), types, args, functions, pos) ->
    if is_some @@ Context.in_scope ctx class_name then
      seq_error (sprintf "cannot define class %s as the variable with same name exists" class_name) name_pos;

    let typ = ref_type class_name in
    Context.add ctx class_name (Context.Type typ);
    let ref_ctx = {ctx with
                   map = Hashtbl.copy ctx.map;
                   ref_class = Some(typ)} in
    Context.add_block ref_ctx;
    let arg_names, arg_types = set_generics ref_ctx types args
      (set_ref_generics typ)
      (fun idx name ->
        set_ref_generic_name typ idx name;
        get_ref_generic typ idx) in
    set_ref_record typ @@ record_type arg_names arg_types;

    List.iter functions ~f:(fun f ->
      ignore @@ get_seq_stmt ~ct:ref_ctx (f :> extended_statement));
    set_ref_done typ;
    pass_stmt (), pos
  | `Extend((class_name, _), functions, pos) ->
    let typ = match Context.in_scope ctx class_name with
    | Some (Context.Type t) -> t
    | _ -> seq_error (sprintf "cannot extend non-existing class %s" class_name) pos in
    let ref_ctx = {ctx with
                   map = Hashtbl.copy ctx.map;
                   ref_class = Some(typ)} in
    List.iter functions ~f:(fun f ->
      ignore @@ get_seq_stmt ~ct:ref_ctx (f :> extended_statement));
    pass_stmt (), pos
  | `Import(imports, pos) ->
    List.iter imports ~f:(fun ((what, pos), _) ->
      let file = sprintf "%s/%s.seq" (Filename.dirname ctx.filename) what in
      match Sys.file_exists file with
      | `Yes -> parsemod file ctx
      | _ -> 
        let seqpath = Option.value (Sys.getenv "SEQ_PATH") ~default:"" in
        let file = sprintf "%s/%s.seq" seqpath what in
        match Sys.file_exists file with
        | `Yes -> parsemod file ctx
        | _ -> seq_error (sprintf "cannot locate module %s" what) pos);
    pass_stmt (), pos
  end in
  ignore @@ finalize_stmt stmt pos

let rec parse_string ?fname ?debug code ctx =
  let fname = Option.value fname ~default:"" in 

  let lexbuf = Lexing.from_string (code ^ "\n") in
  try
    let state = Lexer.stack_create fname in
    let ast = Parser.program (Lexer.token state) lexbuf in

    if is_some debug then begin
      eprintf "%s%!" @@ T.sprintf [T.Bold; T.green] "|> AST of \n%s ==> \n" code;
      eprintf "%s%!" @@ T.sprintf [T.green] "%s\n%!" @@ Ast.prn_ast ast
    end;

    match ast with Module stmts ->
      List.iter stmts ~f:(fun x ->
        get_seq_stmt ctx parse_file (x :> extended_statement));
  with
  | Lexer.SyntaxError(msg, pos) ->
    raise @@ CompilerError(Lexer(msg), pos)
  | Parser.Error ->
    let pos = {lexbuf.lex_start_p with pos_fname = fname} in
    raise @@ CompilerError(Parser, pos)
  | SeqCamlError(msg, pos) ->
    raise @@ CompilerError(Descent(msg), pos)
  | SeqCError(msg, pos) ->
    raise @@ CompilerError(Compiler(msg), pos)

and parse_file ?debug file ctx =
  (* eprintf "==> parsing %s\n" file; *)
  let lines = In_channel.read_lines file in
  let code = (String.concat ~sep:"\n" lines) ^ "\n" in
  parse_string ?debug ~fname:(Filename.realpath file) code ctx

let init file error_handler =
  try
    let mdl = init_module () in
    let ctx = Context.init (Filename.realpath file) mdl mdl (get_module_block mdl) in

    let preamble = sprintf  "__argv__ = array[str](%d)" (Array.length Sys.argv) in
    let preamble = preamble::(Array.to_list @@ Array.mapi Sys.argv ~f:(fun idx s ->
      sprintf "__argv__[%d] = \"%s\"" idx s)) in
    parse_string (String.concat ~sep:"\n" preamble) ctx ~fname:"<preamble>";

    let seqpath = Option.value (Sys.getenv "SEQ_PATH") ~default:"" in
    let stdlib_path = sprintf "%s/stdlib.seq" seqpath in
    parse_file stdlib_path ctx;

    parse_file file ctx;
    Some mdl
  with CompilerError(typ, pos) ->
    error_handler typ pos;
    None

open Ctypes
let parse_c fname =
  let error_handler typ (pos: pos_t) =
    let file, line, col = pos.pos_fname, pos.pos_lnum, pos.pos_cnum - pos.pos_bol in
    let msg = match typ with
    | Lexer s -> s
    | Parser -> "parsing error"
    | Descent s -> s
    | Compiler s -> s in
    let callback = Foreign.foreign "caml_error_callback"
      (string @-> int @-> int @-> string @-> returning void) in
    callback msg line col file
  in
  let seq_module = init fname error_handler in
  match seq_module with
  | Some seq_module -> Ctypes.raw_address_of_ptr (Ctypes.to_voidp seq_module)
  | None -> Nativeint.zero

let () =
  let _ = Callback.register "parse_c" parse_c in

  if Array.length Sys.argv >= 2 then
    try
      let m = init Sys.argv.(1) (fun a b -> raise @@ CompilerError(a, b)) in
      match m with
      | Some m -> begin
        try exec_module m false
        with SeqCError(msg, pos) -> raise @@ CompilerError(Compiler(msg), pos)
      end
      | None -> raise Caml.Not_found
    with CompilerError (typ, pos) ->
      let file, line, col = pos.pos_fname, pos.pos_lnum, pos.pos_cnum - pos.pos_bol in
      let kind, msg = match typ with
      | Lexer s -> "lexer", s
      | Parser -> "parser", "Parsing error"
      | Descent s -> "descent", s
      | Compiler s -> "compiler", s in

      let file_line = 
        if String.length file > 0 && file.[0] <> '<' then 
          try
            let lines = In_channel.read_lines file in List.nth lines (line - 1)
          with _ -> None 
        else None in

      let style = [T.Bold; T.red] in
      eprintf "%s%!" @@ T.sprintf style "[ERROR] %s error: %s\n" kind msg;
      begin match file_line with 
      | Some file_line ->
        eprintf "%s%!" @@ T.sprintf style "        %s: %d,%d\n" file line col;
        eprintf "%s%!" @@ T.sprintf style "   %3d: %s" line (String.prefix file_line col);
        eprintf "%s%!" @@ T.sprintf [T.Bold; T.white; T.on_red] "%s" (String.drop_prefix file_line col);
        eprintf "%s%!" @@ T.sprintf [] "\n"
      | None -> ()
      end;
      exit 1
