(* 786 *)

open Core

open Ast
open Seqtypes

exception SeqCamlError of string

(* 
TODO: 
- classes
  - base/self
  - GetStaticElemExpr
  - MethodExpr
- funcexpr
- AssignMember
- matches
- type shadowing
*)

type assignable = 
  | Var of seq_var
  | Func of seq_func

let typemap = Hashtbl.create (module String);;
Hashtbl.set typemap ~key:"void"  ~data:(void_type ());
Hashtbl.set typemap ~key:"int"   ~data:(int_type ());
Hashtbl.set typemap ~key:"str"   ~data:(str_type ());
Hashtbl.set typemap ~key:"bool"  ~data:(bool_type ());
Hashtbl.set typemap ~key:"float" ~data:(float_type ());

type context = { 
  base: seq_func; 
  map: (string, assignable) Hashtbl.t;
  stack: (string list) Stack.t 
}

let noimp s = raise (NotImplentedError ("Not yet implemented: " ^ s))

let get_seq_type s = 
  match Hashtbl.find typemap s with
  | Some t -> t
  | None ->
    if String.length s > 0 && s.[0] = '\'' 
    then generic_type ()
    else noimp (sprintf "Type %s" s)

let rec get_seq_expr ctx = function
  | Array -> raise (SeqCamlError "array needs to be part of the CallExpr")
  | Bool(b) -> bool_expr b
  | Int(i) -> int_expr i
  | Float(f) -> float_expr f
  | String(s) -> str_expr s
  | Id(s) -> (match Hashtbl.find ctx.map s with
    | Some v -> (match v with
      | Var v -> var_expr v
      | Func f -> func_expr f)
    | None -> raise (SeqCamlError (sprintf "Variable %s not found" s)))
  | IfExpr(cond, ifc, els) ->
    let condexpr = get_seq_expr ctx cond in
    let ifexpr = get_seq_expr ctx ifc in 
    let elexpr = get_seq_expr ctx els in
    cond_expr condexpr ifexpr elexpr
  | Not(ex) ->
    let expr = get_seq_expr ctx ex in
    uop_expr "!" expr
  | Cond(lhs, op, rhs)
  | Binary(lhs, op, rhs) ->
    let lhexpr = get_seq_expr ctx lhs in
    let rhexpr = get_seq_expr ctx rhs in
    bop_expr op lhexpr rhexpr
  | Call(fn, args) -> begin
    match fn with 
    | Array -> (match args with
      | [Id(s); expr] -> 
        array_expr (array_type @@ get_seq_type s) (get_seq_expr ctx expr)
      | _ -> raise (SeqCamlError "Array constructor requires 2 arguments: one string and one int"))
    | Id(s) when is_some (Hashtbl.find typemap s) ->
      let ty = Hashtbl.find_exn typemap s in
      construct_expr ty @@ List.map args ~f:(get_seq_expr ctx)  
    | _ ->
      let fnexpr = get_seq_expr ctx fn in
      let argexprs = List.map args ~f:(get_seq_expr ctx) in
      call_expr fnexpr argexprs
    end
  | Dot(lhs, rhs) ->
    let lhexpr = get_seq_expr ctx lhs in
    getelem_expr lhexpr rhs
  | Index(lhs, index) ->
    let lhexpr = get_seq_expr ctx lhs in begin
    match index with
    | Slice(st, ed, step) ->
      if is_some step then noimp "Step";
      let st = match st with None -> Ctypes.null | Some st -> get_seq_expr ctx st in
      let ed = match ed with None -> Ctypes.null | Some ed -> get_seq_expr ctx ed in
      array_slice_expr lhexpr st ed
    | _ -> array_lookup_expr lhexpr (get_seq_expr ctx index)
    end
  | Tuple(exprs) ->
    record_expr @@ List.map exprs ~f:(get_seq_expr ctx) 
  | _ -> noimp "Unknown expr"

