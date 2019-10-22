(* ****************************************************************************
 * Seqaml.Codegen_stmt: Generate Seq code from statement AST
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core

open Ast.Expr
open Ast.Stmt

module C = Codegen_ctx
module E = Codegen_expr

(* ***************************************************************
    Public interface
    *************************************************************** *)

(** [parse ~ctx expr] dispatches a statement AST node [expr] to the proper code generation function. *)
let rec parse ~(ctx : C.t) ?(toplevel=false) ?(jit = false) (ann, node) =
  C.push_ann ~ctx ann;
  let stmt =
    match node with
    | Break p -> parse_break ctx p
    | Continue p -> parse_continue ctx p
    | Expr p -> parse_expr ctx p
    | Assign p -> parse_assign ctx p ~toplevel ~jit
    | Del p -> parse_del ctx p
    | Print p -> parse_print ctx p ~jit
    | Return p -> parse_return ctx p
    | Yield p -> parse_yield ctx p
    | Assert p -> parse_assert ctx p
    | TypeAlias p -> parse_type_alias ctx p ~toplevel
    | If p -> parse_if ctx p
    | While p -> parse_while ctx p
    | For p -> parse_for ~ctx ann p
    | Match p -> parse_match ctx p
    | Import p -> parse_import ctx p ~toplevel
    | ImportPaste p -> parse_impaste ctx p
    | Pass p -> parse_pass ctx p
    | Try p -> parse_try ctx p
    | Throw p -> parse_throw ctx p
    | Global p -> parse_global ctx p
    | Prefetch p -> parse_prefetch ctx p
    (* | Function p -> parse_function ctx p ~toplevel *)
    (* | Class p -> parse_class ctx p ~toplevel *)
    (* | Type p -> parse_class ctx p ~toplevel ~is_type:true *)
    (* | Declare p -> parse_declare ctx p ~toplevel ~jit *)
    (* | Special p -> parse_special ctx p *)
    | _ -> C.err ~ctx "not yet!"
  in
  C.pop_ann ~ctx |> ignore;
  Llvm.Stmt.set_base stmt ctx.env.base;
  Llvm.Stmt.set_pos stmt ann;
  Llvm.Block.add_stmt ctx.env.block stmt;
  stmt

(** [parse_module ~ctx stmts] parses a module.
    A module is just a simple list [stmts] of statements. *)
and parse_module ?(jit = false) ~(ctx : C.t) stmts =
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
and parse_pass _ _ = Llvm.Stmt.pass ()
and parse_break _ _ = Llvm.Stmt.break ()
and parse_continue _ _ = Llvm.Stmt.continue ()

and parse_expr ctx expr =
  let expr = E.parse ~ctx expr in
  Llvm.Stmt.expr expr

and parse_assign ctx ~toplevel ~jit (lhs, rhs, _, _) =
  match lhs, rhs with
  | [ _, Id var ], [ rhs ] when jit && toplevel ->
    let v = Llvm.JIT.var ctx.env.mdl (E.parse ~ctx rhs) in
    C.add ~ctx ~toplevel ~global:true var (C.Var v);
    Llvm.Stmt.pass ()
  | [ _, Id var ], [ rhs ] ->
    (match Ctx.in_scope ~ctx var with
    | Some (C.Var v, vann) when C.is_accessible ~ctx vann ->
      Llvm.Stmt.assign v (E.parse ~ctx rhs)
    | _ ->
      let var_stmt = Llvm.Stmt.var (E.parse ~ctx rhs) in
      let v = Llvm.Var.stmt var_stmt in
      if toplevel then Llvm.Var.set_global v;
      C.add ~ctx ~toplevel ~global:toplevel var (C.Var v);
      var_stmt)
  | [ _, Dot (lh_lhs, lh_rhs) ], [ rhs ] ->
    Llvm.Stmt.assign_member (E.parse ~ctx lh_lhs) lh_rhs (E.parse ~ctx rhs)
  | [ _, Index (var_expr, [index_expr]) ], [rhs] ->
    Llvm.Stmt.assign_index (E.parse ~ctx var_expr) (E.parse ~ctx index_expr) (E.parse ~ctx rhs)
  | _ -> C.err ~ctx "invalid assignment statement"

and parse_del ctx expr =
  match snd expr with
  | Id var ->
    (match Ctx.in_scope ~ctx var with
    | Some (C.Var v, vann) when C.is_accessible ~ctx vann ->
      (* TODO: test this carefully *)
      Ctx.remove ~ctx var;
      Llvm.Stmt.del v
    | _ -> C.err ~ctx "cannot find %s" var)
  | _ -> C.err ~ctx "bad del expression"

and parse_print ctx ~jit (exprs, _) =
  let call = if jit then Llvm.Stmt.print_jit else Llvm.Stmt.print in
  call (E.parse ~ctx (List.hd_exn exprs))

and parse_return ctx ret =
  match ret with
  | None -> Llvm.Stmt.return Ctypes.null
  | Some ret ->
    let s = Llvm.Stmt.return (E.parse ~ctx ret) in
    (* TODO: get rid of this *)
    Llvm.Func.set_return ctx.env.base s;
    s

and parse_yield ctx ret =
  match ret with
  | None -> Llvm.Stmt.yield Ctypes.null
  | Some ret ->
    let s = Llvm.Stmt.yield (E.parse ~ctx ret) in
    Llvm.Func.set_yield ctx.env.base s;
    s

and parse_assert ctx expr =
  Llvm.Stmt.assrt (E.parse ~ctx expr)

and parse_type_alias ctx ~toplevel (name, expr) =
  let expr = E.parse ~ctx expr in
  if Llvm.Expr.is_type expr
  then (
    C.add ~ctx ~toplevel ~global:toplevel name (C.Type (Llvm.Type.expr_type expr));
    Llvm.Stmt.pass ())
  else C.err ~ctx "must refer to type"

and parse_while ctx (cond, stmts) =
  let cond_expr = E.parse ~ctx cond in
  let while_stmt = Llvm.Stmt.while_loop cond_expr in
  let block = Llvm.Block.while_loop while_stmt in
  add_block { ctx with env = { ctx.env with block } } stmts;
  while_stmt

and parse_for ~ctx pos (for_vars, gen_expr, stmts) =
  let for_stmt = Llvm.Stmt.loop (E.parse ~ctx gen_expr) in
  let block = Llvm.Block.loop for_stmt in
  add_block { ctx with env = { ctx.env with block } } stmts
    ~preprocess:(fun ctx ->
      let var = Llvm.Var.loop for_stmt in
      C.add ~ctx (List.hd_exn for_vars) (C.Var var));
  for_stmt

and parse_if ctx cases =
  let if_stmt = Llvm.Stmt.cond () in
  List.iter cases ~f:(function { cond; cond_stmts } ->
      let block = match cond with
        | None -> Llvm.Block.elseb if_stmt
        | Some cond -> Llvm.Block.elseif if_stmt (E.parse ~ctx cond)
      in
      add_block { ctx with env = { ctx.env with block } } cond_stmts);
  if_stmt

and parse_match ctx (what, cases) =
  let what = E.parse ~ctx what in
  let match_stmt = Llvm.Stmt.matchs what in
  List.iter cases ~f:(fun { pattern; case_stmts } ->
      let pat, var =
        match pattern with
        | BoundPattern (name, pat) ->
          Ctx.add_block ~ctx;
          let pat = parse_pattern ctx pat in
          let pat = Llvm.Pattern.bound pat in
          Ctx.clear_block ~ctx;
          pat, Some (name, Llvm.Var.bound_pattern pat)
        | _ as pat ->
          Ctx.add_block ~ctx;
          let pat = parse_pattern ctx pat in
          Ctx.clear_block ~ctx;
          pat, None
      in
      let block = Llvm.Block.case match_stmt pat in
      add_block
        { ctx with env = { ctx.env with block } }
        case_stmts
        ~preprocess:(fun ctx ->
          match var with
          | Some (n, v) -> C.add ~ctx n (C.Var v)
          | None -> ()));
  match_stmt

and parse_pattern ctx = function
  | BoundPattern _ -> C.err ~ctx "invalid bound pattern"
  | StarPattern -> Llvm.Pattern.star ()
  | IntPattern i -> Llvm.Pattern.int i
  | BoolPattern b -> Llvm.Pattern.bool b
  | StrPattern s -> Llvm.Pattern.str s
  | SeqPattern s -> Llvm.Pattern.seq s
  | TuplePattern tl -> Llvm.Pattern.record (List.map tl ~f:(parse_pattern ctx))
  | RangePattern (i, j) -> Llvm.Pattern.range i j
  | ListPattern tl -> Llvm.Pattern.array (List.map tl ~f:(parse_pattern ctx))
  | OrPattern tl -> Llvm.Pattern.orp (List.map tl ~f:(parse_pattern ctx))
  | WildcardPattern None -> Llvm.Pattern.wildcard ()
  | WildcardPattern (Some wild) ->
    let pat = Llvm.Pattern.wildcard () in
    C.add ~ctx wild (C.Var (Llvm.Var.bound_pattern pat));
    pat
  | GuardedPattern (pat, expr) ->
    let pat = parse_pattern ctx pat in
    Llvm.Pattern.guarded pat (E.parse ~ctx expr)

(** [parse_import ?ext context position data] parses import AST.
    Import file extension is set via [seq] (default is [".seq"]). *)
and parse_import ctx ?(ext = ".seq") ~toplevel imports =
  let import (ctx : C.t) file { from; what; import_as } =
    let vtable =
      match Hashtbl.find ctx.globals.imported file with
      | Some t -> t
      | None ->
        let new_ctx = C.init_empty ~ctx in
        if file = ctx.env.filename then C.err ~ctx "recursive import";
        In_channel.read_lines file
          |> String.concat ~sep:"\n"
          |> Codegen.parse ~file:(Filename.realpath file)
          |> List.map ~f:(parse ~ctx:new_ctx ~toplevel:true)
          |> ignore;
        let new_ctx =
          { new_ctx with
            map =
              Hashtbl.filteri new_ctx.map ~f:(fun ~key ~data ->
                  match data with
                  | [] -> false
                  | (_, { global; internal; _ }) :: _ -> global && not internal)
          }
        in
        Hashtbl.set ctx.globals.imported ~key:file ~data:new_ctx;
        Util.dbg "importing %s <%s>" file from;
        C.to_dbg_output ~ctx:new_ctx;
        new_ctx
    in
    match what with
    | None ->
      (* import foo (as bar) *)
      let from = Option.value import_as ~default:from in
      C.add ~ctx ~toplevel ~global:toplevel from (C.Import file)
    | Some [ ("*", None) ] ->
      (* from foo import * *)
      Hashtbl.iteri vtable.map ~f:(fun ~key ~data ->
          match data with
          | (var, { global = true; internal = false; _ }) :: _ ->
            Util.dbg "[import] adding %s::%s" from key;
            C.add ~ctx ~toplevel ~global:toplevel key var
          | _ -> ())
    | Some lst ->
      (* from foo import bar *)
      List.iter lst ~f:(fun (name, import_as) ->
          match Hashtbl.find vtable.map name with
          | Some ((var, ({ global = true; internal = false; _ } as ann)) :: _) ->
            let name = Option.value import_as ~default:name in
            C.add ~ctx ~toplevel ~global:toplevel name var
          | _ -> C.err ~ctx "name %s not found in %s" name from)
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
          | None -> C.err ~ctx "cannot locate module %s" from)
      in
      import ctx file i);
  Llvm.Stmt.pass ()

