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
  let rec parse ~(ctx : Codegen_ctx.t) (ann, node) =
    let expr =
      match node with
      | Empty p -> parse_none ctx ann p
      | Bool p -> parse_bool ctx ann p
      | Int p -> parse_int ctx ann p
      | IntS p -> parse_int ctx ann (fst p) ~kind:(snd p)
      | Float p -> parse_float ctx ann p
      | FloatS p -> parse_float ctx ann (fst p) ~kind:(snd p)
      | String p -> parse_str ctx ann p
      | Kmer p -> parse_kmer ctx ann p
      | Seq p -> parse_seq ctx ann p
      | Id p -> parse_id ctx ann p
      | Tuple p -> parse_tuple ctx ann p
      | List p -> parse_list ctx ann p
      | Set p -> parse_list ctx ann p ~kind:"set"
      | Dict p -> parse_dict ctx ann p
      | Generator p -> parse_gen ctx ann p
      | ListGenerator p -> parse_list_gen ctx ann p
      | SetGenerator p -> parse_list_gen ctx ann p ~kind:"set"
      | DictGenerator p -> parse_dict_gen ctx ann p
      | IfExpr p -> parse_if ctx ann p
      | Unary p -> parse_unary ctx ann p
      | Binary p -> parse_binary ctx ann p
      | Pipe p -> parse_pipe ctx ann p
      | Index p -> parse_index ctx ann p
      | Call p -> parse_call ctx ann p
      | Dot p -> parse_dot ctx ann p
      | TypeOf p -> parse_typeof ctx ann p
      | Ptr p -> parse_ptr ctx ann p
      | Ellipsis p -> Ctypes.null
      | Slice _ -> serr ~ann "slice is currently only valid within an index expression"
      | Unpack _ -> serr ~ann "invalid unpacking expression"
      | Lambda _ -> serr ~ann "lambdas not yet supported (parse)"
    in
    (* Update C++ bookkeeping members *)
    Llvm.Expr.set_pos expr ann;
    Llvm.Expr.set_trycatch expr ctx.env.trycatch;
    expr

  (** [parse_type ~ctx expr] parses an [expr] AST and ensures that it compiles to a type.
      Returns a [Llvm.TypeExpr] handle.  Raises error if [expr] does not compile to a type. *)
  and parse_type ~ctx (ann, node) =
    let expr =
      match node with
      | Id p -> parse_id ctx ann p ~is_type:true
      | Index p -> parse_index ctx ann p ~is_type:true
      | Dot p -> parse_dot ctx ann p
      | TypeOf p -> parse_typeof ctx ann p
      | _ -> serr ~ann "must refer to type"
    in
    Llvm.Expr.set_pos expr ann;
    Llvm.Expr.set_trycatch expr ctx.env.trycatch;
    if Llvm.Expr.is_type expr
    then Llvm.Type.expr_type expr
    else serr ~ann "must refer to type"

  (* ***************************************************************
     Node code generators
     ***************************************************************
     Each AST node is dispatched to the proper codegen function.
     Each codegen function [f] is called as [f context position data]
     where [data] is a node-specific type defined in [Ast_expr]. *)
  and parse_none _ _ _ = Llvm.Expr.none ()
  and parse_bool _ _ b = Llvm.Expr.bool b

  and parse_int ctx ann ?(kind = "") i =
    let is_unsigned =
      if String.prefix i 1 <> "-" then Caml.Int64.of_string_opt ("0u" ^ i) else None
    in
    let i =
      match kind, Caml.Int64.of_string_opt i with
      | _, Some i -> Llvm.Expr.int i
      | ("u" | "U"), None when is_some is_unsigned ->
        Llvm.Expr.int (Option.value_exn is_unsigned)
      | _ -> serr ~ann "integer too large"
    in
    match kind with
    | "z" | "Z" ->
      (* z-variables are used in Sequre and are syntactic sugar for [MInt] class. *)
      let t = get_internal_type ~ann ~ctx "MInt" in
      Llvm.Expr.construct t [ i ]
    | _ -> i

  and parse_float ctx ann ?(kind = "") f =
    let f = Llvm.Expr.float f in
    match kind with
    | "z" | "Z" ->
      let t = get_internal_type ~ann ~ctx "ModFloat" in
      Llvm.Expr.construct t [ f ]
    | _ -> f

  and parse_str _ _ s = Llvm.Expr.str s
  and parse_seq _ _ s = Llvm.Expr.seq s

  and parse_kmer ctx ann s =
    let n = sprintf "%d" @@ String.length s in
    parse ~ctx
    @@ ( ann
       , Call
           ( (ann, Index ((ann, Id "Kmer"), [ ann, Int n ]))
           , [ { name = None; value = ann, Seq s } ] ) )

  and parse_id ?map ?(is_type = false) ctx ann var =
    let map = Option.value map ~default:ctx.map in
    let pref = String.prefix var 1 in
    let suf = String.suffix var (String.length var - 1) in
    match is_type, Hashtbl.find map var with
    (* Make sure that a variable is either accessible within
      the same base (function) or that it is global variable  *)
    | _, Some ((Codegen_ctx.Type t, _) :: _) -> Llvm.Expr.typ t
    | true, _ -> serr ~ann "type %s not found or realized" var
    | false, Some ((Codegen_ctx.Var v, { base; global; _ }) :: _)
      when ctx.env.base = base || global ->
      let e = Llvm.Expr.var v in
      if global
         && ctx.env.base = base
         && Stack.exists ctx.env.flags ~f:(( = ) "atomic")
      then (
        Llvm.Module.warn ~ann "atomic load %s" var;
        Llvm.Var.set_atomic e);
      e
    | false, Some ((Codegen_ctx.Func (t, _), _) :: _) -> Llvm.Expr.func t
    | _ -> serr ~ann "identifier %s not found or realized" var

  and parse_tuple ctx _ args =
    let args = List.map args ~f:(parse ~ctx) in
    Llvm.Expr.tuple args

  (* [kind] is either "set" or "list". Anything else will crash a program. *)
  and parse_list ?(kind = "list") ctx ann args =
    let typ = get_internal_type ~ann ~ctx kind in
    let args = List.map args ~f:(parse ~ctx) in
    Llvm.Expr.list ~kind typ args

  and parse_dict ctx ann args =
    let flatten l =
      List.fold ~init:[] ~f:(fun acc (x, y) -> y :: x :: acc) l |> List.rev
    in
    let typ = get_internal_type ~ann ~ctx "dict" in
    let args = List.map (flatten args) ~f:(parse ~ctx) in
    Llvm.Expr.list ~kind:"dict" typ args

  and parse_gen ctx ann (expr, gen) =
    let captures = String.Table.create () in
    walk
      ~ctx
      (ann, Generator (expr, gen))
      ~f:(fun (ctx : Codegen_ctx.t) var ->
        match Hashtbl.find ctx.map var with
        | Some ((Codegen_ctx.Var v, { base; global; _ }) :: _)
          when ctx.env.base = base || global ->
          Hashtbl.set captures ~key:var ~data:v
        | _ -> ());
    Hashtbl.iter_keys captures ~f:(fun key ->
        Util.dbg "[expr/parse_gen] captured %s" key);
    (* [final_expr] will be set later during the recursion *)
    let final_expr = ref Ctypes.null in
    let ctx = { ctx with env = { ctx.env with trycatch = Ctypes.null } } in
    let body =
      comprehension_helper ~ctx ann gen ~finally:(fun ctx ->
          let expr = parse ~ctx expr in
          let captures = Hashtbl.data captures in
          final_expr := Llvm.Expr.gen_comprehension expr captures)
    in
    assert (not (Ctypes.is_null !final_expr));
    Llvm.Expr.set_comprehension_body ~kind:"gen" !final_expr body;
    !final_expr

  and parse_list_gen ?(kind = "list") ctx ann (expr, gen) =
    let typ = get_internal_type ~ann ~ctx kind in
    (* [final_expr] will be set later during the recursion *)
    let final_expr = ref Ctypes.null in
    let body =
      comprehension_helper ~ctx ann gen ~finally:(fun ctx ->
          let expr = parse ~ctx expr in
          final_expr := Llvm.Expr.list_comprehension ~kind typ expr)
    in
    assert (not (Ctypes.is_null !final_expr));
    Llvm.Expr.set_comprehension_body ~kind !final_expr body;
    !final_expr

  and parse_dict_gen ctx ann (expr, gen) =
    let typ = get_internal_type ~ann ~ctx "dict" in
    (* [final_expr] will be set later during the recursion *)
    let final_expr = ref Ctypes.null in
    let body =
      comprehension_helper ~ctx ann gen ~finally:(fun ctx ->
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

  and parse_binary ctx ann (lh_expr, bop, rh_expr) =
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
      when Stack.exists ctx.env.flags ~f:(( = ) "atomic") ->
      (match Hashtbl.find ctx.map var with
      | Some ((Codegen_ctx.Var v, { global; base; _ }) :: _)
        when global && ctx.env.base = base ->
        let rh_expr = parse ~ctx rh_expr in
        Llvm.Module.warn ~ann " atomic %s on %s" bop var;
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
  and parse_index ?(is_type = false) ctx ann (lh_expr, indices) =
    let indices = List.hd_exn indices in
    match is_type, snd lh_expr, snd indices with
    | _, Id (("array" | "ptr" | "generator") as name), Tuple _ ->
      serr ~ann "%s requires a single type" name
    | _, Id (("array" | "ptr" | "generator") as name), _ ->
      let typ = parse_type ~ctx indices in
      Llvm.Expr.typ @@ Llvm.Type.param ~name typ
    | _, Id "Kmer", Int n ->
      let n = int_of_string n in
      if n < 1 || n > 1024
      then serr ~ann "invalid Kmer parameter (must be an integer in 1..1024)";
      Llvm.Expr.typ @@ Llvm.Type.kmerN n
    | _, Id "Int", Int n ->
      let n = int_of_string n in
      if n < 1 || n > 2048
      then serr ~ann "invalid Int parameter (must be an integer in 1..2048)";
      Llvm.Expr.typ @@ Llvm.Type.intN n
    | _, Id "UInt", Int n ->
      let n = int_of_string n in
      if n < 1 || n > 2048
      then serr ~ann "invalid UInt parameter (must be an integer in 1..2048)";
      Llvm.Expr.typ @@ Llvm.Type.uintN n
    | true, Id "function", Tuple indices ->
      let indices = List.map indices ~f:(parse_type ~ctx) in
      let ret, args = List.hd_exn indices, List.tl_exn indices in
      Llvm.Expr.typ @@ Llvm.Type.func ret args
    | true, Id "function", _ ->
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
      then serr ~ann "slices with stepping parameter are not yet supported";
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
          serr
            ~ann
            "expression is not realizable (make sure that it is a generic type)")
      else (
        let t =
          match indices with
          | [ t ] -> t
          | l -> Llvm.Expr.tuple l
        in
        Llvm.Expr.lookup lh_expr t)

  and parse_call ctx ann (callee_expr, args) =
    (* [@@@ocamlformat.disable] *)
    match snd callee_expr, args with
    | Index ((_, Id "__array__"), [ t ]), [ { name = _; value } ] ->
      let t = parse_type ~ctx t in
      let arg = parse ~ctx value in
      Llvm.Expr.alloc_array t arg
    (* fast ''.join optimization *)
    | ( Dot ((_, Id "str"), "join")
      , [ { name = _; value = _, String "" }
        ; { name = _
          ; value =
              ann_g, ListGenerator ((_, { gen; cond = None; next = None; _ }) as g)
          }
        ] )
    | ( Dot ((_, Id "str"), "join")
      , [ { name = _; value = _, String "" }
        ; { name = _
          ; value = ann_g, Generator ((_, { gen; cond = None; next = None; _ }) as g)
          }
        ] )
    | ( Dot ((_, String ""), "join")
      , [ { name = _
          ; value =
              ann_g, ListGenerator ((_, { gen; cond = None; next = None; _ }) as g)
          }
        ] )
    | ( Dot ((_, String ""), "join")
      , [ { name = _
          ; value = ann_g, Generator ((_, { gen; cond = None; next = None; _ }) as g)
          }
        ] ) ->
      parse_call_real
        ctx
        ann
        ( (ann, Dot ((ann, Id "str"), "cati_ext"))
        , [ { name = None; value = ann_g, Generator g } ] )
    | _ -> parse_call_real ctx ann (callee_expr, args)

  and parse_call_real ctx ann (callee_expr, args) =
    let callee_expr = parse ~ctx callee_expr in
    let names = Llvm.Func.get_arg_names callee_expr in
    let args =
      if List.length names = 0
      then
        List.mapi args ~f:(fun i { name; value } ->
            match name with
            | None -> parse ~ctx value
            | Some _ -> serr ~ann "cannot use named arguments here")
      else (
        (* Check names *)
        let has_named_args =
          List.fold args ~init:false ~f:(fun acc { name; _ } ->
              match name with
              | None when acc ->
                serr ~ann "unnamed argument cannot follow a named argument"
              | None -> false
              | Some _ -> true)
        in
        if has_named_args
        then
          List.mapi names ~f:(fun i n ->
              match List.findi args ~f:(fun _ x -> x.name = Some n) with
              | Some (idx, x) -> parse ~ctx x.value
              | None ->
                (match List.nth args i with
                | Some { name; _ } when is_some name ->
                  serr ~ann "argument %s expected here" n
                | Some { value; _ } -> parse ~ctx value
                | None -> serr ~ann "cannot find an argument %s" n))
        else List.map args ~f:(fun x -> parse ~ctx x.value))
    in
    if Llvm.Expr.is_type callee_expr
    then (
      let typ = Llvm.Type.expr_type callee_expr in
      Llvm.Expr.construct typ args)
    else (
      let kind =
        if List.exists args ~f:(( = ) Ctypes.null) then "partial" else "call"
      in
      Llvm.Expr.call ~kind callee_expr args)

  and parse_dot ctx ann (lh_expr, rhs) =
    (* checks whether [lh_expr.rhs] refers to an import *)
    let rec imports (ictx : Codegen_ctx.t) = function
      | _, Id x ->
        (match Hashtbl.find ictx.map x with
        | Some ((Codegen_ctx.Import x, _) :: _) -> Hashtbl.find ictx.globals.imported x
        | _ -> None)
      | _, Dot (a, x) ->
        (match imports ictx a with
        | Some ictx ->
          (match Hashtbl.find ictx.map x with
          | Some ((Codegen_ctx.Import x, _) :: _) ->
            Hashtbl.find ictx.globals.imported x
          | _ -> None)
        | _ -> None)
      | _, _ -> None
    in
    match imports ctx lh_expr with
    | Some ictx ->
      Util.dbg "import helper...";
      parse_id ctx ~map:ictx.map ann rhs
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

  and parse_ptr ctx ann = function
    | _, Id var ->
      (match Hashtbl.find ctx.map var with
      | Some ((Codegen_ctx.Var v, { base; global; _ }) :: _)
        when ctx.env.base = base || global ->
        Llvm.Expr.ptr v
      | _ -> serr ~ann "symbol %s not found" var)
    | _ -> serr ~ann "ptr requires an identifier as a parameter"
    (* ***************************************************************
     Helper functions
     *************************************************************** *)

  (** [comprehension_helper ~add ~finally ~ctx comprehension_expr]
      constructs a series of [For] statements from [comprehension_expr]
      and passes the final context with the loop variables to a finalization
      function [finally context]. This function typically constructs [CompExpr] handle.
      Returns topmost [For] statement that is not assigned to the matching block (set [add] to true to allow that).
      Other [For] statements are assigned to the matching blocks. *)
  and comprehension_helper ?(add = false) ~finally ~(ctx : Codegen_ctx.t) pos comp =
    S.parse_for ~ctx pos (comp.var, comp.gen, []) ~next:(fun orig_ctx ctx for_stmt ->
        let block =
          match comp.cond with
          | None -> ctx.env.block
          | Some expr ->
            let if_stmt = Llvm.Stmt.cond () in
            let if_expr = parse ~ctx expr in
            let if_block = Llvm.Block.elseif if_stmt if_expr in
            ignore @@ S.finalize ~ctx if_stmt pos;
            if_block
        in
        let ctx = { ctx with env = { ctx.env with block } } in
        let expr =
          match comp.next with
          | None -> finally ctx
          | Some next ->
            ignore @@ comprehension_helper ~add:true ~finally ~ctx pos next
        in
        ignore @@ S.finalize ~add ~ctx:orig_ctx for_stmt pos)

  (** [get_internal_type ~ctx typ_str] returns a
      corresponding [Llvm.Type.typ] for type identifier [typ_str].
      Raises error if [typ_str] does not exist within [ctx].
      Useful for getting types for [list], [dict] and other internal classes. *)
  and get_internal_type ~ann ~(ctx : Codegen_ctx.t) typ_str =
    match Hashtbl.find ctx.map typ_str with
    | Some ((Codegen_ctx.Type typ, _) :: _) -> typ
    | _ -> ierr ~ann "cannot find base type %s (get_internal_type)" typ_str

  (** [walk ~ctx ~f expr] walks through an AST node [expr] and calls
      [f ctx identifier] on each child that potentially contains an identifier [Id].
      Useful for locating all captured variables within [expr]. *)
  and walk ~(ctx : Codegen_ctx.t) ~f (pos, node) =
    let rec walk_comp ~ctx ~f c =
      walk ~ctx ~f c.gen;
      Option.value_map c.cond ~default:() ~f:(walk ~ctx ~f);
      Option.value_map c.next ~default:() ~f:(fun x -> walk_comp ~ctx ~f x)
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
      List.iter l ~f:(walk ~ctx ~f)
    | Call (a, l) ->
      walk ~ctx ~f a;
      List.iter l ~f:(fun { value; _ } -> walk ~ctx ~f value)
    | ListGenerator (e, c) | SetGenerator (e, c) | Generator (e, c) ->
      walk ~ctx ~f e;
      walk_comp ~ctx ~f c
    | DictGenerator ((e1, e2), c) ->
      walk ~ctx ~f e1;
      walk ~ctx ~f e2;
      walk_comp ~ctx ~f c
    (* TODO: | Slice | Lambda *)
    | _ -> ()
end
