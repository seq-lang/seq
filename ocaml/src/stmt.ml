(* 786 *)

open Core
open Err
open Ast
open Util

module StmtParser (E : Intf.Expr) : Intf.Stmt =
struct
  open ExprNode
  open StmtNode

  (* ***************************************************************
     Public interface
     *************************************************************** *)
  
  (** Dispatch function for [StmtNode.t] *)
  let rec parse (ctx: Ctx.t) (pos, node) =
    let stmt = match node with
      | Break    p -> parse_break    ctx pos p
      | Continue p -> parse_continue ctx pos p
      | Expr     p -> parse_expr     ctx pos p
      | Assign   p -> parse_assign   ctx pos p
      | Del      p -> parse_del      ctx pos p
      | Print    p -> parse_print    ctx pos p
      | Return   p -> parse_return   ctx pos p
      | Yield    p -> parse_yield    ctx pos p
      | Assert   p -> parse_assert   ctx pos p
      | Type     p -> parse_type     ctx pos p
      | If       p -> parse_if       ctx pos p
      | While    p -> parse_while    ctx pos p
      | For      p -> parse_for      ctx pos p
      | Match    p -> parse_match    ctx pos p
      | Extern   p -> parse_extern   ctx pos p
      | Extend   p -> parse_extend   ctx pos p
      | Import   p -> parse_import   ctx pos p
      | Pass     p -> parse_pass     ctx pos p
      | Generic 
        Function p -> parse_function ctx pos p
      | Generic 
        Class    p -> parse_class    ctx pos p 
    in
    finalize_stmt ctx stmt pos

  (** Dispatch function for [Ast.t] *)
  and parse_module (ctx: Ctx.t) mdl = 
    match mdl with
    | Module stmts -> 
      ignore @@ List.map stmts ~f:(parse ctx)

  (* ***************************************************************
     Node parsers
     ***************************************************************
     Each AST node is dispatched to its parser function.
     Each parser function [f] is called as [f context position data]
     where [data] is a tuple varying from node to node. *)
  
  and parse_pass _ _ _ =
    Llvm.Stmt.pass ()

  and parse_break _ _ _ =
    Llvm.Stmt.break ()
    
  and parse_continue _ _ _ =
    Llvm.Stmt.continue ()

  and parse_expr ctx pos expr =
    match snd expr with
    | Id "___dump___" ->
      Ctx.dump ctx;
      Llvm.Stmt.pass ()
    | _ ->
      let expr = E.parse ctx expr in
      Llvm.Stmt.expr expr

  and parse_assign ctx pos (lh, rh, shadow) =
    match lh, rh with
    | [lhs], [rhs] ->
      let rh_expr = E.parse ctx rhs in
      parse_assign_helper ctx pos (lhs, rh_expr, shadow)
    | lh, rh when List.length lh <> List.length rh ->
      serr ~pos "RHS length must match LHS";
    | _, _ ->
      let var_stmts = List.map rh ~f:(fun rhs ->
        let rh_expr = E.parse ctx rhs in
        finalize_stmt ctx (Llvm.Stmt.var rh_expr) pos) 
      in
      List.iter (List.zip_exn lh var_stmts) ~f:(fun ((pos, _) as lhs, varst) ->
        let rh_expr = Llvm.Expr.var (Llvm.Var.var_of_stmt varst) in
        let stmt = parse_assign_helper ctx pos (lhs, rh_expr, shadow) in
        ignore @@ finalize_stmt ctx stmt pos);
      Llvm.Stmt.pass ()

  and parse_assign_helper ctx pos ((pos, lhs), rh_expr, shadow) =
    match lhs with
    | Id var ->
      begin match Hashtbl.find ctx.map var with
      | Some (Ctx.Assignable.Var v :: _) when not shadow ->
        Llvm.Stmt.assign v rh_expr
      | Some (Ctx.Assignable.(Type _ | Func _) :: _) ->
        serr ~pos "cannot assign functions or types"
      | _ ->
        let var_stmt = Llvm.Stmt.var rh_expr in
        Ctx.add ctx var (Ctx.Assignable.Var (Llvm.Var.var_of_stmt var_stmt));
        var_stmt
      end
    | Dot(lh_lhs, lh_rhs) -> (* a.x = b *)
      Llvm.Stmt.assign_member (E.parse ctx lh_lhs) lh_rhs rh_expr
    | Index(var_expr, [index_expr]) -> (* a[x] = b *)
      let var_expr = E.parse ctx var_expr in
      let index_expr = E.parse ctx index_expr in
      Llvm.Stmt.assign_index var_expr index_expr rh_expr
    | _ ->
      serr ~pos "assignment requires Id / Dot / Index on LHS"
    
  and parse_del ctx pos exprs =
    List.iter exprs ~f:(fun (pos, node) ->
      match node with
      | Index(lhs, [rhs]) ->
        let lhs_expr = E.parse ctx lhs in
        let rhs_expr = E.parse ctx rhs in
        let stmt = Llvm.Stmt.del_index lhs_expr rhs_expr in
        ignore @@ finalize_stmt ctx stmt pos
      | Id var ->
        failwith "stmt/parse_del -- can't remove variable"
      | _ -> 
        serr ~pos "cannot del non-index expression");
    Llvm.Stmt.pass ()

  and parse_print ctx pos exprs =
    let str_node s = 
      (pos, ExprNode.String(s))
    in
    List.iteri exprs ~f:(fun i ((pos, _) as expr) ->
      if i > 0 then begin
        let stmt = Llvm.Stmt.print 
          (E.parse ctx @@ str_node " ") 
        in
        ignore @@ finalize_stmt ctx stmt pos
      end;
      let stmt = Llvm.Stmt.print (E.parse ctx expr) in
      ignore @@ finalize_stmt ctx stmt pos);
    Llvm.Stmt.print (E.parse ctx @@ str_node "\n")

  and parse_return ctx pos ret =
    match ret with
    | None ->
      Llvm.Stmt.return Ctypes.null
    | Some ret ->
      let expr = E.parse ctx ret in
      let ret_stmt = Llvm.Stmt.return expr in
      Llvm.Func.set_return ctx.base ret_stmt;
      ret_stmt

  and parse_yield ctx pos ret =
    match ret with
    | None ->
      Llvm.Stmt.yield Ctypes.null
    | Some ret -> 
      let expr = E.parse ctx ret in
      let yield_stmt = Llvm.Stmt.yield expr in
      Llvm.Func.set_yield ctx.base yield_stmt;
      yield_stmt

  and parse_assert ctx pos exprs =
    List.iter exprs ~f:(function (pos, _) as expr ->
      let expr = E.parse ctx expr in
      let stmt = Llvm.Stmt.assrt expr in
      ignore @@ finalize_stmt ctx stmt pos);
    Llvm.Stmt.pass ()

  and parse_type ctx pos (name, args) =
    let arg_names, arg_types =  
      List.map args ~f:(function
        | (pos, { name; typ = None }) ->
          serr ~pos "type member %s must have type specification" name
        | (_,   { name; typ = Some t }) -> 
          (name, t)) 
      |> List.unzip
    in
    if is_some @@ Ctx.in_scope ctx name then
      serr ~pos "type %s already defined" name;
    let arg_types = List.map arg_types ~f:(E.parse_type ctx) in
    let typ = Llvm.Type.record arg_names arg_types in
    Ctx.add ctx name (Ctx.Assignable.Type typ);
    Llvm.Stmt.pass ()

  and parse_while ctx pos (cond, stmts) =
    let cond_expr = E.parse ctx cond in
    let while_stmt = Llvm.Stmt.while_loop cond_expr in

    let block = Llvm.Stmt.Block.while_loop while_stmt in
    add_block { ctx with block } stmts;

    while_stmt

  and parse_for ctx pos (for_vars, gen_expr, stmts) =
    let gen_expr = E.parse ctx gen_expr in
    let for_stmt = Llvm.Stmt.loop gen_expr in
    let block = Llvm.Stmt.Block.loop for_stmt in

    let var = Llvm.Var.loop for_stmt in
    let var_expr = Llvm.Expr.var var in
    
    add_block { ctx with block } stmts 
      ~preprocess:(fun ctx ->
        match for_vars with
        | [name] ->
          let var = Llvm.Var.loop for_stmt in
          Ctx.add ctx name (Ctx.Assignable.Var var)
        | for_vars -> 
          List.iteri for_vars ~f:(fun idx var_name ->
            let expr = Llvm.Expr.lookup var_expr (Llvm.Expr.int idx) in
            let var_stmt = Llvm.Stmt.var expr in
            let var = Llvm.Var.var_of_stmt var_stmt in
            Ctx.add ctx var_name (Ctx.Assignable.Var var)
          )
      );
    for_stmt

  and parse_if ctx pos cases =
    let if_stmt = Llvm.Stmt.cond () in
    List.iter cases ~f:(function (_, {cond; stmts}) ->
      let block = match cond with
        | None ->
          Llvm.Stmt.Block.elseb if_stmt
        | Some cond_expr ->
          let expr = E.parse ctx cond_expr in
          Llvm.Stmt.Block.elseif if_stmt expr
      in
      add_block { ctx with block } stmts);
    if_stmt

  and parse_match ctx pos (what, cases) =
    let what = E.parse ctx what in
    let match_stmt = Llvm.Stmt.matchs what in
    List.iter cases ~f:(fun (_, {pattern; stmts}) ->
      let pat, var = match pattern with
        | BoundPattern(name, pat) ->
          Ctx.add_block ctx;
          let pat = parse_pattern ctx pos pat in
          let pat = Llvm.Stmt.Pattern.bound pat in
          Ctx.clear_block ctx;
          pat, Some(name, Llvm.Var.bound_pattern pat)
        | _ as pat ->
          Ctx.add_block ctx;
          let pat = parse_pattern ctx pos pat in
          Ctx.clear_block ctx;
          pat, None
      in
      let block = Llvm.Stmt.Block.case match_stmt pat in
      add_block { ctx with block } stmts ~preprocess:(fun ctx ->
        match var with 
        | Some(n, v) -> Ctx.add ctx n (Ctx.Assignable.Var v) 
        | None -> ()));
    match_stmt
  
  and parse_extern ctx pos (lang, dylib, (_, {name; typ}), args) =
    if lang <> "c" && lang <> "C" then
      serr ~pos "only C external functions are currently supported";
    if is_some @@ Ctx.in_block ctx name then
      serr ~pos "function %s already exists" name;
    
    let names, types = 
      List.map args ~f:(fun (_, {name; typ}) ->
        name, E.parse_type ctx (Option.value_exn typ))
      |> List.unzip
    in
    let fn = Llvm.Func.func name in
    Llvm.Func.set_args fn names types;
    Llvm.Func.set_extern fn;
    let typ = E.parse_type ctx (Option.value_exn typ) in
    Llvm.Func.set_return fn typ;
    
    Ctx.add ctx name (Ctx.Assignable.Func fn);
    Llvm.Stmt.func fn

  and parse_extend ctx pos (name, stmts) =
    let typ = match Ctx.in_scope ctx name with
      | Some (Ctx.Assignable.Type t) -> t
      | _ -> serr ~pos "cannot extend non-existing class %s" name
    in
    let new_ctx = 
      { ctx with 
        map = Hashtbl.filter ctx.map
          ~f:(function Ctx.Assignable.Var _ :: _ -> false | _ -> true) } 
    in
    ignore @@ List.map stmts ~f:(function
      | pos, Function f -> parse_function new_ctx pos f ~cls:typ
      | _ -> failwith "classes only support functions as members");

    Llvm.Stmt.pass ()
  
  and parse_import ?(ext="seq") ctx pos imports =
    List.iter imports ~f:(fun ((pos, what), _) ->
      let file = sprintf "%s/%s.%s" (Filename.dirname ctx.filename) what ext in
      match Sys.file_exists file with
      | `Yes -> 
        ctx.parse_file ctx file
      | _ -> 
        let seqpath = Option.value (Sys.getenv "SEQ_PATH") ~default:"" in
        let file = sprintf "%s/%s.%s" seqpath what ext in
        match Sys.file_exists file with
        | `Yes -> 
          ctx.parse_file ctx file
        | _ -> 
          serr ~pos "cannot locate module %s" what);
    Llvm.Stmt.pass ()

  and parse_function ?cls ctx pos ((_, { name; typ }), types, args, stmts) =
    if is_some @@ Ctx.in_block ctx name then
      serr ~pos "function %s already exists" name;

    let fn = Llvm.Func.func name in
    begin match cls with 
      | Some cls -> Llvm.Type.add_cls_method cls name fn
      | None -> Ctx.add ctx name (Ctx.Assignable.Func fn)
    end;
    Option.value_map typ
      ~f:(fun typ -> Llvm.Func.set_return fn (E.parse_type ctx typ))
      ~default:();

    let new_ctx = 
      { ctx with 
        map = Hashtbl.filter ctx.map
          ~f:(function Ctx.Assignable.Var _ :: _ -> false | _ -> true);
        base = fn; 
        stack = Stack.create ();
        block = Llvm.Stmt.Block.func fn } 
    in
    Ctx.add_block new_ctx;
    let names, types = parse_generics 
      new_ctx 
      types args
      (Llvm.Generics.Func.set_number fn)
      (fun idx name ->
        Llvm.Generics.Func.set_name fn idx name;
        Llvm.Generics.Func.get fn idx) 
    in
    Llvm.Func.set_args fn names types;

    add_block new_ctx stmts 
      ~preprocess:(fun ctx ->
        List.iter names ~f:(fun name ->
          let var = Ctx.Assignable.Var (Llvm.Func.get_arg fn name) in
          Ctx.add ctx name var));
    Llvm.Stmt.func fn
  
  and parse_class ctx pos ((name, types, args, stmts) as stmt) =
    if is_some @@ Ctx.in_scope ctx name then
      serr ~pos "class %s already exists" name;
    List.iter args ~f:(fun (pos, { name; typ }) ->
      if is_none typ then 
        serr ~pos "class field %s does not have type" name);

    let typ = Llvm.Type.cls name in
    Ctx.add ctx name (Ctx.Assignable.Type typ);

    let new_ctx = 
      { ctx with 
        map = Hashtbl.filter ctx.map
          ~f:(function Ctx.Assignable.Var _ :: _ -> false | _ -> true);
        stack = Stack.create () } 
    in
    Ctx.add_block new_ctx;
    let names, types = parse_generics 
      new_ctx 
      types args
      (Llvm.Generics.Type.set_number typ)
      (fun idx name ->
        Llvm.Generics.Type.set_name typ idx name;
        Llvm.Generics.Type.get typ idx) 
    in
    Llvm.Type.set_cls_args typ names types;
    ignore @@ List.map stmts ~f:(function
      | pos, Function f -> parse_function new_ctx pos f ~cls:typ
      | _ -> failwith "classes only support functions as members");
    Llvm.Type.set_cls_done typ;

    Llvm.Stmt.pass ()

  (* ***************************************************************
     Helper functions
     *************************************************************** *)

  and finalize_stmt ?(add=true) ctx stmt pos =
    Llvm.Stmt.set_base stmt ctx.base;
    Llvm.Stmt.set_pos stmt pos;
    if add then
      Llvm.Stmt.Block.add_stmt ctx.block stmt;
    stmt 
  
  and add_block ctx ?(preprocess=(fun _ -> ())) stmts =
    Ctx.add_block ctx;
    preprocess ctx;
    ignore @@ List.map stmts ~f:(parse ctx);
    Ctx.clear_block ctx

  and parse_pattern ctx pos = function
    | StarPattern ->
      Llvm.Stmt.Pattern.star ()
    | BoundPattern _ ->
      serr ~pos "invalid bound pattern"
    | IntPattern i -> 
      Llvm.Stmt.Pattern.int i
    | BoolPattern b -> 
      Llvm.Stmt.Pattern.bool b
    | StrPattern s -> 
      Llvm.Stmt.Pattern.str s
    | SeqPattern s -> 
      Llvm.Stmt.Pattern.seq s
    | TuplePattern tl ->
      let tl = List.map tl ~f:(parse_pattern ctx pos) in
      Llvm.Stmt.Pattern.record tl
    | RangePattern (i, j) -> 
      Llvm.Stmt.Pattern.range i j
    | ListPattern tl ->
      let tl = List.map tl ~f:(parse_pattern ctx pos) in
      Llvm.Stmt.Pattern.array tl
    | OrPattern tl ->
      let tl = List.map tl ~f:(parse_pattern ctx pos) in
      Llvm.Stmt.Pattern.orp tl
    | WildcardPattern wild ->
      let pat = Llvm.Stmt.Pattern.wildcard () in
      if is_some wild then begin
        let var = Llvm.Var.bound_pattern pat in
        Ctx.add ctx (Option.value_exn wild) (Ctx.Assignable.Var var)
      end;
      pat
    | GuardedPattern (pat, expr) ->
      let pat = parse_pattern ctx pos pat in
      let expr = E.parse ctx expr in
      Llvm.Stmt.Pattern.guarded pat expr

  and parse_generics ctx generic_types args set_generic_count get_generic =
    let names, types = 
      List.map args ~f:(function
        | _, { name; typ = Some (typ) } -> 
          name, typ
        | pos, { name; typ = None }-> 
          name, (pos, ExprNode.Generic (sprintf "``%s" name)))
      |> List.unzip
    in
    let type_args = List.map generic_types ~f:snd in
    let generic_args = List.filter_map types ~f:(fun x ->
      match snd x with
      | Generic(g) when String.is_prefix g ~prefix:"``" -> Some g
      | _ -> None)
    in
    let generics = 
      List.append type_args generic_args
      |> List.dedup_and_sort ~compare in
    set_generic_count (List.length generics);

    List.iteri generics ~f:(fun cnt key ->
      Ctx.add ctx key (Ctx.Assignable.Type (get_generic cnt key)));
    let types = List.map types ~f:(E.parse_type ctx) in
    names, types
end