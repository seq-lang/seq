(******************************************************************************
 * 
 * Seq OCaml 
 * expr.ml: Expression AST parsing module
 *
 * Author: inumanag
 *
 ******************************************************************************)

open Core
open Err
open Ast

(** This module is an implementation of [Intf.ExprIntf] module that
    describes expression AST parser.
    Requires [Intf.StmtIntf] for parsing generators 
    ([parse_for] and [finalize]) *)
module ExprParser (S : Intf.StmtIntf) : Intf.ExprIntf = 
struct
  open ExprNode

  (* ***************************************************************
     Public interface
     *************************************************************** *)

  (** [parse context expr] dispatches expression AST to the proper parser.
      Afterwards, a position [pos] is set for [expr] *)
  let rec parse (ctx: Ctx.t) (pos, node) =
    let expr = match node with
      | Empty          p -> parse_none     ctx pos p
      | Bool           p -> parse_bool     ctx pos p
      | Int            p -> parse_int      ctx pos p
      | Float          p -> parse_float    ctx pos p
      | String         p -> parse_str      ctx pos p
      | Seq            p -> parse_seq      ctx pos p
      | Generic p | Id p -> parse_id       ctx pos p
      | Tuple          p -> parse_tuple    ctx pos p
      | List           p -> parse_list     ctx pos p
      | Set            p -> parse_list     ctx pos p ~kind:"set"
      | Dict           p -> parse_dict     ctx pos p
      | Generator      p -> parse_gen      ctx pos p
      | ListGenerator  p -> parse_list_gen ctx pos p
      | SetGenerator   p -> parse_list_gen ctx pos p ~kind:"set"
      | DictGenerator  p -> parse_dict_gen ctx pos p
      | IfExpr         p -> parse_if       ctx pos p
      | Unary          p -> parse_unary    ctx pos p
      | Binary         p -> parse_binary   ctx pos p
      | Pipe           p -> parse_pipe     ctx pos p
      | Index          p -> parse_index    ctx pos p
      | Call           p -> parse_call     ctx pos p
      | Dot            p -> parse_dot      ctx pos p
      | Ellipsis       p -> Ctypes.null
      | Slice  _ -> serr  ~pos "slice is only valid within an index"
      | Unpack _ -> serr  ~pos "unpack is not valid here"
      | TypeOf _ -> failwith "todo: expr/typeof"
      | Lambda _ -> failwith "todo: expr/lambda"
    in
    Llvm.Expr.set_pos expr pos; 
    Llvm.Expr.set_trycatch expr ctx.trycatch;
    expr
  
  (** [parse_type context expr] parses [expr] AST and ensures that 
      it is a type expression. Returns a [TypeExpr].
      Raises error if [expr] does not describe a type. *)
  and parse_type ctx t =
    match parse ctx t with
    | typ_expr when Llvm.Expr.is_type typ_expr ->
      Llvm.Type.expr_type typ_expr
    | _ ->
      serr ~pos:(fst t) "not a type"

  (* ***************************************************************
     Node parsers
     ***************************************************************
     Each AST node is dispatched to its parser function.
     Each parser function [f] is called as [f context position data]
     where [data] is a tuple varying from node to node. *)
  
  and parse_none _ _ _ =
    Llvm.Expr.none ()

  and parse_bool _ _ b =
    Llvm.Expr.bool b

  and parse_int _ _ i = 
    Llvm.Expr.int i

  and parse_float _ _ f = 
    Llvm.Expr.float f

  and parse_str _ _ s = 
    Llvm.Expr.str s

  and parse_seq _ _ s = 
    Llvm.Expr.seq s

  and parse_id ?map ctx pos var = 
    let map = Option.value map ~default:ctx.map in
    match Hashtbl.find map var with
    (* Make sure that a variable is either accessible within 
       the same base (function) or that it is global variable  *)
    | Some (Ctx.Namespace.Var (v, { base; global; _ }) :: _) 
      when (ctx.base = base) || global -> 
      Llvm.Expr.var v
    | Some (Ctx.Namespace.Type t :: _) -> 
      Llvm.Expr.typ t
    | Some (Ctx.Namespace.Func (t, _) :: _) -> 
      Llvm.Expr.func t
    | _ ->
      serr ~pos "symbol '%s' not found or realized" var

  and parse_tuple ctx _ args =
    let args = List.map args ~f:(parse ctx) in
    Llvm.Expr.tuple args

  (** [kind] can be set or list. Anything else will crash a program. *)
  and parse_list ?(kind="list") ctx _ args =
    let typ = get_internal_type ctx kind in
    let args = List.map args ~f:(parse ctx) in
    Llvm.Expr.list ~kind typ args

  and parse_dict ctx _ args =
    let flatten l = 
      List.fold ~init:[] ~f:(fun acc (x, y) -> y::x::acc) l |> List.rev
    in
    let typ = get_internal_type ctx "dict" in
    let args = List.map (flatten args) ~f:(parse ctx) in
    Llvm.Expr.list ~kind:"dict" typ args

  and parse_gen ctx pos (expr, gen) = 
    let captures = String.Table.create () in
    walk ctx (pos, Generator (expr, gen)) ~f:(fun (ctx: Ctx.t) var ->
      match Hashtbl.find ctx.map var with
      | Some (Ctx.Namespace.Var (v, { base; global; _ }) :: _) 
        when (ctx.base = base) || global -> 
        Hashtbl.set captures ~key:var ~data:v
      | _ -> ());
    Hashtbl.iter_keys captures ~f:(fun key ->
      Util.dbg "[expr/parse_gen] captured %s" key);

    (* [final_expr] will be set later during the recursion *)
    let final_expr = ref Ctypes.null in 
    let body = comprehension_helper ctx gen 
      ~finally:(fun ctx ->
        let expr = parse ctx expr in
        let captures = Hashtbl.data captures in
        final_expr := Llvm.Expr.gen_comprehension expr captures)
    in
    assert (not (Ctypes.is_null !final_expr));
    Llvm.Expr.set_comprehension_body ~kind:"gen" !final_expr body;
    !final_expr

  and parse_list_gen ?(kind="list") ctx _ (expr, gen) = 
    let typ = get_internal_type ctx kind in
    (* [final_expr] will be set later during the recursion *)
    let final_expr = ref Ctypes.null in 
    let body = comprehension_helper ctx gen 
      ~finally:(fun ctx ->
        let expr = parse ctx expr in
        final_expr := Llvm.Expr.list_comprehension ~kind typ expr)
    in
    assert (not (Ctypes.is_null !final_expr));
    Llvm.Expr.set_comprehension_body ~kind !final_expr body;
    !final_expr

  and parse_dict_gen ctx _ (expr, gen) = 
    let typ = get_internal_type ctx "dict" in
    (* [final_expr] will be set later during the recursion *)
    let final_expr = ref Ctypes.null in 
    let body = comprehension_helper ctx gen 
      ~finally:(fun ctx ->
        let e1 = parse ctx (fst expr) in
        let e2 = parse ctx (snd expr) in 
        final_expr := Llvm.Expr.dict_comprehension typ e1 e2)
    in
    assert (not (Ctypes.is_null !final_expr));
    Llvm.Expr.set_comprehension_body ~kind:"dict" !final_expr body;
    !final_expr

  and parse_if ctx _  (cond, if_expr, else_expr) =
    let if_expr = parse ctx if_expr in
    let else_expr = parse ctx else_expr in
    let c_expr = parse ctx cond in
    Llvm.Expr.cond c_expr if_expr else_expr

  and parse_unary ctx _ (op, expr) =
    let expr = parse ctx expr in
    Llvm.Expr.unary op expr

  and parse_binary ctx _ (lh_expr, bop, rh_expr) =
    let lh_expr = parse ctx lh_expr in
    let rh_expr = parse ctx rh_expr in
    Llvm.Expr.binary lh_expr bop rh_expr

  and parse_pipe ctx _ exprs =
    let exprs = List.map exprs ~f:(parse ctx) in
    Llvm.Expr.pipe exprs

  (** Parses index expression which also includes type realization rules.
      Check GOTCHAS for details. *)
  and parse_index ctx pos (lh_expr, indices) =
    match snd lh_expr, indices with
    | _, [(_, Slice(st, ed, step))] ->
      if is_some step then 
        failwith "todo: expr/step";
      let unpack st = 
        Option.value_map st ~f:(parse ctx) ~default:Ctypes.null 
      in
      let lh_expr = parse ctx lh_expr in
      Llvm.Expr.slice lh_expr (unpack st) (unpack ed)
    | Id("array" | "ptr" | "generator" as name), _ ->
      if List.length indices <> 1 then
        serr ~pos "%s requires one type" name;
      let typ = parse_type ctx (List.hd_exn indices) in
      Llvm.Expr.typ @@ Llvm.Type.param ~name typ
    | Id("function"), _ ->
      let indices = List.map indices ~f:(parse_type ctx) in
      let ret, args = List.hd_exn indices, List.tl_exn indices in
      Llvm.Expr.typ @@ Llvm.Type.func ret args
    | _ -> 
      let lh_expr = parse ctx lh_expr in
      let indices = List.map indices ~f:(parse ctx) in
      let all_types = List.for_all indices ~f:Llvm.Expr.is_type in
      if all_types then
        let indices = List.map indices ~f:Llvm.Type.expr_type in
        match Llvm.Expr.get_name lh_expr with
        | "type" ->
          let typ = Llvm.Type.expr_type lh_expr in
          let typ = Llvm.Generics.Type.realize typ indices in
          Llvm.Expr.typ typ
        | ("func" | "elem" | "static") as kind ->
          Llvm.Generics.set_types ~kind lh_expr indices;
          (* let typ = Llvm.Generics.Func.realize lh_expr indices in *)
          lh_expr
        | _ ->
          serr ~pos "wrong LHS for type realization"
      else if (List.length indices) = 1 then
        Llvm.Expr.lookup lh_expr (List.hd_exn indices)
      else
        serr ~pos "index requires only one item"
  
  and parse_call ctx pos (callee_expr, args) =
    let callee_expr = parse ctx callee_expr in
    let names = Llvm.Func.get_arg_names callee_expr in
    let args = 
      if List.length names = 0 then 
        List.mapi args ~f:(fun i (pos, { name; value }) -> 
          match name with 
          | None -> parse ctx value
          | Some _ -> serr ~pos "cannot use named parameters here")
      else begin
        (* Check names *)
        let has_named_args = List.fold args ~init:false 
          ~f:(fun acc (pos, { name; _ }) ->
            match name with
              | None when acc ->
                serr ~pos "cannot have unnamed argument after named one"
              | None -> false
              | Some _ -> true)
        in
        if has_named_args then
          List.mapi names ~f:(fun i n ->
            match List.findi args ~f:(fun _ x -> (snd x).name = Some n) with
            | Some (idx, x) ->
              parse ctx (snd x).value
            | None -> 
              match List.nth args i with
              | Some (pos, { name; _ }) when is_some name ->
                serr ~pos "argument %s expected here" n
              | Some (_, { value; _ }) ->
                parse ctx value
              | None ->
                serr ~pos "cannot find argument %s" n)
        else 
          List.map args ~f:(fun x -> parse ctx (snd x).value)
      end
    in
    if Llvm.Expr.is_type callee_expr then
      let typ = Llvm.Type.expr_type callee_expr in
      Llvm.Expr.construct typ args
    else
      let kind = 
        if List.exists args ~f:((=) Ctypes.null) then "partial" 
        else "call" 
      in
      Llvm.Expr.call ~kind callee_expr args
    
  and parse_dot ctx pos (lh_expr, rhs) =
    (* check is import *)
    let rec imports ictx = function 
      | _, Id x -> 
        begin match Hashtbl.find ictx x with 
          | Some (Ctx.Namespace.Import x :: _) -> Some x
          | _ -> None
        end
      | _, Dot (a, x) -> 
        begin match imports ictx a with 
          | Some ictx ->
            begin match Hashtbl.find ictx x with 
              | Some (Ctx.Namespace.Import x :: _) -> Some x
              | _ -> None
            end
          | _ -> None
        end
      | _, _ -> None
    in 
    match imports ctx.map lh_expr with
    | Some ictx ->
      Util.dbg "import helper...";
      parse_id ctx ~map:ictx pos rhs
    | None ->
      let lh_expr = parse ctx lh_expr in
      if Llvm.Expr.is_type lh_expr then
        let typ = Llvm.Type.expr_type lh_expr in
        Llvm.Expr.static typ rhs 
      else
        Llvm.Expr.element lh_expr rhs
              
  (* ***************************************************************
     Helper functions
     *************************************************************** *)

  (** [comprehension_helper ~add ~finally context comprehension_expr] 
      constructs a series of [for] statements that form a [comprehension] 
      and passes the final context with the loop variables to the finalization 
      function [finally context]. This function should construct a proper 
      [CompExpr].
      Returns topmost [For] statement that is not assigned to the
      matching block (set [add] to true to allow that). 
      Other [For] statements are assigned to the matching blocks. *)
  and comprehension_helper ?(add=false) ~finally (ctx: Ctx.t) (pos, comp) =
    S.parse_for ctx pos (comp.var, comp.gen, [])
      ~next:(fun orig_ctx ctx for_stmt -> 
        let block = match comp.cond with
          | None -> 
            ctx.block
          | Some expr ->
            let if_stmt = Llvm.Stmt.cond () in
            let if_expr = parse ctx expr in
            let if_block = Llvm.Block.elseif if_stmt if_expr in
            ignore @@ S.finalize ctx if_stmt pos;
            if_block
        in
        let ctx = { ctx with block } in
        let expr = match comp.next with
          | None -> 
            finally ctx
          | Some next ->   
            ignore @@ comprehension_helper ~add:true ~finally ctx next 
        in
        ignore @@ S.finalize ~add orig_ctx for_stmt pos)

  (** [get_internal_type context type_string] returns a 
      [Llvm.Type.typ] that describes type signature [type_string].  
      Raises error if signature does not exist. 
      Used to get types for [list], [dict] and other internal classes. *)
  and get_internal_type (ctx: Ctx.t) typ_str = 
    match Hashtbl.find ctx.map typ_str with
    | Some (Ctx.Namespace.Type typ :: _) -> typ
    | _ -> failwith (sprintf "can't find internal type %s" typ_str)

  (** [walk context ~f expr] walks the AST [expr] and calls 
      function [f context identifier] on each child generic or identifier. 
      Useful for locating all captured variables within [expr]. *)
  and walk (ctx: Ctx.t) ~f (pos, node) =
    let rec walk_comp ctx ~f c = 
      let open ExprNode in
      walk ctx ~f c.gen;
      Option.value_map c.cond ~default:() ~f:(walk ctx ~f);
      Option.value_map c.next ~default:() ~f:(fun x -> walk_comp ctx ~f (snd x))
    in
    match node with
    | Generic p | Id p -> 
      f ctx p
    | Tuple l | List l | Set l | Pipe l -> 
      List.iter l ~f:(walk ctx ~f)
    | Dict l -> 
      List.iter l ~f:(fun (x, y) -> 
        walk ctx ~f x; walk ctx ~f y)
    | IfExpr (a, b, c) ->
      walk ctx ~f a; walk ctx ~f b; walk ctx ~f c
    | Unary (_, e) | Dot (e, _) | TypeOf e -> 
      walk ctx ~f e
    | Binary (e1, _, e2) -> 
      walk ctx ~f e1; walk ctx ~f e2
    | Index (a, l) ->
      walk ctx ~f a; List.iter l ~f:(walk ctx ~f)
    | Call (a, l) -> 
      walk ctx ~f a; 
      List.iter l ~f:(fun (_, { value; _ }) -> walk ctx ~f value)
    | ListGenerator (e, c) | SetGenerator (e, c) | Generator (e, c) ->
      walk ctx ~f e;
      walk_comp ctx ~f (snd c)
    | DictGenerator ((e1, e2), c) ->
      walk ctx ~f e1; walk ctx ~f e2;
      walk_comp ctx ~f (snd c)
    (* TODO: | Slice | Lambda *)
    | _ -> ()
end