let match_arg = function
  | PlainArg n -> (n, "'") 
  | TypedArg(n, t) -> (n, match t with Some t -> t | None -> "'")

let rec get_seq_stmt ctx block stmt =   
  let stmt = match stmt with
  | Pass -> pass_stmt ()
  | Break -> break_stmt ()
  | Continue -> continue_stmt ()
  | Statements stmts ->
    List.iter stmts ~f:(get_seq_stmt ctx block);
    pass_stmt ()
  | Assign(lhs, ex) ->
    let rhexpr = get_seq_expr ctx ex in begin 
    match lhs with
    | Id(s) -> begin
      match Hashtbl.find ctx.map s with
      | Some v -> (match v with
        | Var v -> assign_stmt v rhexpr 
        | Func _ -> raise (SeqCamlError (sprintf "Cannot assign something to a function %s" s)))
      | None -> 
        let stmt = var_stmt rhexpr in
        let v = var_stmt_var stmt in
        Hashtbl.set ctx.map ~key:s ~data:(Var v);
        Stack.push ctx.stack (s::Stack.pop_exn ctx.stack);
        stmt
      end
    | Index(var, idx) -> 
      assign_index_stmt (get_seq_expr ctx var) (get_seq_expr ctx idx) rhexpr
    | _ -> raise (SeqCamlError "Assignment requires a variable name on LHS")
    end
  | Exprs exprs ->
    expr_stmt @@ get_seq_expr ctx (List.hd_exn exprs)
  | Print exprs ->
    print_stmt @@ get_seq_expr ctx (List.hd_exn exprs)
  | Return ex ->
    return_stmt @@ get_seq_expr ctx ex
  | Yield ex ->
    set_func_gen ctx.base;
    yield_stmt @@ get_seq_expr ctx ex
  | Type(s, args) ->
    let arg_names, arg_types = List.unzip @@ List.map args ~f:match_arg in
    if is_some (Hashtbl.find typemap s) then
      raise (SeqCamlError (sprintf "Type %s already defined" s));
    let ty = record_type arg_names (List.map arg_types ~f:get_seq_type) in
    Hashtbl.set typemap ~key:s ~data:ty;
    pass_stmt ()
  | If(ifl) -> 
    let ifs = if_stmt () in
    List.iter ifl ~f:(fun (cond, stl) ->
      let bl = match cond with 
      | None -> get_else_block ifs
      | Some cond -> get_elif_block ifs @@ get_seq_expr ctx cond
      in
      Stack.push ctx.stack [];
      List.iter stl ~f:(get_seq_stmt ctx bl);
      Stack.pop_exn ctx.stack |> List.iter ~f:(Hashtbl.remove ctx.map));
    ifs
  | While(cond, stl) ->
    let condexpr = get_seq_expr ctx cond in
    let whs = while_stmt(condexpr) in
    let bl = get_while_block whs in
    Stack.push ctx.stack [];
    List.iter stl ~f:(get_seq_stmt ctx bl);
    Stack.pop_exn ctx.stack |> List.iter ~f:(Hashtbl.remove ctx.map);
    whs
  | For(var, gen, stl) ->
    let genexpr = get_seq_expr ctx gen in
    let fors = for_stmt(genexpr) in

    let var_name, var = match var with
    | Id var -> (match Hashtbl.find ctx.map var with
      | Some _ -> raise (SeqCamlError (sprintf "Variable %s already declared" var))
      | None -> (var, get_for_var fors))
    | _ -> noimp "For non-ID variable"
    in
    let bl = get_for_block fors in
    Stack.push ctx.stack [var_name];
    Hashtbl.set ctx.map ~key:var_name ~data:(Var var);
    List.iter stl ~f:(get_seq_stmt ctx bl);
    Stack.pop_exn ctx.stack |> List.iter ~f:(Hashtbl.remove ctx.map);
    fors
  | Function(ret, args, stl) as f ->
    let _ = get_seq_func ctx f in
    pass_stmt ()
  | Class(s, args, fns) ->
    if is_some (Hashtbl.find typemap s) then
      raise (SeqCamlError (sprintf "Type %s already defined" s));
    
    let arg_names, arg_types = List.unzip @@ List.map args ~f:match_arg in
    let rty = record_type arg_names (List.map arg_types ~f:get_seq_type) in
    let ty = ref_type s rty in
    Hashtbl.set typemap ~key:s ~data:ty;

    let new_ctx = {ctx with map = String.Table.create ()} in
    List.iter fns ~f:(fun f -> 
      let name, fn = get_seq_func new_ctx f in 
      add_ref_method ty name fn);
    pass_stmt ()
  | _ -> noimp "Unknown stmt"
  in 
  set_base stmt ctx.base;
  add_stmt stmt block

