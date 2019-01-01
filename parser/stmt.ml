(******************************************************************************
 *
 * Seq OCaml 
 * stmt.ml: Statement AST parsing module
 *
 * Author: inumanag
 *
 ******************************************************************************)

open Core
open Err
open Ast

(** This module is an implementation of [Intf.StmtIntf] module that
    describes statement AST parser.
    Requires [Intf.ExprIntf] for parsing expressions 
    ([parse] and [parse_type]) *)
module StmtParser (E : Intf.ExprIntf) : Intf.StmtIntf =
struct
  open ExprNode
  open StmtNode

  (* ***************************************************************
     Public interface
     *************************************************************** *)
  
  (** [parse context expr] dispatches statement AST to the proper parser 
      and finalizes the processed statement. *)
  let rec parse ?(toplevel=false) ?(jit=false) (ctx: Ctx.t) (pos, node) =
    let stmt = match node with
      | Break    p -> parse_break    ctx pos p
      | Continue p -> parse_continue ctx pos p
      | Expr     p -> parse_expr     ctx pos p
      | Assign   p -> parse_assign   ctx pos p ~toplevel ~jit
      | Del      p -> parse_del      ctx pos p
      | Print    p -> parse_print    ctx pos p ~jit
      | Return   p -> parse_return   ctx pos p
      | Yield    p -> parse_yield    ctx pos p
      | Assert   p -> parse_assert   ctx pos p
      | Type     p -> parse_type     ctx pos p ~toplevel
      | If       p -> parse_if       ctx pos p
      | While    p -> parse_while    ctx pos p
      | For      p -> parse_for      ctx pos p
      | Match    p -> parse_match    ctx pos p
      | Extern   p -> parse_extern   ctx pos p ~toplevel
      | Extend   p -> parse_extend   ctx pos p ~toplevel
      | Import   p -> parse_import   ctx pos p ~toplevel
      | Pass     p -> parse_pass     ctx pos p
      | Try      p -> parse_try      ctx pos p
      | Throw    p -> parse_throw    ctx pos p
      | Global   p -> parse_global   ctx pos p
      | Generic 
        Function p -> parse_function ctx pos p ~toplevel
      | Generic 
        Class    p -> parse_class    ctx pos p ~toplevel
    in
    finalize ctx stmt pos

  (** [parse_module context module] parses module AST.
      A module is just a simple list of statements. *)
  and parse_module ?(jit=false) (ctx: Ctx.t) mdl = 
    match mdl with
    | Module stmts ->
      let stmts = 
        if jit then 
          List.rev @@ match List.rev stmts with
            | (pos, Expr e) :: tl -> 
              (pos, Print (pos, String "\n")) :: (pos, Print e) :: tl
            | l -> l
        else stmts 
      in
      List.iter stmts ~f:(function 
        | pos, Generic Function ((_, { name; typ }), types, args, stmts) ->
          if is_some @@ Ctx.in_block ctx name then
            serr ~pos "function %s already exists" name;
          let fn = Llvm.Func.func name in
          let names = List.map args ~f:(fun (_, x) -> x.name) in
          Ctx.add ctx ~toplevel:true ~global:true
            name (Ctx.Namespace.Func (fn, names))
        | pos, Generic Class cls ->
          if is_some @@ Ctx.in_scope ctx cls.class_name then
            serr ~pos "class %s already exists" cls.class_name;
          let typ = Llvm.Type.cls cls.class_name in
          Ctx.add ctx ~toplevel:true ~global:true
            cls.class_name (Ctx.Namespace.Type typ)
        | _ -> ());
      ignore @@ List.map stmts ~f:(parse ctx ~toplevel:true ~jit)

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

  and parse_assign ctx pos ~toplevel ~jit (lhs, rhs, shadow) =
    let rh_expr = E.parse ctx rhs in
    match lhs with
    | pos, Id var ->
      begin match Hashtbl.find ctx.map var with
      | Some ((Ctx.Namespace.Var v, { base; global; toplevel; _ }) :: _) 
        when (not shadow) 
          && (global || (ctx.base = base)) ->
        Llvm.Stmt.assign v rh_expr
      | Some ((Ctx.Namespace.(Type _ | Func _ | Import _), _) :: _) ->
        serr ~pos "cannot assign functions or types"
      | _ when jit && toplevel ->
        let v = Ctx.Namespace.Var (Llvm.JIT.var ctx.mdl rh_expr) in
        Ctx.add ctx ~toplevel ~global:true var v;
        Llvm.Stmt.pass ()
      | _ ->
        let var_stmt = Llvm.Stmt.var rh_expr in
        Ctx.add ctx ~toplevel var (Ctx.Namespace.Var (Llvm.Var.stmt var_stmt));
        var_stmt
      end
    | pos, Dot (lh_lhs, lh_rhs) -> (* a.x = b *)
      Llvm.Stmt.assign_member (E.parse ctx lh_lhs) lh_rhs rh_expr
    | pos, Index (var_expr, [index_expr]) -> (* a[x] = b *)
      let var_expr = E.parse ctx var_expr in
      let index_expr = E.parse ctx index_expr in
      Llvm.Stmt.assign_index var_expr index_expr rh_expr
    | _ ->
      serr ~pos "assignment requires Id / Dot / Index on LHS"
    
  and parse_del ctx pos expr =
    match expr with
    | pos, Index (lhs, [rhs]) ->
      let lhs_expr = E.parse ctx lhs in
      let rhs_expr = E.parse ctx rhs in
      Llvm.Stmt.del_index lhs_expr rhs_expr
    | pos, Id var ->
      begin match Ctx.in_scope ctx var with
        | Some (Ctx.Namespace.Var v, _) ->
          Ctx.remove ctx var;
          Llvm.Stmt.del v
        | _ ->
          serr ~pos "cannot find variable %s" var
      end
    | _ -> 
      serr ~pos "cannot del non-index expression"

  and parse_print ctx _ ~jit expr =    
    let expr = E.parse ctx expr in
    if jit then 
      Llvm.Stmt.print_jit expr
    else
      Llvm.Stmt.print expr

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

  and parse_assert ctx _ expr =
    let expr = E.parse ctx expr in
    Llvm.Stmt.assrt expr 

  and parse_type ctx pos ~toplevel (name, args) =
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
    let typ = Llvm.Type.record arg_names arg_types name in
    Ctx.add ctx ~toplevel ~global:toplevel name (Ctx.Namespace.Type typ);
    Llvm.Stmt.pass ()

  and parse_while ctx pos (cond, stmts) =
    let cond_expr = E.parse ctx cond in
    let while_stmt = Llvm.Stmt.while_loop cond_expr in

    let block = Llvm.Block.while_loop while_stmt in
    add_block { ctx with block } stmts;

    while_stmt

  (** [parse_for ?next context position data] parses for statement AST. 
      `next` points to the nested for in the generator expression.  *)
  and parse_for ?next ctx pos (for_vars, gen_expr, stmts) =
    let gen_expr = E.parse ctx gen_expr in
    let for_stmt = Llvm.Stmt.loop gen_expr in
    let block = Llvm.Block.loop for_stmt in
    let for_ctx = { ctx with block } in

    Ctx.add_block for_ctx;
    let var = Llvm.Var.loop for_stmt in
    begin match for_vars with
      | [name] ->
        Ctx.add for_ctx name (Ctx.Namespace.Var var)
      | for_vars -> 
        let var_expr = Llvm.Expr.var var in
        List.iteri for_vars ~f:(fun idx var_name ->
          let expr = Llvm.Expr.lookup var_expr (Llvm.Expr.int idx) in
          let var_stmt = Llvm.Stmt.var expr in
          ignore @@ finalize for_ctx var_stmt pos;
          let var = Llvm.Var.stmt var_stmt in
          Ctx.add for_ctx var_name (Ctx.Namespace.Var var))
    end;
    let _ = match next with 
      | Some next -> 
        next ctx for_ctx for_stmt
      | None -> 
        ignore @@ List.map stmts ~f:(parse for_ctx ~toplevel:false)
    in
    Ctx.clear_block for_ctx;

    for_stmt

  and parse_if ctx pos cases =
    let if_stmt = Llvm.Stmt.cond () in
    List.iter cases ~f:(function (_, { cond; cond_stmts }) ->
      let block = match cond with
        | None ->
          Llvm.Block.elseb if_stmt
        | Some cond_expr ->
          let expr = E.parse ctx cond_expr in
          Llvm.Block.elseif if_stmt expr
      in
      add_block { ctx with block } cond_stmts);
    if_stmt

  and parse_match ctx pos (what, cases) =
    let what = E.parse ctx what in
    let match_stmt = Llvm.Stmt.matchs what in
    List.iter cases ~f:(fun (_, { pattern; case_stmts }) ->
      let pat, var = match pattern with
        | BoundPattern(name, pat) ->
          Ctx.add_block ctx;
          let pat = parse_pattern ctx pos pat in
          let pat = Llvm.Pattern.bound pat in
          Ctx.clear_block ctx;
          pat, Some(name, Llvm.Var.bound_pattern pat)
        | _ as pat ->
          Ctx.add_block ctx;
          let pat = parse_pattern ctx pos pat in
          Ctx.clear_block ctx;
          pat, None
      in
      let block = Llvm.Block.case match_stmt pat in
      add_block { ctx with block } case_stmts ~preprocess:(fun ctx ->
        match var with 
        | Some (n, v) -> Ctx.add ctx n (Ctx.Namespace.Var v) 
        | None -> ()));
    match_stmt
  
  and parse_extern ctx pos ~toplevel 
    (lang, dylib, ctx_name, (_, { name; typ }), args) =

    if lang <> "c" && lang <> "C" then
      serr ~pos "only C external functions are currently supported";
    if is_some @@ Ctx.in_block ctx ctx_name then
      serr ~pos "function %s already exists" ctx_name;
    
    let names, types = 
      List.map args ~f:(fun (_, { name; typ }) ->
        name, E.parse_type ctx (Option.value_exn typ))
      |> List.unzip
    in
    let fn = Llvm.Func.func name in
    Llvm.Func.set_args fn names types;
    Llvm.Func.set_extern fn;
    let typ = E.parse_type ctx (Option.value_exn typ) in
    Llvm.Func.set_type fn typ;
    
    let names = List.map args ~f:(fun (_, x) -> x.name) in
    Ctx.add ctx ~toplevel ~global:toplevel 
      ctx_name (Ctx.Namespace.Func (fn, names));
    Llvm.Stmt.func fn

  and parse_extend ctx pos ~toplevel (name, stmts) =
    if not toplevel then
      serr ~pos "extends must be declared at toplevel";

    let typ = match Ctx.in_scope ctx name with
      | Some (Ctx.Namespace.Type t, _) -> t
      | _ -> serr ~pos "cannot extend non-existing class %s" name
    in
    let new_ctx = { ctx with map = Hashtbl.copy ctx.map } in
    ignore @@ List.map stmts ~f:(function
      | pos, Function f -> 
        parse_function new_ctx pos f ~cls:typ
      | _ -> 
        failwith "classes only support functions as members");

    Llvm.Stmt.pass ()
  
  (** [parse_import ?ext context position data] parses import AST.
      Import file extension is set via [seq] (default is [".seq"]). *)
  and parse_import ctx pos ?(ext="seq") ~toplevel imports =
    List.iter imports ~f:(fun { from; what; import_as; stdlib } ->
      let from = snd from in
      let file = sprintf "%s/%s.%s" (Filename.dirname ctx.filename) from ext in
      let new_ctx = if stdlib then ctx else
        { (Ctx.init file ctx.mdl ctx.base ctx.block ctx.parse_file)
          with trycatch = ctx.trycatch }
      in
      begin match Sys.file_exists file with
        | `Yes -> 
          new_ctx.parse_file new_ctx file
        | `No | `Unknown -> 
          let seqpath = Option.value (Sys.getenv "SEQ_PATH") ~default:"" in
          let file = sprintf "%s/%s.%s" seqpath from ext in
          match Sys.file_exists file with
          | `Yes -> 
            new_ctx.parse_file new_ctx file
          | `No | `Unknown -> 
            serr ~pos "cannot locate module %s" from
      end;
      if not stdlib then match what with
        | None -> (* import foo (as bar) *)
          let from = Option.value import_as ~default:from in
          let map = Hashtbl.filteri new_ctx.map ~f:(fun ~key ~data ->
            match data with
            | [] -> false 
            | (_, { global; internal; _ }) :: _ -> global && (not internal))
          in
          Ctx.add ctx ~toplevel ~global:toplevel  
            from (Ctx.Namespace.Import map)
        | Some [_, ("*", None)] -> (* from foo import * *)
          Hashtbl.iteri new_ctx.map ~f:(fun ~key ~data ->
            match data with
            | (var, { global = true; internal = false; _ }) :: _ ->
              Util.dbg "[import] adding %s::%s" from key;
              Ctx.add ctx ~toplevel ~global:toplevel 
                key var
            | _ -> ());
        | Some lst -> (* from foo import bar *)
          List.iter lst ~f:(fun (pos, (name, import_as)) ->
            match Ctx.in_scope new_ctx name with
            | Some (var, ({ global = true; internal = false; _ } as ann)) -> 
              let name = Option.value import_as ~default:name in
              Ctx.add ctx 
                ~toplevel ~global:toplevel 
                name var
            | _ ->
              serr ~pos "name %s not found in %s" name from));
    Llvm.Stmt.pass ()

  (** [parse_function ?cls context position data] parses function AST.
      Set `cls` to `Llvm.Types.typ` if you want a function to be 
      a class `cls` method. *)
  and parse_function ctx pos ?cls ?(toplevel=false)
    ((_, { name; typ }), types, args, stmts) =

    let fn = match Ctx.in_block ctx name with
      | Some (Ctx.Namespace.Func (fn, _), _) when toplevel ->
        fn 
      | Some _ ->
        serr ~pos "function %s already exists" name;
      | None when not toplevel ->
        Llvm.Func.func name
      | None ->
        failwith "function pre-register failed"
    in
    begin match cls with 
      | Some cls -> 
        Llvm.Type.add_cls_method cls name fn
      | None -> 
        let names = List.map args ~f:(fun (_, x) -> x.name) in
        if not toplevel then begin
          Ctx.add ctx ~toplevel ~global:toplevel 
            name (Ctx.Namespace.Func (fn, names));
          Llvm.Func.set_enclosing fn ctx.base
        end
    end;

    let new_ctx = 
      { ctx with 
        base = fn; 
        stack = Stack.create ();
        block = Llvm.Block.func fn;
        map = Hashtbl.copy ctx.map } 
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

    Option.value_map typ
      ~f:(fun typ -> Llvm.Func.set_type fn (E.parse_type new_ctx typ))
      ~default:();

    add_block new_ctx stmts 
      ~preprocess:(fun ctx ->
        List.iter names ~f:(fun name ->
          let var = Ctx.Namespace.Var (Llvm.Func.get_arg fn name) in
          Ctx.add ctx name var));
    Llvm.Stmt.func fn
  
  and parse_class ctx pos ~toplevel cls =
    (* ((name, types, args, stmts) as stmt) *)
    if not toplevel then
      serr ~pos "classes must be declared at toplevel";
    let typ = match Ctx.in_block ctx cls.class_name with
      | Some (Ctx.Namespace.Type typ, _) ->
        typ
      | Some _ | None ->
        failwith "class pre-register failed"
    in
    let new_ctx = 
      { ctx with 
        map = Hashtbl.copy ctx.map;
        stack = Stack.create () } 
    in
    Ctx.add_block new_ctx;
    
    begin match cls.args with
      | None -> ()
      | Some args ->
        List.iter args ~f:(fun (pos, { name; typ }) ->
          if is_none typ then 
            serr ~pos "class field %s does not have type" name);
        let names, types = parse_generics 
          new_ctx 
          cls.generics args
          (Llvm.Generics.Type.set_number typ)
          (fun idx name ->
            Llvm.Generics.Type.set_name typ idx name;
            Llvm.Generics.Type.get typ idx) 
        in
        Llvm.Type.set_cls_args typ names types;
    end;

    ignore @@ List.map cls.members ~f:(function
      | pos, Function f -> 
        parse_function new_ctx pos f ~cls:typ
      | _ -> failwith "classes only support functions as members");
    Llvm.Type.set_cls_done typ;

    Llvm.Stmt.pass ()

  and parse_try ctx pos (stmts, catches, finally) =
    let try_stmt = Llvm.Stmt.trycatch () in

    let block = Llvm.Block.try_block try_stmt in
    add_block { ctx with block ; trycatch = try_stmt } stmts;

    List.iteri catches ~f:(fun idx (pos, { exc; var; stmts }) ->
      let typ = match exc with
        | Some exc -> E.parse_type ctx (pos, Id(exc)) 
        | None -> Ctypes.null 
      in
      let block = Llvm.Block.catch try_stmt typ in
      add_block { ctx with block } stmts
        ~preprocess:(fun ctx ->
          Option.value_map var 
            ~f:(fun var ->
              let v = Llvm.Var.catch try_stmt idx in
              Ctx.add ctx var (Ctx.Namespace.Var v))
            ~default: ()) 
    );

    Option.value_map finally 
      ~f:(fun final ->
        let block = Llvm.Block.finally try_stmt in
        add_block { ctx with block } final)
      ~default:();

    try_stmt

  and parse_throw ctx _ expr =
    let expr = E.parse ctx expr in
    Llvm.Stmt.throw expr

  and parse_global ctx pos var =
    match Hashtbl.find ctx.map var with
    | Some ((Ctx.Namespace.Var v, ({ internal = false; _ } as ann)) :: rest) ->
      if (ctx.base = ann.base) || ann.global then 
        serr ~pos "symbol '%s' either local or already set as global" var;
      Llvm.Var.set_global v;
      let new_var = Ctx.Namespace.(Var v, { ann with global = true }) in
      Hashtbl.set ctx.map ~key:var ~data:(new_var :: rest);
      Llvm.Stmt.pass ()
    | _ ->
      serr ~pos "symbol '%s' not found or not a variable" var

  (* ***************************************************************
     Helper functions
     *************************************************************** *)

  (** [finalize ~add context statement position] finalizes [Llvm.Types.stmt]
      by setting its base to [context.base], its position to [position]
      and by adding the [statement] to the current block ([context.block])
      if [add] is [true]. Returns the finalized statement. *)
  and finalize ?(add=true) ctx stmt pos =
    Llvm.Stmt.set_base stmt ctx.base;
    Llvm.Stmt.set_pos stmt pos;
    if add then
      Llvm.Block.add_stmt ctx.block stmt;
    stmt 
  
  (** [add_block ?preprocess context statements] creates a new block within
      [context[ and adds [statements] to that block. 
      [preprocess context], if provided, is run after the block is created 
      to initialize the block. *)
  and add_block ctx ?(preprocess=(fun _ -> ())) stmts =
    Ctx.add_block ctx;
    preprocess ctx;
    ignore @@ List.map stmts ~f:(parse ctx ~toplevel:false);
    Ctx.clear_block ctx

  (** Helper for parsing match patterns *)
  and parse_pattern ctx pos = function
    | StarPattern ->
      Llvm.Pattern.star ()
    | BoundPattern _ ->
      serr ~pos "invalid bound pattern"
    | IntPattern i -> 
      Llvm.Pattern.int i
    | BoolPattern b -> 
      Llvm.Pattern.bool b
    | StrPattern s -> 
      Llvm.Pattern.str s
    | SeqPattern s -> 
      Llvm.Pattern.seq s
    | TuplePattern tl ->
      let tl = List.map tl ~f:(parse_pattern ctx pos) in
      Llvm.Pattern.record tl
    | RangePattern (i, j) -> 
      Llvm.Pattern.range i j
    | ListPattern tl ->
      let tl = List.map tl ~f:(parse_pattern ctx pos) in
      Llvm.Pattern.array tl
    | OrPattern tl ->
      let tl = List.map tl ~f:(parse_pattern ctx pos) in
      Llvm.Pattern.orp tl
    | WildcardPattern wild ->
      let pat = Llvm.Pattern.wildcard () in
      if is_some wild then begin
        let var = Llvm.Var.bound_pattern pat in
        Ctx.add ctx (Option.value_exn wild) (Ctx.Namespace.Var var)
      end;
      pat
    | GuardedPattern (pat, expr) ->
      let pat = parse_pattern ctx pos pat in
      let expr = E.parse ctx expr in
      Llvm.Pattern.guarded pat expr

  (** Helper for parsing generic parameters. 
      Parses generic parameters, assigns names to unnamed generics and
      calls C++ APIs to denote generic functions/classes.
      Also adds generics types to the context. *)
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
      | Generic g when String.is_prefix g ~prefix:"``" -> Some g
      | _ -> None)
    in
    let generics = 
      List.append type_args generic_args
      |> List.dedup_and_sort ~compare in
    set_generic_count (List.length generics);

    List.iteri generics ~f:(fun cnt key ->
      Ctx.add ctx key (Ctx.Namespace.Type (get_generic cnt key)));
    let types = List.map types ~f:(E.parse_type ctx) in
    names, types
end