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
  let rec parse ?(toplevel = false) ?(jit = false) ~(ctx : Ctx.t) (pos, node) =
    let stmt =
      match node with
      | Break p -> parse_break ctx pos p
      | Continue p -> parse_continue ctx pos p
      | Expr p -> parse_expr ctx pos p
      | Assign p -> parse_assign ctx pos p ~toplevel ~jit
      | Del p -> parse_del ctx pos p
      | Print p -> parse_print ctx pos p ~jit
      | Return p -> parse_return ctx pos p
      | Yield p -> parse_yield ctx pos p
      | Assert p -> parse_assert ctx pos p
      | TypeAlias p -> parse_type_alias ctx pos p ~toplevel
      | If p -> parse_if ctx pos p
      | While p -> parse_while ctx pos p
      | For p -> parse_for ~ctx pos p
      | Match p -> parse_match ctx pos p
      | Extend p -> parse_extend ctx pos p ~toplevel
      | Import p -> parse_import ctx pos p ~toplevel
      | ImportPaste p -> parse_impaste ctx pos p
      | ImportExtern p -> parse_extern ctx pos p ~toplevel
      | Pass p -> parse_pass ctx pos p
      | Try p -> parse_try ctx pos p
      | Throw p -> parse_throw ctx pos p
      | Global p -> parse_global ctx pos p
      | Generic (Function p) -> parse_function ctx pos p ~toplevel
      | Generic (Class p) -> parse_class ctx pos p ~toplevel
      | Generic (Declare p) -> parse_declare ctx pos p ~toplevel ~jit
      | Generic (Type p) -> parse_class ctx pos p ~toplevel ~is_type:true
      | Special p -> parse_special ctx pos p
      | Prefetch p -> parse_prefetch ctx pos p
    in
    finalize ~ctx stmt pos

  (** [parse_module ~ctx stmts] parses a module.
      A module is just a simple list [stmts] of statements. *)
  and parse_module ?(jit = false) ~(ctx : Ctx.t) stmts =
    let stmts =
      if jit
      then
        List.rev
        @@
        match List.rev stmts with
        | (pos, Expr e) :: tl -> (pos, Print ([ e ], "\n")) :: tl
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

  and parse_expr ctx pos expr =
    match snd expr with
    | Id "___dump___" ->
      Ctx.to_dbg_output ctx;
      Llvm.Stmt.pass ()
    | _ ->
      let expr = E.parse ~ctx expr in
      Llvm.Stmt.expr expr

  and parse_assign ctx pos ~toplevel ~jit (lhs, rhs, shadow, typ) =
    match lhs with
    | pos, Id var ->
      (match jit && toplevel, Hashtbl.find ctx.map var, shadow with
      | _, None, Update -> serr ~pos "%s not found" var
      | false, Some ((Ctx_namespace.Var _, { base; global; _ }) :: _), Update
        when ctx.base <> base && not global ->
        serr ~pos "%s not found" var
      | _, Some ((Ctx_namespace.Var v, { base; global; toplevel; _ }) :: _), _
        when ctx.base = base || global ->
        if is_some typ
        then
          serr
            ~pos:(fst @@ Option.value_exn typ)
            "invalid type annotation (%s already defined)"
            var;
        if global && ctx.base = base && Stack.exists ctx.flags ~f:(( = ) "atomic")
        then (
          match shadow, snd rhs with
          | Update, _ -> Llvm.Stmt.expr @@ E.parse ~ctx rhs
          | _, Call ((_, Id bop), [ (_, { value = _, Id v; _ }); (_, { value = e; _ }) ])
            when var = v && (bop = "min" || bop = "max") ->
            Llvm.Stmt.expr @@ E.parse ~ctx (fst rhs, Binary (lhs, "inplace_" ^ bop, e))
          | _ ->
            let rh_expr = E.parse ~ctx rhs in
            let s = Llvm.Stmt.assign v rh_expr in
            Llvm.Stmt.set_atomic_assign s;
            s)
        else Llvm.Stmt.assign v @@ E.parse ~ctx rhs
      | true, _, _ ->
        if is_some typ
        then serr ~pos:(fst @@ Option.value_exn typ) "type annotations not supported in JIT mode";
        let rh_expr = E.parse ~ctx rhs in
        let v = Llvm.JIT.var ctx.mdl rh_expr in
        let var_expr = Llvm.Expr.var v in
        (* eprintf ">> jit_var %s := %s\n%!" (Ast.Expr.to_string lhs) (Ast.Expr.to_string rhs); *)
        (* finalize ~ctx stmt pos; *)
        Ctx.add ~ctx ~toplevel ~global:true var (Ctx_namespace.Var v);
        (* Llvm.Stmt.expr var_expr *)
        (* Llvm.Stmt.assign v rh_expr *)
        Llvm.Stmt.pass ()
        (* assign v @@ rh_expr *)
      | false, r, _ ->
        let typ = Option.value_map typ ~f:(E.parse_type ~ctx) ~default:Ctypes.null in
        let rh_expr = E.parse ~ctx rhs in
        let var_stmt = Llvm.Stmt.var ~typ rh_expr in
        let v = Llvm.Var.stmt var_stmt in
        if toplevel then Llvm.Var.set_global v;
        Ctx.add ~ctx ~toplevel ~global:toplevel var (Ctx_namespace.Var v);
        var_stmt)
    | pos, Dot (lh_lhs, lh_rhs) ->
      (* a.x = b *)
      let rh_expr = E.parse ~ctx rhs in
      Llvm.Stmt.assign_member (E.parse ~ctx lh_lhs) lh_rhs rh_expr
    | pos, Index (var_expr, index_expr) ->
      (* a[x] = b *)
      let var_expr = E.parse ~ctx var_expr in
      let index_expr = E.parse ~ctx index_expr in
      let rh_expr = E.parse ~ctx rhs in
      Llvm.Stmt.assign_index var_expr index_expr rh_expr
    | _ -> serr ~pos "invalid assignment statement"

  and parse_declare ctx pos ~toplevel ~jit { name; typ; _ } =
    if jit then serr ~pos "declarations not yet supported in JIT mode";
    match Hashtbl.find ctx.map name with
    | Some ((Ctx_namespace.Var _, { base; global; toplevel; _ }) :: _)
      when ctx.base = base ->
      serr ~pos:(fst @@ Option.value_exn typ) "%s already defined" name
    | r ->
      let typ = E.parse_type ~ctx @@ Option.value_exn typ in
      let var_stmt = Llvm.Stmt.var ~typ Ctypes.null in
      let v = Llvm.Var.stmt var_stmt in
      if toplevel then Llvm.Var.set_global v;
      Ctx.add ~ctx ~toplevel ~global:toplevel name (Ctx_namespace.Var v);
      var_stmt

  and parse_del ctx pos expr =
    match expr with
    | pos, Index (lhs, rhs) ->
      let lhs_expr = E.parse ~ctx lhs in
      let rhs_expr = E.parse ~ctx rhs in
      Llvm.Stmt.del_index lhs_expr rhs_expr
    | pos, Id var ->
      (match Ctx.in_scope ~ctx var with
      | Some (Ctx_namespace.Var v, _) ->
        Ctx.remove ctx var;
        Llvm.Stmt.del v
      | _ -> serr ~pos "cannot find %s" var)
    | _ -> serr ~pos "cannot delete a non-index expression"

  and parse_print ctx pos ~jit (exprs, ed) =
    let ll = if jit then Llvm.Stmt.print_jit else Llvm.Stmt.print in
    List.iteri exprs ~f:(fun i expr ->
        let expr = E.parse ~ctx expr in
        ignore @@ finalize ~ctx (ll expr) pos;
        if i + 1 < List.length exprs
        then ignore @@ finalize ~ctx (ll @@ E.parse ~ctx (pos, String " ")) pos);
    if ed <> "" then ll @@ E.parse ~ctx (pos, String ed) else Llvm.Stmt.pass ()

  and parse_return ctx pos ret =
    match ret with
    | None -> Llvm.Stmt.return Ctypes.null
    | Some ret ->
      let expr = E.parse ~ctx ret in
      let ret_stmt = Llvm.Stmt.return expr in
      Llvm.Func.set_return ctx.base ret_stmt;
      ret_stmt

  and parse_yield ctx pos ret =
    match ret with
    | None -> Llvm.Stmt.yield Ctypes.null
    | Some ret ->
      let expr = E.parse ~ctx ret in
      let yield_stmt = Llvm.Stmt.yield expr in
      Llvm.Func.set_yield ctx.base yield_stmt;
      yield_stmt

  and parse_assert ctx _ expr =
    let expr = E.parse ~ctx expr in
    Llvm.Stmt.assrt expr

  and parse_type_alias ctx pos ~toplevel (name, expr) =
    let typ = E.parse_type ~ctx expr in
    Ctx.add ~ctx ~toplevel ~global:toplevel name (Ctx_namespace.Type typ);
    Llvm.Stmt.pass ()

  and parse_while ctx pos (cond, stmts) =
    let cond_expr = E.parse ~ctx cond in
    let while_stmt = Llvm.Stmt.while_loop cond_expr in
    let block = Llvm.Block.while_loop while_stmt in
    add_block { ctx with block } stmts;
    while_stmt

  (** [parse_for ?next context position data] parses for statement AST.
      `next` points to the nested for in the generator expression.  *)
  and parse_for ?next ~ctx pos (for_vars, gen_expr, stmts) =
    let gen_expr = E.parse ~ctx gen_expr in
    let for_stmt = Llvm.Stmt.loop gen_expr in
    let block = Llvm.Block.loop for_stmt in
    let for_ctx = { ctx with block } in
    Ctx.add_block for_ctx;
    let var = Llvm.Var.loop for_stmt in
    (match for_vars with
    | [ name ] -> Ctx.add ~ctx:for_ctx name (Ctx_namespace.Var var)
    | for_vars ->
      let var_expr = Llvm.Expr.var var in
      List.iteri for_vars ~f:(fun idx var_name ->
          let expr = Llvm.Expr.lookup var_expr (Llvm.Expr.int @@ Int64.of_int idx) in
          let var_stmt = Llvm.Stmt.var expr in
          ignore @@ finalize ~ctx:for_ctx var_stmt pos;
          let var = Llvm.Var.stmt var_stmt in
          Ctx.add ~ctx:for_ctx var_name (Ctx_namespace.Var var)));
    let _ =
      match next with
      | Some next -> next ctx for_ctx for_stmt
      | None ->
        ignore @@ List.map stmts ~f:(parse ~ctx:for_ctx ~toplevel:false ~jit:false)
    in
    Ctx.clear_block for_ctx;
    for_stmt

  and parse_if ctx pos cases =
    let if_stmt = Llvm.Stmt.cond () in
    List.iter cases ~f:(function _, { cond; cond_stmts } ->
        let block =
          match cond with
          | None -> Llvm.Block.elseb if_stmt
          | Some cond_expr ->
            let expr = E.parse ~ctx cond_expr in
            Llvm.Block.elseif if_stmt expr
        in
        add_block { ctx with block } cond_stmts);
    if_stmt

  and parse_match ctx pos (what, cases) =
    let what = E.parse ~ctx what in
    let match_stmt = Llvm.Stmt.matchs what in
    List.iter cases ~f:(fun (_, { pattern; case_stmts }) ->
        let pat, var =
          match pattern with
          | BoundPattern (name, pat) ->
            Ctx.add_block ctx;
            let pat = parse_pattern ctx pos pat in
            let pat = Llvm.Pattern.bound pat in
            Ctx.clear_block ctx;
            pat, Some (name, Llvm.Var.bound_pattern pat)
          | _ as pat ->
            Ctx.add_block ctx;
            let pat = parse_pattern ctx pos pat in
            Ctx.clear_block ctx;
            pat, None
        in
        let block = Llvm.Block.case match_stmt pat in
        add_block { ctx with block } case_stmts ~preprocess:(fun ctx ->
            match var with
            | Some (n, v) -> Ctx.add ~ctx n (Ctx_namespace.Var v)
            | None -> ()));
    match_stmt

  and parse_extern ctx pos ~toplevel ext =
    if ext.lang <> "c" then serr ~pos "only cdef externs are currently supported";
    let ctx_name = Option.value ext.e_as ~default:ext.e_name.name in
    match ext.e_from with
    | Some from ->
      let params = pos, Tuple
        ((Option.value_exn ext.e_name.typ) :: (List.map ext.e_args ~f:(fun (_, t) -> Option.value_exn t.typ)))
      in
      let rhs =
        pos, Call
          ( (pos, Index ((pos, Id "function"), params))
          , [ pos,
              { name = None
              ; value = pos, Call
                ( (pos, Dot ((pos, Id "C"), "dlsym"))
                , [ pos
                  , { name = None; value = pos, Call
                      ( (pos, Dot ((pos, Id "C"), "dlopen"))
                      , [ pos, { name = None; value = from } ]
                      )
                    }
                  ; pos, { name = None; value = pos, String ext.e_name.name }
                  ]
                )
              }
            ]
          )
      in
      Util.dbg "::: %s = %s" (ctx_name) (Ast.Expr.to_string rhs);
      parse_assign ctx pos ~toplevel ~jit:false
        ((pos, Id ctx_name), rhs, Shadow, None)
    | None ->
      let names, types =
        List.map ext.e_args ~f:(fun (_, { name; typ; _ }) ->
            name, E.parse_type ~ctx (Option.value_exn typ))
        |> List.unzip
      in
      let fn = Llvm.Func.func ext.e_name.name in
      Llvm.Func.set_args fn names types;
      Llvm.Func.set_extern fn;
      let typ = E.parse_type ~ctx (Option.value_exn ext.e_name.typ) in
      Llvm.Func.set_type fn typ;
      let names = List.map ext.e_args ~f:(fun (_, x) -> x.name) in
      Ctx.add ~ctx ~toplevel ~global:toplevel ctx_name (Ctx_namespace.Func (fn, names));
      Llvm.Stmt.func fn

  and parse_extend ctx pos ~toplevel (name, stmts) =
    if not toplevel then serr ~pos "extensions must be declared at the toplevel";
    let typ = E.parse_type ~ctx name in
    (* let typ = match Ctx.in_scope ctx name with
      | Some (Ctx_namespace.Type t, _) -> t
      | _ -> serr ~pos "cannot extend non-existing class %s" name
    in *)
    let new_ctx = { ctx with map = Hashtbl.copy ctx.map } in
    ignore
    @@ List.map stmts ~f:(function
           | pos, Function f -> parse_function new_ctx pos f ~cls:typ
           | pos, _ -> serr ~pos "type extensions can only specify functions");
    Llvm.Stmt.pass ()

  (** [parse_import ?ext context position data] parses import AST.
      Import file extension is set via [seq] (default is [".seq"]). *)
  and parse_import ctx pos ?(ext = ".seq") ~toplevel imports =
    let import (ctx : Ctx.t) file { from; what; import_as } =
      let from = snd from in
      let vtable =
        match Hashtbl.find ctx.imported file with
        | Some t -> t
        | None ->
          let new_ctx = Ctx.init_empty ctx in
          if file = ctx.filename then
            serr ~pos "recursive import";
          Ctx.parse_file ~ctx:new_ctx file;
          let map =
            Hashtbl.filteri new_ctx.map ~f:(fun ~key ~data ->
                match data with
                | [] -> false
                | (_, { global; internal; _ }) :: _ -> global && not internal)
          in
          Hashtbl.set ctx.imported ~key:file ~data:map;
          Util.dbg "importing %s <%s>" file from;
          Ctx.to_dbg_output { new_ctx with map };
          map
      in
      match what with
      | None ->
        (* import foo (as bar) *)
        let from = Option.value import_as ~default:from in
        let map =
          Hashtbl.filteri vtable ~f:(fun ~key ~data ->
              match data with
              | [] -> false
              | (_, { global; internal; _ }) :: _ -> global && not internal)
        in
        Ctx.add ~ctx ~toplevel ~global:toplevel from (Ctx_namespace.Import map)
      | Some [ (_, ("*", None)) ] ->
        (* from foo import * *)
        Hashtbl.iteri vtable ~f:(fun ~key ~data ->
            match data with
            | (var, { global = true; internal = false; _ }) :: _ ->
              Util.dbg "[import] adding %s::%s" from key;
              Ctx.add ~ctx ~toplevel ~global:toplevel key var
            | _ -> ())
      | Some lst ->
        (* from foo import bar *)
        List.iter lst ~f:(fun (pos, (name, import_as)) ->
            match Hashtbl.find vtable name with
            | Some ((var, ({ global = true; internal = false; _ } as ann)) :: _) ->
              let name = Option.value import_as ~default:name in
              Ctx.add ~ctx ~toplevel ~global:toplevel name var
            | _ -> serr ~pos "name %s not found in %s" name from)
    in
    List.iter imports ~f:(fun i ->
        let from = snd i.from in
        let file = sprintf "%s/%s%s" (Filename.dirname ctx.filename) from ext in
        let file =
          match Sys.file_exists file with
          | `Yes -> file
          | `No | `Unknown ->
            (match Util.get_from_stdlib ~ext from with
            | Some file -> file
            | None -> serr ~pos "cannot locate module %s" from)
        in
        import ctx file i);
    Llvm.Stmt.pass ()

  and parse_impaste ctx pos ?(ext = ".seq") from =
    match Util.get_from_stdlib ~ext from with
    | Some file ->
      Ctx.parse_file ~ctx file;
      Llvm.Stmt.pass ()
    | None -> serr ~pos "cannot locate module %s" from

  (** [parse_function ?cls context position data] parses function AST.
      Set `cls` to `Llvm.Types.typ` if you want a function to be
      a class `cls` method. *)
  and parse_function
      ctx
      pos
      ?cls
      ?(toplevel = false)
      { fn_name = { name; typ; _ }; fn_generics; fn_args; fn_stmts; fn_attrs }
    =
    let fn =
      match cls with
      | Some cls ->
        let fn = Llvm.Func.func name in
        Llvm.Type.add_cls_method cls name fn;
        fn
      | None ->
        let fn = Llvm.Func.func name in
        let names = List.map fn_args ~f:(fun (_, x) -> x.name) in
        Ctx.add ~ctx ~toplevel ~global:toplevel name (Ctx_namespace.Func (fn, names));
        if not toplevel then Llvm.Func.set_enclosing fn ctx.base;
        fn
    in
    let flags = Stack.create () in
    List.iter fn_attrs ~f:(fun (_, x) ->
      if is_none cls
      then (
        let fnp = Option.value_exn (Ctx.in_scope ~ctx name) in
        Hash_set.add (snd fnp).attrs x;
        if x = "atomic" then Stack.push flags "atomic");
      Llvm.Func.set_attr fn x;
    );
    let new_ctx =
      { ctx with
        base = fn
      ; stack = Stack.create ()
      ; flags
      ; block = Llvm.Block.func fn
      ; map = Hashtbl.copy ctx.map
      }
    in
    Ctx.add_block new_ctx;
    let names, types, defaults =
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
    Llvm.Func.set_defaults fn defaults;
    Option.value_map
      typ
      ~f:(fun typ -> Llvm.Func.set_type fn (E.parse_type ~ctx:new_ctx typ))
      ~default:();
    add_block new_ctx fn_stmts ~preprocess:(fun ctx ->
        List.iter names ~f:(fun name ->
            let var = Ctx_namespace.Var (Llvm.Func.get_arg fn name) in
            Ctx.add ~ctx name var));
    Llvm.Stmt.func fn

  and parse_class ctx pos ~toplevel ?(is_type = false) cls =
    (* ((name, types, args, stmts) as stmt) *)
    let typ =
      let typ =
        if is_type
        then Llvm.Type.record [] [] cls.class_name
        else Llvm.Type.cls cls.class_name
      in
      Ctx.add ~ctx ~toplevel ~global:toplevel cls.class_name (Ctx_namespace.Type typ);
      typ
    in
    let new_ctx = { ctx with map = Hashtbl.copy ctx.map; stack = Stack.create () } in
    Ctx.add_block new_ctx;
    let args = match cls.args with
      | None when is_type -> serr ~pos "type definitions must have at least one member"
      | None -> []
      | Some args -> args
    in
    List.iter args ~f:(fun (pos, { name; typ; _ }) ->
        if is_none typ then serr ~pos "class member %s needs type specification" name);
    let names, types, _ =
      if is_type
      then
        List.unzip3
        @@ List.map args ~f:(function
                | pos, { name; typ = None; _ } ->
                  serr ~pos "type member %s needs type specification" name
                | _, { name; typ = Some t; _ } -> name, E.parse_type ~ctx t, Ctypes.null)
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
    else Llvm.Type.set_cls_args typ names types;
    ignore
    @@ List.map cls.members ~f:(function
           | pos, Function f -> parse_function new_ctx pos f ~cls:typ
           | _ -> serr ~pos "only function can be defined within a class definition");
    if not is_type then Llvm.Type.set_cls_done typ;
    Llvm.Stmt.pass ()

  and parse_try ctx pos (stmts, catches, finally) =
    let try_stmt = Llvm.Stmt.trycatch () in
    let block = Llvm.Block.try_block try_stmt in
    add_block { ctx with block; trycatch = try_stmt } stmts;
    List.iteri catches ~f:(fun idx (pos, { exc; var; stmts }) ->
        let typ =
          match exc with
          | Some exc -> E.parse_type ~ctx (pos, Id exc)
          | None -> Ctypes.null
        in
        let block = Llvm.Block.catch try_stmt typ in
        add_block { ctx with block } stmts ~preprocess:(fun ctx ->
            Option.value_map
              var
              ~f:(fun var ->
                let v = Llvm.Var.catch try_stmt idx in
                Ctx.add ~ctx var (Ctx_namespace.Var v))
              ~default:()));
    Option.value_map
      finally
      ~f:(fun final ->
        let block = Llvm.Block.finally try_stmt in
        add_block { ctx with block } final)
      ~default:();
    try_stmt

  and parse_throw ctx _ expr =
    let expr = E.parse ~ctx expr in
    Llvm.Stmt.throw expr

  and parse_global ctx pos var =
    match Hashtbl.find ctx.map var with
    | Some ((Ctx_namespace.Var v, ({ internal = false; _ } as ann)) :: rest) ->
      if not ann.global
      then serr ~pos "only toplevel symbols can be set as a local global";
      if ann.base <> ctx.base then Ctx.add ~ctx var ~global:true (Ctx_namespace.Var v);
      Llvm.Stmt.pass ()
    | _ -> serr ~pos "variable %s not found" var

  and parse_special ctx pos (kind, stmts, inputs) =
    ierr ~pos "not yet implemented (parse_special)"

  and parse_prefetch ctx pos pfs =
    let keys, wheres =
      List.unzip
      @@ List.map pfs ~f:(function
             | _, Index (e1, e2) ->
               let e1 = E.parse ~ctx e1 in
               let e2 = E.parse ~ctx e2 in
               e1, e2
             | _ -> serr ~pos "invalid prefetch expression (only a[b] allowed)")
    in
    let s = Llvm.Stmt.prefetch keys wheres in
    Llvm.Func.set_prefetch ctx.base s;
    s
    (* ***************************************************************
     Helper functions
     *************************************************************** *)

  (** [finalize ~add ~ctx statement position] finalizes [Llvm.Types.stmt]
      by setting its base to [ctx.base], its position to [position]
      and by adding the [statement] to the current block ([ctx.block])
      if [add] is [true]. Returns the finalized statement. *)
  and finalize ?(add = true) ~ctx stmt pos =
    Llvm.Stmt.set_base stmt ctx.base;
    Llvm.Stmt.set_pos stmt pos;
    if add then Llvm.Block.add_stmt ctx.block stmt;
    stmt

  (** [add_block ?preprocess context statements] creates a new block within the [context]
      and adds [statements] to that block.
      [preprocess context], if provided, is run after the block is created
      to initialize the block. *)
  and add_block ctx ?(preprocess = fun _ -> ()) stmts =
    Ctx.add_block ctx;
    preprocess ctx;
    ignore @@ List.map stmts ~f:(parse ~ctx ~toplevel:false);
    Ctx.clear_block ctx

  (** Helper for parsing match patterns *)
  and parse_pattern ctx pos = function
    | StarPattern -> Llvm.Pattern.star ()
    | BoundPattern _ -> serr ~pos "invalid bound pattern"
    | IntPattern i -> Llvm.Pattern.int i
    | BoolPattern b -> Llvm.Pattern.bool b
    | StrPattern s -> Llvm.Pattern.str s
    | SeqPattern s -> Llvm.Pattern.seq s
    | TuplePattern tl ->
      let tl = List.map tl ~f:(parse_pattern ctx pos) in
      Llvm.Pattern.record tl
    | RangePattern (i, j) -> Llvm.Pattern.range i j
    | ListPattern tl ->
      let tl = List.map tl ~f:(parse_pattern ctx pos) in
      Llvm.Pattern.array tl
    | OrPattern tl ->
      let tl = List.map tl ~f:(parse_pattern ctx pos) in
      Llvm.Pattern.orp tl
    | WildcardPattern wild ->
      let pat = Llvm.Pattern.wildcard () in
      if is_some wild
      then (
        let var = Llvm.Var.bound_pattern pat in
        Ctx.add ~ctx (Option.value_exn wild) (Ctx_namespace.Var var));
      pat
    | GuardedPattern (pat, expr) ->
      let pat = parse_pattern ctx pos pat in
      let expr = E.parse ~ctx expr in
      Llvm.Pattern.guarded pat expr

  (** Helper for parsing generic parameters.
      Parses generic parameters, assigns names to unnamed generics and calls C++ APIs to denote generic functions/classes.
      Also adds generics types to the context. *)
  and parse_generics ctx generic_types args set_generic_count get_generic =
    let names_seen = String.Hash_set.create () in
    let defaults_started = ref false in
    let names, types, defaults =
      List.map args ~f:(fun (pos, { name; typ; default }) ->
          let typ = match typ with
            | Some typ -> typ
            | None -> pos, Ast.Expr.Id (sprintf "'%s" name)
          in
          if Hash_set.mem names_seen name then
            serr ~pos "argument %s already specified" name;
          Hash_set.add names_seen name;
          (match default with
            | Some x -> defaults_started := true
            | None when !defaults_started -> serr ~pos "cannot have argument without default value here"
            | None -> ());
          name, typ, default
        )
      |> List.unzip3
    in
    (* Util.dbg "== generics: %s" @@ Util.ppl generic_types ~f:snd ; *)
    let type_args = List.map generic_types ~f:snd in
    let generic_args =
      List.filter_map types ~f:(fun x ->
          match snd x with
          | Id g when String.is_prefix g ~prefix:"'" -> Some g
          | _ -> None)
    in
    let generics = List.append type_args generic_args |> List.dedup_and_sort ~compare in
    set_generic_count (List.length generics);
    List.iteri generics ~f:(fun cnt key ->
        Util.dbg "adding %s ..." key;
        Ctx.add ~ctx key (Ctx_namespace.Type (get_generic cnt key)));
    let types = List.map types ~f:(E.parse_type ~ctx) in
    let defaults = List.map defaults ~f:(function
      | None -> Ctypes.null
      | Some expr -> E.parse ~ctx expr) in
    names, types, defaults
end
