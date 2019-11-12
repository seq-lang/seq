(* ****************************************************************************
 * Seqaml.Codegen_expr: Generate Seq code from expression AST
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Err

(** This module implements [Codegen_intf.Expr].
    Parametrized by [Codegen_intf.Stmt] for parsing generators ([parse_for] and [finalize]) *)
module Codegen (S : Codegen_intf.Stmt) : Codegen_intf.Expr = struct
  open Ast.Expr

  (* ***************************************************************
     Public interface
     *************************************************************** *)

  (** [parse ~ctx expr] dispatches an expression AST node [expr] to the proper code generation function. *)
  let rec parse ~(ctx : Ctx.t) (pos, node) =
    let expr =
      match node with
      | Empty p -> parse_none ctx pos p
      | Bool p -> parse_bool ctx pos p
      | Int p -> parse_int ctx pos p
      | IntS p -> parse_int ctx pos (fst p) ~kind:(snd p)
      | Float p -> parse_float ctx pos p
      | FloatS p -> parse_float ctx pos (fst p) ~kind:(snd p)
      | String p -> parse_str ctx pos p
      | Kmer p -> parse_kmer ctx pos p
      | Seq p -> parse_seq ctx pos p
      | Id p -> parse_id ctx pos p
      | Tuple p -> parse_tuple ctx pos p
      | List p -> parse_list ctx pos p
      | Set p -> parse_list ctx pos p ~kind:"set"
      | Dict p -> parse_dict ctx pos p
      | Generator p -> parse_gen ctx pos p
      | ListGenerator p -> parse_list_gen ctx pos p
      | SetGenerator p -> parse_list_gen ctx pos p ~kind:"set"
      | DictGenerator p -> parse_dict_gen ctx pos p
      | IfExpr p -> parse_if ctx pos p
      | Unary p -> parse_unary ctx pos p
      | Binary p -> parse_binary ctx pos p
      | Pipe p -> parse_pipe ctx pos p
      | Index p -> parse_index ctx pos p
      | Call p -> parse_call ctx pos p
      | Dot p -> parse_dot ctx pos p
      | TypeOf p -> parse_typeof ctx pos p
      | Ptr p -> parse_ptr ctx pos p
      | Ellipsis p -> Ctypes.null
      | Slice _ -> serr ~pos "slice is currently only valid within an index expression"
      | Unpack _ -> serr ~pos "invalid unpacking expression"
      | Lambda _ -> serr ~pos "lambdas not yet supported (parse)"
    in
    (* Update C++ bookkeeping members *)
    Llvm.Expr.set_pos expr pos;
    Llvm.Expr.set_trycatch expr ctx.trycatch;
    expr

  (** [parse_type ~ctx expr] parses an [expr] AST and ensures that it compiles to a type.
      Returns a [Llvm.TypeExpr] handle.  Raises error if [expr] does not compile to a type. *)
  and parse_type ~ctx (pos, node) =
    let expr =
      match node with
      | Id p -> parse_id ctx pos p ~is_type:true
      | Index p -> parse_index ctx pos p ~is_type:true
      | Dot p -> parse_dot ctx pos p
      | TypeOf p -> parse_typeof ctx pos p
      | _ -> serr ~pos "must refer to type"
    in
    Llvm.Expr.set_pos expr pos;
    Llvm.Expr.set_trycatch expr ctx.trycatch;
    if Llvm.Expr.is_type expr
    then Llvm.Type.expr_type expr
    else serr ~pos "must refer to type"

  (* ***************************************************************
     Node code generators
     ***************************************************************
     Each AST node is dispatched to the proper codegen function.
     Each codegen function [f] is called as [f context position data]
     where [data] is a node-specific type defined in [Ast_expr]. *)
  and parse_none _ _ _ = Llvm.Expr.none ()
  and parse_bool _ _ b = Llvm.Expr.bool b

  and parse_int ctx pos ?(kind = "") i =
    let is_unsigned =
      if String.prefix i 1 <> "-" then Caml.Int64.of_string_opt ("0u" ^ i) else None
    in
    let i =
      match kind, Caml.Int64.of_string_opt i with
      | _, Some i -> Llvm.Expr.int i
      | ("u" | "U"), None when is_some is_unsigned ->
        Llvm.Expr.int (Option.value_exn is_unsigned)
      | _ -> serr ~pos "integer too large"
    in
    match kind with
    | "z" | "Z" ->
      (* z-variables are used in Sequre and are syntactic sugar for [MInt] class. *)
      let t = get_internal_type ~pos ~ctx "MInt" in
      Llvm.Expr.construct t [ i ]
    | _ -> i

  and parse_float ctx pos ?(kind = "") f =
    let f = Llvm.Expr.float f in
    match kind with
    | "z" | "Z" ->
      let t = get_internal_type ~pos ~ctx "ModFloat" in
      Llvm.Expr.construct t [ f ]
    | _ -> f

  and parse_str _ _ s = Llvm.Expr.str s

  and parse_seq ctx pos (p, s) =
    match p with
    | "p" ->
      parse_call ctx pos ((pos, Id "pseq"),
        [pos, {name=None; value=(pos, String s)}])
    | "s" -> Llvm.Expr.seq s
    | _ -> serr ~pos "invalid seq prefix"

  and parse_kmer ctx pos s =
    let n = sprintf "%d" @@ String.length s in
    parse ~ctx
    @@ ( pos
       , Call
           ( (pos, Index ((pos, Id "Kmer"), (pos, Int n)))
           , [ pos, { name = None; value = pos, Seq ("s", s) } ] ) )

  and parse_id ?map ?(is_type = false) ctx pos var =
    let map = Option.value map ~default:ctx.map in
    let pref = String.prefix var 1 in
    let suf = String.suffix var (String.length var - 1) in
    match is_type, Hashtbl.find map var with
    (* Make sure that a variable is either accessible within
      the same base (function) or that it is global variable  *)
    | _, Some ((Ctx_namespace.Type t, _) :: _) -> Llvm.Expr.typ t
    | true, _ -> serr ~pos "type %s not found or realized" var
    | false, Some ((Ctx_namespace.Var v, { base; global; _ }) :: _)
      when ctx.base = base || global ->
      let e = Llvm.Expr.var v in
      if global && ctx.base = base && Stack.exists ctx.flags ~f:(( = ) "atomic")
      then Llvm.Var.set_atomic e;
      e
    | false, Some ((Ctx_namespace.Func (t, _), _) :: _) -> Llvm.Expr.func t
    | _ -> serr ~pos "identifier %s not found or realized" var

  and parse_tuple ctx _ args =
    let args = List.map args ~f:(parse ~ctx) in
    Llvm.Expr.tuple args

  (* [kind] is either "set" or "list". Anything else will crash a program. *)
  and parse_list ?(kind = "list") ctx pos args =
    let typ = get_internal_type ~pos ~ctx kind in
    let args = List.map args ~f:(parse ~ctx) in
    Llvm.Expr.list ~kind typ args

  and parse_dict ctx pos args =
    let flatten l =
      List.fold ~init:[] ~f:(fun acc (x, y) -> y :: x :: acc) l |> List.rev
    in
    let typ = get_internal_type ~pos ~ctx "dict" in
    let args = List.map (flatten args) ~f:(parse ~ctx) in
    Llvm.Expr.list ~kind:"dict" typ args

  and parse_gen ctx pos (expr, gen) =
    let captures = String.Table.create () in
    walk
      ~ctx
      (pos, Generator (expr, gen))
      ~f:(fun (ctx : Ctx.t) var ->
        match Hashtbl.find ctx.map var with
        | Some ((Ctx_namespace.Var v, { base; global; _ }) :: _)
          when ctx.base = base || global ->
          Hashtbl.set captures ~key:var ~data:v
        | _ -> ());
    Hashtbl.iter_keys captures ~f:(fun key -> Util.dbg "[expr/parse_gen] captured %s" key);
    (* [final_expr] will be set later during the recursion *)
    let final_expr = ref Ctypes.null in
    let ctx = { ctx with trycatch = Ctypes.null } in
    let body =
      comprehension_helper ~ctx gen ~finally:(fun ctx ->
          let expr = parse ~ctx expr in
          let captures = Hashtbl.data captures in
          final_expr := Llvm.Expr.gen_comprehension expr captures)
    in
    assert (not (Ctypes.is_null !final_expr));
    Llvm.Expr.set_comprehension_body ~kind:"gen" !final_expr body;
    !final_expr

  and parse_list_gen ?(kind = "list") ctx pos (expr, gen) =
    let typ = get_internal_type ~pos ~ctx kind in
    (* [final_expr] will be set later during the recursion *)
    let final_expr = ref Ctypes.null in
    let body =
      comprehension_helper ~ctx gen ~finally:(fun ctx ->
          let expr = parse ~ctx expr in
          final_expr := Llvm.Expr.list_comprehension ~kind typ expr)
    in
    assert (not (Ctypes.is_null !final_expr));
    Llvm.Expr.set_comprehension_body ~kind !final_expr body;
    !final_expr

  and parse_dict_gen ctx pos (expr, gen) =
    let typ = get_internal_type ~pos ~ctx "dict" in
    (* [final_expr] will be set later during the recursion *)
    let final_expr = ref Ctypes.null in
    let body =
      comprehension_helper ~ctx gen ~finally:(fun ctx ->
          let e1 = parse ~ctx (fst expr) in
          let e2 = parse ~ctx (snd expr) in
          final_expr := Llvm.Expr.dict_comprehension typ e1 e2)
    in
    assert (not (Ctypes.is_null !final_expr));
    Llvm.Expr.set_comprehension_body ~kind:"dict" !final_expr body;
    !final_expr

  and parse_if ctx _ (cond, if_expr, else_expr) =
    let if_expr = parse ~ctx if_expr in
    let else_expr = parse ~ctx else_expr in
    let c_expr = parse ~ctx cond in
    Llvm.Expr.cond c_expr if_expr else_expr

  and parse_unary ctx _ (op, expr) =
    let expr = parse ~ctx expr in
    Llvm.Expr.unary op expr

  and parse_binary ctx pos (lh_expr, bop, rh_expr) =
    (* in-place expressions are specified via +=, -= and similar operators *)
    let inplace, bop =
      if String.prefix bop 8 = "inplace_"
      then true, String.drop_prefix bop 8
      else false, bop
    in
    let bop_expr () =
      let lh_expr = parse ~ctx lh_expr in
      let rh_expr = parse ~ctx rh_expr in
      Llvm.Expr.binary ~inplace lh_expr bop rh_expr
    in
    match lh_expr, inplace, bop with
    (* ensure that atomic operators are properly handled *)
    | (_, Id var), true, ("+" | "-" | "&" | "|" | "^" | "min" | "max")
      when Stack.exists ctx.flags ~f:(( = ) "atomic") ->
      (match Hashtbl.find ctx.map var with
      | Some ((Ctx_namespace.Var v, { global; base; _ }) :: _)
        when global && ctx.base = base ->
        let rh_expr = parse ~ctx rh_expr in
        Llvm.Expr.atomic_binary v bop rh_expr
      | _ -> bop_expr ())
    | _ -> bop_expr ()

  and parse_pipe ctx _ exprs =
    let exprs' = List.map exprs ~f:(fun (_, e) -> parse ~ctx e) in
    let ret = Llvm.Expr.pipe exprs' in
    List.iteri exprs ~f:(fun i (pipe, _) ->
        if pipe = "||>" then Llvm.Expr.set_parallel ret i);
    ret

  (* Parses index expression which also includes type realization rules.
     Check GOTCHAS for details. *)
  and parse_index ?(is_type = false) ctx pos (lh_expr, indices) =
    match is_type, snd lh_expr, snd indices with
    | _, Id (("array" | "ptr" | "generator" | "optional") as name), Tuple _ ->
      serr ~pos "%s requires a single type" name
    | _, Id (("array" | "ptr" | "generator" | "optional") as name), _ ->
      let typ = parse_type ~ctx indices in
      Llvm.Expr.typ @@ Llvm.Type.param ~name typ
    | _, Id "Kmer", Int n ->
      let n = int_of_string n in
      if n < 1 || n > 1024
      then serr ~pos "invalid Kmer parameter (must be an integer in 1..1024)";
      Llvm.Expr.typ @@ Llvm.Type.kmerN n
    | _, Id "Int", Int n ->
      let n = int_of_string n in
      if n < 1 || n > 2048
      then serr ~pos "invalid Int parameter (must be an integer in 1..2048)";
      Llvm.Expr.typ @@ Llvm.Type.intN n
    | _, Id "UInt", Int n ->
      let n = int_of_string n in
      if n < 1 || n > 2048
      then serr ~pos "invalid UInt parameter (must be an integer in 1..2048)";
      Llvm.Expr.typ @@ Llvm.Type.uintN n
    | _, Id "function", Tuple indices ->
      let indices = List.map indices ~f:(parse_type ~ctx) in
      let ret, args = List.hd_exn indices, List.tl_exn indices in
      Llvm.Expr.typ @@ Llvm.Type.func ret args
    | _, Id "function", _ ->
      let ret, args = parse_type ~ctx indices, [] in
      Llvm.Expr.typ @@ Llvm.Type.func ret args
    | _, Id "tuple", Tuple indices ->
      let indices = List.map indices ~f:(parse_type ~ctx) in
      let names = List.map indices ~f:(fun _ -> "") in
      Llvm.Expr.typ @@ Llvm.Type.record names indices ""
    | _, Id "tuple", _ ->
      Llvm.Expr.typ @@ Llvm.Type.record [ "" ] [ parse_type ~ctx indices ] ""
    | false, _, Slice (st, ed, step) ->
      if is_some step
      then serr ~pos "slices with stepping parameter are not yet supported";
      let unpack st = Option.value_map st ~f:(parse ~ctx) ~default:Ctypes.null in
      let lh_expr = parse ~ctx lh_expr in
      Llvm.Expr.slice lh_expr (unpack st) (unpack ed)
    | _ ->
      let lh_expr = parse ~ctx lh_expr in
      let indices =
        match snd indices with
        | Tuple indices -> List.map indices ~f:(parse ~ctx)
        | _ -> [ parse ~ctx indices ]
      in
      let all_types = List.for_all indices ~f:Llvm.Expr.is_type in
      if all_types
      then (
        let indices = List.map indices ~f:Llvm.Type.expr_type in
        match Llvm.Expr.get_name lh_expr with
        | "type" ->
          let typ = Llvm.Type.expr_type lh_expr in
          let typ = Llvm.Generics.Type.realize typ indices in
          Llvm.Expr.typ typ
        | ("func" | "elem" | "static") as kind ->
          Llvm.Generics.set_types ~kind lh_expr indices;
          lh_expr
        | _ ->
          serr ~pos "expression is not realizable (make sure that it is a generic type)")
      else (
        let t =
          match indices with
          | [ t ] -> t
          | l -> Llvm.Expr.tuple l
        in
        Llvm.Expr.lookup lh_expr t)

  and parse_call ctx pos (callee_expr, args) =
    (* [@@@ocamlformat.disable] *)
    match snd callee_expr, args with
    | Index ((_, Id "__array__"), t), [ (_, { name = _; value }) ] ->
      let t = parse_type ~ctx t in
      let arg = parse ~ctx value in
      Llvm.Expr.alloc_array t arg
    (* fast ''.join optimization *)
    | ( Dot ((_, Id "str"), "join")
      , [ (_, { name = _; value = _, String "" })
        ; ( _
          , { name = _
            ; value =
                ( pos_g
                , ListGenerator ((_, (_, { gen; cond = None; next = None; _ })) as g) )
            } )
        ] )
    | ( Dot ((_, Id "str"), "join")
      , [ (_, { name = _; value = _, String "" })
        ; ( _
          , { name = _
            ; value =
                pos_g, Generator ((_, (_, { gen; cond = None; next = None; _ })) as g)
            } )
        ] )
    | ( Dot ((_, String ""), "join")
      , [ ( _
          , { name = _
            ; value =
                ( pos_g
                , ListGenerator ((_, (_, { gen; cond = None; next = None; _ })) as g) )
            } )
        ] )
    | ( Dot ((_, String ""), "join")
      , [ ( _
          , { name = _
            ; value =
                pos_g, Generator ((_, (_, { gen; cond = None; next = None; _ })) as g)
            } )
        ] ) ->
      parse_call_real
        ctx
        pos
        ( (pos, Dot ((pos, Id "str"), "cati_ext"))
        , [ pos_g, { name = None; value = pos_g, Generator g } ] )
    | _ -> parse_call_real ctx pos (callee_expr, args)

  and parse_call_real ctx pos (callee_expr, args) =
    let callee_expr = parse ~ctx callee_expr in
    let names = Llvm.Func.get_arg_names callee_expr in
    let _, args =
          List.fold_map args ~init:false ~f:(fun acc (pos, { name; value }) ->
              match name with
              | None when acc ->
                serr ~pos "unnamed argument cannot follow a named argument"
              | None -> false, ("", parse ~ctx value)
              | Some name -> true, (name, parse ~ctx value))
        in
    if Llvm.Expr.is_type callee_expr
    then (
      let typ = Llvm.Type.expr_type callee_expr in
      Llvm.Expr.construct typ (List.map args ~f:snd))
    else (
      let kind = if List.exists args ~f:(fun (_, x) -> x = Ctypes.null) then "partial" else "call" in
      Llvm.Expr.call ~kind callee_expr args)

  and parse_dot ctx pos (lh_expr, rhs) =
    (* checks whether [lh_expr.rhs] refers to an import *)
    let rec imports ictx = function
      | _, Id x ->
        (match Hashtbl.find ictx x with
        | Some ((Ctx_namespace.Import x, _) :: _) -> Some x
        | _ -> None)
      | _, Dot (a, x) ->
        (match imports ictx a with
        | Some ictx ->
          (match Hashtbl.find ictx x with
          | Some ((Ctx_namespace.Import x, _) :: _) -> Some x
          | _ -> None)
        | _ -> None)
      | _, _ -> None
    in
    match imports ctx.map lh_expr with
    | Some ictx ->
      Util.dbg "import helper...";
      parse_id ctx ~map:ictx pos rhs
    | None ->
      let lh_expr = parse ~ctx lh_expr in
      if Llvm.Expr.is_type lh_expr
      then (
        let typ = Llvm.Type.expr_type lh_expr in
        Llvm.Expr.static typ rhs)
      else Llvm.Expr.element lh_expr rhs

  and parse_typeof ctx _ expr =
    let expr = parse ~ctx expr in
    Llvm.Expr.typ @@ Llvm.Expr.typeof expr

  and parse_ptr ctx pos = function
    | _, Id var ->
      (match Hashtbl.find ctx.map var with
      | Some ((Ctx_namespace.Var v, { base; global; _ }) :: _)
        when ctx.base = base || global ->
        Llvm.Expr.ptr v
      | _ -> serr ~pos "symbol %s not found" var)
    | _ -> serr ~pos "ptr requires an identifier as a parameter"
    (* ***************************************************************
     Helper functions
     *************************************************************** *)

  (** [comprehension_helper ~add ~finally ~ctx comprehension_expr]
      constructs a series of [For] statements from [comprehension_expr]
      and passes the final context with the loop variables to a finalization
      function [finally context]. This function typically constructs [CompExpr] handle.
      Returns topmost [For] statement that is not assigned to the matching block (set [add] to true to allow that).
      Other [For] statements are assigned to the matching blocks. *)
  and comprehension_helper ?(add = false) ~finally ~(ctx : Ctx.t) (pos, comp) =
    S.parse_for ~ctx pos (comp.var, comp.gen, []) ~next:(fun orig_ctx ctx for_stmt ->
        let block =
          match comp.cond with
          | None -> ctx.block
          | Some expr ->
            let if_stmt = Llvm.Stmt.cond () in
            let if_expr = parse ~ctx expr in
            let if_block = Llvm.Block.elseif if_stmt if_expr in
            ignore @@ S.finalize ~ctx if_stmt pos;
            if_block
        in
        let ctx = { ctx with block } in
        let expr =
          match comp.next with
          | None -> finally ctx
          | Some next -> ignore @@ comprehension_helper ~add:true ~finally ~ctx next
        in
        ignore @@ S.finalize ~add ~ctx:orig_ctx for_stmt pos)

  (** [get_internal_type ~ctx typ_str] returns a
      corresponding [Llvm.Type.typ] for type identifier [typ_str].
      Raises error if [typ_str] does not exist within [ctx].
      Useful for getting types for [list], [dict] and other internal classes. *)
  and get_internal_type ~pos ~(ctx : Ctx.t) typ_str =
    match Hashtbl.find ctx.map typ_str with
    | Some ((Ctx_namespace.Type typ, _) :: _) -> typ
    | _ -> ierr ~pos "cannot find base type %s (get_internal_type)" typ_str

  (** [walk ~ctx ~f expr] walks through an AST node [expr] and calls
      [f ctx identifier] on each child that potentially contains an identifier [Id].
      Useful for locating all captured variables within [expr]. *)
  and walk ~(ctx : Ctx.t) ~f (pos, node) =
    let rec walk_comp ~ctx ~f c =
      walk ~ctx ~f c.gen;
      Option.value_map c.cond ~default:() ~f:(walk ~ctx ~f);
      Option.value_map c.next ~default:() ~f:(fun x -> walk_comp ~ctx ~f (snd x))
    in
    match node with
    | Id p -> f ctx p
    | Tuple l | List l | Set l -> List.iter l ~f:(walk ~ctx ~f)
    | Pipe l -> List.iter l ~f:(fun (_, e) -> walk ~ctx ~f e)
    | Dict l ->
      List.iter l ~f:(fun (x, y) ->
          walk ~ctx ~f x;
          walk ~ctx ~f y)
    | IfExpr (a, b, c) ->
      walk ~ctx ~f a;
      walk ~ctx ~f b;
      walk ~ctx ~f c
    | Unary (_, e) | Dot (e, _) | TypeOf e -> walk ~ctx ~f e
    | Binary (e1, _, e2) ->
      walk ~ctx ~f e1;
      walk ~ctx ~f e2
    | Index (a, l) ->
      walk ~ctx ~f a;
      walk ~ctx ~f l
    | Call (a, l) ->
      walk ~ctx ~f a;
      List.iter l ~f:(fun (_, { value; _ }) -> walk ~ctx ~f value)
    | ListGenerator (e, c) | SetGenerator (e, c) | Generator (e, c) ->
      walk ~ctx ~f e;
      walk_comp ~ctx ~f (snd c)
    | DictGenerator ((e1, e2), c) ->
      walk ~ctx ~f e1;
      walk ~ctx ~f e2;
      walk_comp ~ctx ~f (snd c)
    (* TODO: | Slice | Lambda *)
    | _ -> ()
end