and get_seq_func ctx = function 
  | Function(ret, args, stl) ->
    let fn_name, fn_type = match_arg ret in
    let fn_type = get_seq_type fn_type in
    if is_some @@ Hashtbl.find ctx.map fn_name then 
      SeqCamlError (sprintf "Cannot define function %s as the variable with same name exists" fn_name) |> raise;
    let fn = func_stmt fn_name fn_type in
    Hashtbl.set ctx.map ~key:fn_name ~data:(Func fn);

    let arg_names, arg_types = List.unzip @@ List.map args ~f:match_arg in 
    let arg_types = List.map arg_types ~f:get_seq_type in
    set_func_params fn arg_names arg_types;

    let fctx = {base = fn; map = String.Table.create (); stack = Stack.create ()} in
    List.iter arg_names ~f:(fun a -> Hashtbl.set fctx.map ~key:a ~data:(Var (get_func_arg fn a)));
    Stack.push fctx.stack arg_names;
    
    let bl = get_func_block fn in
    List.iter stl ~f:(get_seq_stmt fctx bl);
    (fn_name, fn)
  | _ -> raise (SeqCamlError "get_seq_func MUST HAVE Function as an input")

let seq_exec ast = 
  let mdl = init_module () in
  let block = get_module_block mdl in
  let ctx = {base = mdl; map = String.Table.create (); stack = Stack.create ()} in
  Stack.push ctx.stack [];
  match ast with Module stmts -> List.iter stmts ~f:(get_seq_stmt ctx block);
  exec_module mdl true
  
let () = 
  fprintf stderr "behold the ocaml-seq!\n";
  if Array.length Sys.argv < 2 then begin
    noimp "No arguments"
  end;
  let infile = Sys.argv.(1) in
  let lines = In_channel.read_lines infile in
  let code = (String.concat ~sep:"\n" lines) ^ "\n" in
  let lexbuf = Lexing.from_string code in
  let state = Lexer.stack_create () in
  try
    fprintf stderr "|> Code ==> \n%s\n" code;
    let ast = Parser.program (Lexer.token state) lexbuf in  
    fprintf stderr "|> AST::Caml ==> \n%s\n" @@ Ast.prn_ast ast;
    fprintf stderr "|> C++ ==>\n%!";
    seq_exec ast
  with 
  | Lexer.SyntaxError msg ->
    fprintf stderr "!! Lexer error: %s\n%!" msg
  | Parser.Error ->
    let print_position lexbuf =
      let pos = lexbuf.Lexing.lex_curr_p in
      sprintf "%s;line %d;pos %d" pos.pos_fname pos.pos_lnum (pos.pos_cnum - pos.pos_bol + 1)
    in
    fprintf stderr "!! Menhir error %s: %s\n%!" (print_position lexbuf) (Lexing.lexeme lexbuf)
  | SeqCamlError msg ->
    fprintf stderr "!! OCaml/Seq error: %s\n%!" msg
  | Failure msg ->
    fprintf stderr "!! C++/JIT error: %s\n%!" msg
