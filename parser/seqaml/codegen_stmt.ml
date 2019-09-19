(* ****************************************************************************
 * Seqaml.Codegen_stmt: Generate Seq code from statement AST
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Err

(** This module implements [Codegen_intf.Stmt].
    Parametrized by [Codegen_intf.Expr] for parsing expressions ([parse] and [parse_type]) *)
module Codegen (E : Codegen_intf.Expr) : Codegen_intf.Stmt = struct
  open Ast.Expr
  open Ast.Stmt

  (* ***************************************************************
     Public interface
     *************************************************************** *)

  (** [parse ~ctx expr] dispatches a statement AST node [expr] to the proper code generation function. *)
  let rec parse ?(toplevel = false) ?(jit = false) ~(ctx : Codegen_ctx.t) (ann, node) =
    let stmt =
      match node with
      | Break p -> parse_break ctx ann p
      | Continue p -> parse_continue ctx ann p
      | Expr p -> parse_expr ctx ann p
      | Assign p -> parse_assign ctx ann p ~toplevel ~jit
      | Del p -> parse_del ctx ann p
      | Print p -> parse_print ctx ann p ~jit
      | Return p -> parse_return ctx ann p
      | Yield p -> parse_yield ctx ann p
      | Assert p -> parse_assert ctx ann p
      | TypeAlias p -> parse_type_alias ctx ann p ~toplevel
      | If p -> parse_if ctx ann p
      | While p -> parse_while ctx ann p
      | For p -> parse_for ~ctx ann p
      | Match p -> parse_match ctx ann p
      | Extern p -> parse_extern ctx ann p ~toplevel
      | Extend p -> parse_extend ctx ann p ~toplevel
      | Import p -> parse_import ctx ann p ~toplevel
      | ImportPaste p -> parse_impaste ctx ann p
      | Pass p -> parse_pass ctx ann p
      | Try p -> parse_try ctx ann p
      | Throw p -> parse_throw ctx ann p
      | Global p -> parse_global ctx ann p
      | Function p -> parse_function ctx ann p ~toplevel
      | Class p -> parse_class ctx ann p ~toplevel
      | Declare p -> parse_declare ctx ann p ~toplevel ~jit
      | Type p -> parse_class ctx ann p ~toplevel ~is_type:true
      | Special p -> parse_special ctx ann p
      | Prefetch p -> parse_prefetch ctx ann p
    in
    finalize ~ctx stmt ann

  (** [parse_module ~ctx stmts] parses a module.
      A module is just a simple list [stmts] of statements. *)
  and parse_module ?(jit = false) ~(ctx : Codegen_ctx.t) stmts =
    let stmts =
      if jit
      then
        List.rev
        @@
        match List.rev stmts with
        | (ann, Expr e) :: tl -> (ann, Print ([ e ], "\n")) :: tl
        | l -> l
      else stmts
    in
    ignore @@ List.map stmts ~f:(parse ~ctx ~toplevel:true ~jit)

  (* ***************************************************************
     Node code generators
     ***************************************************************
     Each AST node is dispatched to its codegen function.
     Each codegen function [f] is called as [f context position data]
     where [data] is a tuple defined in [Ast_stmt] varying from node to node. *)
  and parse_pass _ _ _ = Llvm.Stmt.pass ()
  and parse_break _ _ _ = Llvm.Stmt.break ()
  and parse_continue _ _ _ = Llvm.Stmt.continue ()

  and parse_expr ctx ann expr =
    match snd expr with
    | Id "___dump___" ->
      Codegen_ctx.to_dbg_output ~ctx;
      Llvm.Stmt.pass ()
    | _ ->
      let expr = E.parse ~ctx expr in
      Llvm.Stmt.expr expr

  and parse_assign ctx ann ~toplevel ~jit (lhs, rhs, shadow, typ) =
    match lhs with
    | ann, Id var ->
      (match Hashtbl.find ctx.map var, shadow with
      | None, Update -> serr ~ann "%s not found" var
      | Some ((Codegen_ctx.Var _, { base; _ }) :: _), Update when ctx.env.base <> base ->
        serr ~ann "%s not found" var
      | Some ((Codegen_ctx.Var v, { base; global; toplevel; _ }) :: _), _
        when ctx.env.base = base ->
        if is_some typ
        then
          serr
            ~ann:(fst @@ Option.value_exn typ)
            "invalid type annotation (%s already defined)"
            var;
        if global && ctx.env.base = base && Stack.exists ctx.env.flags ~f:(( = ) "atomic")
        then (
          match shadow, snd rhs with
          | Update, _ -> Llvm.Stmt.expr @@ E.parse ~ctx rhs
          | _, Call ((_, Id bop), [ { value = _, Id v; _ }; { value = e; _ } ])
            when var = v && (bop = "min" || bop = "max") ->
            Llvm.Stmt.expr @@ E.parse ~ctx (fst rhs, Binary (lhs, "inplace_" ^ bop, e))
          | _ ->
            Llvm.Module.warn ~ann "atomic store %s" var;
            let rh_expr = E.parse ~ctx rhs in
            let s = Llvm.Stmt.assign v rh_expr in
            Llvm.Stmt.set_atomic_assign s;
            s)
        else Llvm.Stmt.assign v @@ E.parse ~ctx rhs
      | _ when jit && toplevel ->
        if is_some typ
        then serr ~ann:(fst @@ Option.value_exn typ) "invalid type annotation (JIT mode)";
        let rh_expr = E.parse ~ctx rhs in
        let v = Codegen_ctx.Var (Llvm.JIT.var ctx.env.mdl rh_expr) in
        Codegen_ctx.add ~ctx ~toplevel ~global:true var v;
        Llvm.Stmt.pass ()
      | r, _ ->
        (match r with
        | Some (_ :: _) -> Llvm.Module.warn ~ann "shadowing %s" var
        | _ -> ());
        let typ = Option.value_map typ ~f:(E.parse_type ~ctx) ~default:Ctypes.null in
        let rh_expr = E.parse ~ctx rhs in
        let var_stmt = Llvm.Stmt.var ~typ rh_expr in
        let v = Llvm.Var.stmt var_stmt in
        if toplevel then Llvm.Var.set_global v;
        Codegen_ctx.add ~ctx ~toplevel ~global:toplevel var (Codegen_ctx.Var v);
        var_stmt)
    | ann, Dot (lh_lhs, lh_rhs) ->
      (* a.x = b *)
      let rh_expr = E.parse ~ctx rhs in
      Llvm.Stmt.assign_member (E.parse ~ctx lh_lhs) lh_rhs rh_expr
    | ann, Index (var_expr, index_expr) ->
      (* a[x] = b *)
      let var_expr = E.parse ~ctx var_expr in
      let index_expr = E.parse ~ctx index_expr in
      let rh_expr = E.parse ~ctx rhs in
      Llvm.Stmt.assign_index var_expr index_expr rh_expr
    | _ -> serr ~ann "invalid assignment statement"

  and parse_declare ctx ann ~toplevel ~jit { name; typ } =
    if jit then serr ~ann "declarations not yet supported in JIT mode";
    match Hashtbl.find ctx.map name with
    | Some ((Codegen_ctx.Var _, { base; global; toplevel; _ }) :: _)
      when ctx.env.base = base ->
      serr ~ann:(fst @@ Option.value_exn typ) "%s already defined" name
    | r ->
      (match r with
      | Some (((Codegen_ctx.(Type _ | Func _ | Import _) as v), _) :: _) ->
        Llvm.Module.warn ~ann "shadowing %s" name
      | _ -> ());
      let typ = E.parse_type ~ctx @@ Option.value_exn typ in
      let var_stmt = Llvm.Stmt.var ~typ Ctypes.null in
      let v = Llvm.Var.stmt var_stmt in
      if toplevel then Llvm.Var.set_global v;
      Codegen_ctx.add ~ctx ~toplevel ~global:toplevel name (Codegen_ctx.Var v);
      var_stmt

  and parse_del ctx ann expr =
    match expr with
    | ann, Index (lhs, rhs) ->
      let lhs_expr = E.parse ~ctx lhs in
      let rhs_expr = E.parse ~ctx rhs in
      Llvm.Stmt.del_index lhs_expr rhs_expr
    | ann, Id var ->
      (match Ctx.in_scope ~ctx var with
      | Some (Codegen_ctx.Var v, _) ->
        Ctx.remove ~ctx var;
        Llvm.Stmt.del v
      | _ -> serr ~ann "cannot find %s" var)
    | _ -> serr ~ann "cannot delete a non-index expression"

  and parse_print ctx ann ~jit (exprs, ed) =
    let ll = if jit then Llvm.Stmt.print_jit else Llvm.Stmt.print in
    List.iteri exprs ~f:(fun i expr ->
        let expr = E.parse ~ctx expr in
        ignore @@ finalize ~ctx (ll expr) ann;
        if i + 1 < List.length exprs
        then ignore @@ finalize ~ctx (ll @@ E.parse ~ctx (ann, String " ")) ann);
    if ed <> "" then ll @@ E.parse ~ctx (ann, String ed) else Llvm.Stmt.pass ()

  and parse_return ctx ann ret =
    match ret with
    | None -> Llvm.Stmt.return Ctypes.null
    | Some ret ->
      let expr = E.parse ~ctx ret in
      let ret_stmt = Llvm.Stmt.return expr in
      Llvm.Func.set_return ctx.env.base ret_stmt;
      ret_stmt

  and parse_yield ctx ann ret =
    match ret with
    | None -> Llvm.Stmt.yield Ctypes.null
    | Some ret ->
      let expr = E.parse ~ctx ret in
      let yield_stmt = Llvm.Stmt.yield expr in
      Llvm.Func.set_yield ctx.env.base yield_stmt;
      yield_stmt

  and parse_assert ctx _ expr =
    let expr = E.parse ~ctx expr in
    Llvm.Stmt.assrt expr

  and parse_type_alias ctx ann ~toplevel (name, expr) =
    if is_some @@ Ctx.in_scope ~ctx name
    then Llvm.Module.warn ~ann "shadowing existing %s" name;
    let typ = E.parse_type ~ctx expr in
    Codegen_ctx.add ~ctx ~toplevel ~global:toplevel name (Codegen_ctx.Type typ);
    Llvm.Stmt.pass ()

  and parse_while ctx ann (cond, stmts) =
    let cond_expr = E.parse ~ctx cond in
    let while_stmt = Llvm.Stmt.while_loop cond_expr in
    let block = Llvm.Block.while_loop while_stmt in
    add_block { ctx with env = { ctx.env with block } } stmts;
    while_stmt

  (** [parse_for ?next context position data] parses for statement AST.
      `next` points to the nested for in the generator expression.  *)
  and parse_for ?next ~ctx pos (for_vars, gen_expr, stmts) =
    let gen_expr = E.parse ~ctx gen_expr in
    let for_stmt = Llvm.Stmt.loop gen_expr in
    let block = Llvm.Block.loop for_stmt in
    let for_ctx = { ctx with env = { ctx.env with block } } in
    Ctx.add_block ~ctx:for_ctx;
    let var = Llvm.Var.loop for_stmt in
    (match for_vars with
    | [ name ] -> Codegen_ctx.add ~ctx:for_ctx name (Codegen_ctx.Var var)
    | for_vars ->
      let var_expr = Llvm.Expr.var var in
      List.iteri for_vars ~f:(fun idx var_name ->
          let expr = Llvm.Expr.lookup var_expr (Llvm.Expr.int @@ Int64.of_int idx) in
          let var_stmt = Llvm.Stmt.var expr in
          ignore @@ finalize ~ctx:for_ctx var_stmt pos;
          let var = Llvm.Var.stmt var_stmt in
          Codegen_ctx.add ~ctx:for_ctx var_name (Codegen_ctx.Var var)));
    let _ =
      match next with
      | Some next -> next ctx for_ctx for_stmt
      | None ->
        ignore @@ List.map stmts ~f:(parse ~ctx:for_ctx ~toplevel:false ~jit:false)
    in
    Ctx.clear_block ~ctx:for_ctx;
    for_stmt

  and parse_if ctx ann cases =
    let if_stmt = Llvm.Stmt.cond () in
    List.iter cases ~f:(function { cond; cond_stmts } ->
        let block =
          match cond with
          | None -> Llvm.Block.elseb if_stmt
          | Some cond_expr ->
            let expr = E.parse ~ctx cond_expr in
            Llvm.Block.elseif if_stmt expr
        in
        add_block { ctx with env = { ctx.env with block } } cond_stmts);
    if_stmt

  and parse_match ctx ann (what, cases) =
    let what = E.parse ~ctx what in
    let match_stmt = Llvm.Stmt.matchs what in
    List.iter cases ~f:(fun { pattern; case_stmts } ->
        let pat, var =
          match pattern with
          | BoundPattern (name, pat) ->
            Ctx.add_block ~ctx;
            let pat = parse_pattern ctx ann pat in
            let pat = Llvm.Pattern.bound pat in
            Ctx.clear_block ~ctx;
            pat, Some (name, Llvm.Var.bound_pattern pat)
          | _ as pat ->
            Ctx.add_block ~ctx;
            let pat = parse_pattern ctx ann pat in
            Ctx.clear_block ~ctx;
            pat, None
        in
        let block = Llvm.Block.case match_stmt pat in
        add_block { ctx with env = { ctx.env with block } } case_stmts ~preprocess:(fun ctx ->
            match var with
            | Some (n, v) -> Codegen_ctx.add ~ctx n (Codegen_ctx.Var v)
            | None -> ()));
    match_stmt

  and parse_extern ctx
                   ann
                   ~toplevel
                   (lang, dylib, ctx_name, { name; typ }, args) =
    if lang <> "c" then serr ~ann "only cdef externs are currently supported";
    if is_some @@ Ctx.in_block ~ctx ctx_name
    then Llvm.Module.warn ~ann "shadowing %s" ctx_name;
    let names, types =
      List.map args ~f:(fun { name; typ } ->
          name, E.parse_type ~ctx (Option.value_exn typ))
      |> List.unzip
    in
    let fn = Llvm.Func.func name in
    Llvm.Func.set_args fn names types;
    Llvm.Func.set_extern fn;
    let typ = E.parse_type ~ctx (Option.value_exn typ) in
    Llvm.Func.set_type fn typ;
    let names = List.map args ~f:(fun x -> x.name) in
    Codegen_ctx.add ~ctx ~toplevel ~global:toplevel ctx_name (Codegen_ctx.Func (fn, names));
    Llvm.Stmt.func fn

  and parse_extend ctx ann ~toplevel (name, stmts) =
    if not toplevel then serr ~ann "extensions must be declared at the toplevel";
    let typ = E.parse_type ~ctx name in
    (* let typ = match Ctx.in_scope ctx name with
      | Some (Codegen_ctx.Type t, _) -> t
      | _ -> serr ~ann "cannot extend non-existing class %s" name
    in *)
    let new_ctx = { ctx with map = Hashtbl.copy ctx.map } in
    ignore
    @@ List.map stmts ~f:(function
           | pos, Function f -> parse_function new_ctx pos f ~cls:typ
           | pos, _ -> serr ~ann "type extensions can only specify functions");
    Llvm.Stmt.pass ()

  (** [parse_import ?ext context position data] parses import AST.
      Import file extension is set via [seq] (default is [".seq"]). *)
  and parse_import ctx ann ?(ext = ".seq") ~toplevel imports =
    let import (ctx : Codegen_ctx.t) file { from; what; import_as } =
      let vtable =
        match Hashtbl.find ctx.imported file with
        | Some t -> t
        | None ->
          let new_ctx = Codegen_ctx.init_empty ~ctx in
          if file = ctx.env.filename then
            serr ~ann "recursive import";
          Codegen_ctx.parse_file ~ctx:new_ctx file;
          let new_ctx = { new_ctx with map = Hashtbl.filteri new_ctx.map ~f:(fun ~key ~data ->
                match data with
                | [] -> false
                | (_, { global; internal; _ }) :: _ ->
                global && not internal) }
          in
          Hashtbl.set ctx.imported ~key:file ~data:new_ctx;
          Util.dbg "importing %s <%s>" file from;
          Codegen_ctx.to_dbg_output ~ctx:new_ctx;
          new_ctx
      in
      match what with
      | None ->
        (* import foo (as bar) *)
        let from = Option.value import_as ~default:from in
        Codegen_ctx.add ~ctx ~toplevel ~global:toplevel from (Codegen_ctx.Import file)
      | Some [ "*", None ] ->
        (* from foo import * *)
        Hashtbl.iteri vtable.map ~f:(fun ~key ~data ->
            match data with
            | (var, { global = true; internal = false; _ }) :: _ ->
              Util.dbg "[import] adding %s::%s" from key;
              Codegen_ctx.add ~ctx ~toplevel ~global:toplevel key var
            | _ -> ())
      | Some lst ->
        (* from foo import bar *)
        List.iter lst ~f:(fun (name, import_as) ->
            match Hashtbl.find vtable.map name with
            | Some ((var, ({ global = true; internal = false; _ } as ann)) :: _) ->
              let name = Option.value import_as ~default:name in
              Codegen_ctx.add ~ctx ~toplevel ~global:toplevel name var
            | _ -> serr ~ann "name %s not found in %s" name from)
    in
    List.iter imports ~f:(fun i ->
        let from = i.from in
        let file = sprintf "%s/%s%s" (Filename.dirname ctx.env.filename) from ext in
        let file =
          match Sys.file_exists file with
          | `Yes -> file
          | `No | `Unknown ->
            (match Util.get_from_stdlib ~ext from with
            | Some file -> file
            | None -> serr ~ann "cannot locate module %s" from)
        in
        import ctx file i);
    Llvm.Stmt.pass ()

  and parse_impaste ctx ann ?(ext = ".seq") from =
    match Util.get_from_stdlib ~ext from with
    | Some file ->
      Codegen_ctx.parse_file ~ctx file;
      Llvm.Stmt.pass ()
    | None -> serr ~ann "cannot locate module %s" from

  (** [parse_function ?cls context position data] parses function AST.
      Set `cls` to `Llvm.Types.typ` if you want a function to be
      a class `cls` method. *)
  and parse_function
      ctx
      ann
      ?cls
      ?(toplevel = false)
      { fn_name = { name; typ }; fn_generics; fn_args; fn_stmts; fn_attrs }
    =
    let fn =
      match cls with
      | Some cls ->
        let fn = Llvm.Func.func name in
        Llvm.Type.add_cls_method cls name fn;
        fn
      | None ->
        if is_some @@ Ctx.in_block ~ctx name
        then Llvm.Module.warn ~ann "shadowing %s" name;
        let fn = Llvm.Func.func name in
        let names = List.map fn_args ~f:(fun x -> x.name) in
        Codegen_ctx.add ~ctx ~toplevel ~global:toplevel name (Codegen_ctx.Func (fn, names));
        if not toplevel then Llvm.Func.set_enclosing fn ctx.env.base;
        fn
    in
    let flags = Stack.create () in
    List.iter fn_attrs ~f:(fun x ->
      if is_none cls
      then (
        let fnp = Option.value_exn (Ctx.in_scope ~ctx name) in
        Hash_set.add (snd fnp).attrs x;
        if x = "atomic" then Stack.push flags "atomic");
      Llvm.Func.set_attr fn x;
    );
    let new_ctx =
      { ctx with stack = Stack.create ()
      ; map = Hashtbl.copy ctx.map
      ; env = { ctx.env with base = fn; flags; block = Llvm.Block.func fn }
      }
    in
    Ctx.add_block ~ctx:new_ctx;
    let names, types =
      parse_generics
        new_ctx
        fn_generics
        fn_args
        (Llvm.Generics.Func.set_number fn)
        (fun idx name ->
          Llvm.Generics.Func.set_name fn idx name;
          Llvm.Generics.Func.get fn idx)
    in
    Llvm.Func.set_args fn names types;
    Option.value_map
      typ
      ~f:(fun typ -> Llvm.Func.set_type fn (E.parse_type ~ctx:new_ctx typ))
      ~default:();
    add_block new_ctx fn_stmts ~preprocess:(fun ctx ->
        List.iter names ~f:(fun name ->
            let var = Codegen_ctx.Var (Llvm.Func.get_arg fn name) in
            Codegen_ctx.add ~ctx name var));
    Llvm.Stmt.func fn

  and parse_class ctx ann ~toplevel ?(is_type = false) cls =
    (* ((name, types, args, stmts) as stmt) *)
    let typ =
      if is_some @@ Ctx.in_scope ~ctx cls.class_name
      then Llvm.Module.warn ~ann "shadowing %s" cls.class_name;
      let typ =
        if is_type
        then Llvm.Type.record [] [] cls.class_name
        else Llvm.Type.cls cls.class_name
      in
      Codegen_ctx.add ~ctx ~toplevel ~global:toplevel cls.class_name (Codegen_ctx.Type typ);
      typ
    in
    let new_ctx = { ctx with map = Hashtbl.copy ctx.map; stack = Stack.create () } in
    Ctx.add_block ~ctx:new_ctx;
    (match cls.args with
    | None when is_type -> serr ~ann "type definitions must have at least one member"
    | None -> ()
    | Some args ->
      List.iter args ~f:(fun { name; typ } ->
          if is_none typ then serr ~ann "class member %s needs type specification" name);
      let names, types =
        if is_type
        then
          List.unzip
          @@ List.map args ~f:(function
                 | { name; typ = None } ->
                   serr ~ann "type member %s needs type specification" name
                 | { name; typ = Some t } -> name, E.parse_type ~ctx t)
        else
          parse_generics
            new_ctx
            cls.generics
            args
            (Llvm.Generics.Type.set_number typ)
            (fun idx name ->
              Llvm.Generics.Type.set_name typ idx name;
              Llvm.Generics.Type.get typ idx)
      in
      if is_type
      then Llvm.Type.set_record_names typ names types
      else Llvm.Type.set_cls_args typ names types);
    ignore
    @@ List.map cls.members ~f:(function
           | pos, Function f -> parse_function new_ctx pos f ~cls:typ
           | _ -> serr ~ann "only function can be defined within a class definition");
    if not is_type then Llvm.Type.set_cls_done typ;
    Llvm.Stmt.pass ()

  and parse_try ctx ann (stmts, catches, finally) =
    let try_stmt = Llvm.Stmt.trycatch () in
    let block = Llvm.Block.try_block try_stmt in
    add_block { ctx with env = { ctx.env with block; trycatch = try_stmt } } stmts;
    List.iteri catches ~f:(fun idx { exc; var; stmts } ->
        let typ =
          match exc with
          | Some exc -> E.parse_type ~ctx (ann, Id exc)
          | None -> Ctypes.null
        in
        let block = Llvm.Block.catch try_stmt typ in
        add_block { ctx with env = { ctx.env with block } } stmts ~preprocess:(fun ctx ->
            Option.value_map
              var
              ~f:(fun var ->
                let v = Llvm.Var.catch try_stmt idx in
                Codegen_ctx.add ~ctx var (Codegen_ctx.Var v))
              ~default:()));
    Option.value_map
      finally
      ~f:(fun final ->
        let block = Llvm.Block.finally try_stmt in
        add_block { ctx with env = { ctx.env with block } } final)
      ~default:();
    try_stmt

  and parse_throw ctx _ expr =
    let expr = E.parse ~ctx expr in
    Llvm.Stmt.throw expr

  and parse_global ctx ann var =
    match Hashtbl.find ctx.map var with
    | Some ((Codegen_ctx.Var v, ({ internal = false; _ } as a)) :: rest) ->
      if not a.global
      then serr ~ann "only toplevel symbols can be set as a local global";
      if a.base <> ctx.env.base then Codegen_ctx.add ~ctx var ~global:true (Codegen_ctx.Var v);
      Llvm.Stmt.pass ()
    | _ -> serr ~ann "variable %s not found" var

  and parse_special ctx ann (kind, stmts, inputs) =
    ierr ~ann "not yet implemented (parse_special)"

  and parse_prefetch ctx ann pfs =
    let keys, wheres =
      List.unzip
      @@ List.map pfs ~f:(function
             | _, Index (e1, e2) ->
               let e1 = E.parse ~ctx e1 in
               let e2 = E.parse ~ctx e2 in
               e1, e2
             | _ -> serr ~ann "invalid prefetch expression (only a[b] allowed)")
    in
    let s = Llvm.Stmt.prefetch keys wheres in
    Llvm.Func.set_prefetch ctx.env.base s;
    s
    (* ***************************************************************
     Helper functions
     *************************************************************** *)

  (** [finalize ~add ~ctx statement position] finalizes [Llvm.Types.stmt]
      by setting its base to [ctx.base], its position to [position]
      and by adding the [statement] to the current block ([ctx.block])
      if [add] is [true]. Returns the finalized statement. *)
  and finalize ?(add = true) ~ctx stmt pos =
    Llvm.Stmt.set_base stmt ctx.env.base;
    Llvm.Stmt.set_pos stmt pos;
    if add then Llvm.Block.add_stmt ctx.env.block stmt;
    stmt

  (** [add_block ?preprocess context statements] creates a new block within the [context]
      and adds [statements] to that block.
      [preprocess context], if provided, is run after the block is created
      to initialize the block. *)
  and add_block ctx ?(preprocess = fun _ -> ()) stmts =
    Ctx.add_block ~ctx;
    preprocess ctx;
    ignore @@ List.map stmts ~f:(parse ~ctx ~toplevel:false);
    Ctx.clear_block ~ctx

  (** Helper for parsing match patterns *)
  and parse_pattern ctx ann = function
    | StarPattern -> Llvm.Pattern.star ()
    | BoundPattern _ -> serr ~ann "invalid bound pattern"
    | IntPattern i -> Llvm.Pattern.int i
    | BoolPattern b -> Llvm.Pattern.bool b
    | StrPattern s -> Llvm.Pattern.str s
    | SeqPattern s -> Llvm.Pattern.seq s
    | TuplePattern tl ->
      let tl = List.map tl ~f:(parse_pattern ctx ann) in
      Llvm.Pattern.record tl
    | RangePattern (i, j) -> Llvm.Pattern.range i j
    | ListPattern tl ->
      let tl = List.map tl ~f:(parse_pattern ctx ann) in
      Llvm.Pattern.array tl
    | OrPattern tl ->
      let tl = List.map tl ~f:(parse_pattern ctx ann) in
      Llvm.Pattern.orp tl
    | WildcardPattern wild ->
      let pat = Llvm.Pattern.wildcard () in
      if is_some wild
      then (
        let var = Llvm.Var.bound_pattern pat in
        Codegen_ctx.add ~ctx (Option.value_exn wild) (Codegen_ctx.Var var));
      pat
    | GuardedPattern (pat, expr) ->
      let pat = parse_pattern ctx ann pat in
      let expr = E.parse ~ctx expr in
      Llvm.Pattern.guarded pat expr

  (** Helper for parsing generic parameters.
      Parses generic parameters, assigns names to unnamed generics and calls C++ APIs to denote generic functions/classes.
      Also adds generics types to the context. *)
  and parse_generics ctx generic_types args set_generic_count get_generic =
    let names, types =
      List.map args ~f:(function
          | { name; typ = Some typ } -> name, typ
          | { name; typ = None } -> name, (Ast_ann.default, Ast.Expr.Id (sprintf "'%s" name)))
      |> List.unzip
    in
    let generic_args =
      List.filter_map types ~f:(fun x ->
          match snd x with
          | Id g when String.is_prefix g ~prefix:"'" -> Some g
          | _ -> None)
    in
    let generics = List.append generic_types generic_args |> List.dedup_and_sort ~compare in
    set_generic_count (List.length generics);
    List.iteri generics ~f:(fun cnt key ->
        Codegen_ctx.add ~ctx key (Codegen_ctx.Type (get_generic cnt key)));
    let types = List.map types ~f:(E.parse_type ~ctx) in
    names, types
end