and parse_impaste ctx ?(ext = ".seq") from =
  match Util.get_from_stdlib ~ext from with
  | Some file ->
    In_channel.read_lines file
      |> String.concat ~sep:"\n"
      |> Codegen.parse ~file:(Filename.realpath file)
      |> List.map ~f:(parse ~ctx ~toplevel:true)
      |> ignore;
    Llvm.Stmt.pass ()
  | None -> C.err ~ctx "cannot locate module %s" from

and parse_try ctx (stmts, catches, finally) =
  let try_stmt = Llvm.Stmt.trycatch () in
  let block = Llvm.Block.try_block try_stmt in
  add_block { ctx with env = { ctx.env with block; trycatch = try_stmt } } stmts;
  List.iteri catches ~f:(fun idx { exc; var; stmts } ->
      let typ = Option.value_map exc ~f:(fun e -> C.get_realization ~ctx (Ast.var_of_node_exn e)) ~default:Ctypes.null in
      let block = Llvm.Block.catch try_stmt typ in
      add_block { ctx with env = { ctx.env with block } } stmts ~preprocess:(fun ctx ->
          Option.value_map var ~default:() ~f:(fun var ->
              let v = Llvm.Var.catch try_stmt idx in
              C.add ~ctx var (C.Var v))));
  Option.value_map finally  ~default:() ~f:(
      add_block { ctx with env = { ctx.env with block = (Llvm.Block.finally try_stmt) } });
  try_stmt

and parse_throw ctx expr =
  Llvm.Stmt.throw (E.parse ~ctx expr)

and parse_global ctx var =
  match Ctx.in_scope ~ctx var with
  | Some (_, vann) when not vann.global ->
    C.err ~ctx "only toplevel symbols can be set as a local global"
  | Some (C.Var v, vann) when vann.base <> ctx.env.base ->
    C.add ~ctx var ~global:true (C.Var v);
    Llvm.Stmt.pass ()
  | _ -> C.err ~ctx "variable %s not found" var

and parse_prefetch ctx pfs =
  let keys, wheres =
    List.unzip
    @@ List.map pfs ~f:(function
            | _, Index (e1, [ e2 ]) ->
              let e1 = E.parse ~ctx e1 in
              let e2 = E.parse ~ctx e2 in
              e1, e2
            | _ -> C.err ~ctx "invalid prefetch expression (only a[b] allowed)")
  in
  let s = Llvm.Stmt.prefetch keys wheres in
  Llvm.Func.set_prefetch ctx.env.base s;
  s

  (* ***************************************************************
    Helper functions
    *************************************************************** *)

(** [add_block ?preprocess context statements] creates a new block within the [context]
    and adds [statements] to that block.
    [preprocess context], if provided, is run after the block is created
    to initialize the block. *)
and add_block ctx ?(preprocess = fun _ -> ()) stmts =
  Ctx.add_block ~ctx;
  preprocess ctx;
  ignore @@ List.map stmts ~f:(parse ~ctx ~toplevel:false);
  Ctx.clear_block ~ctx
