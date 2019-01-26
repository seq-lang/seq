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

let es = ExprNode.to_string
let ss = StmtNode.to_string

let cnt = ref 1 ;;

(* type node = 
  | Node of (string * node list) *)

type ctx_t = 
  { secure:   bool;
    part:     bool;
    residue:  bool;
    children: string list }

let get_var ctx s = 
  match Hashtbl.find ctx s with
  | Some _ ->
    let s = sprintf "%s_%d" s !cnt in
    cnt := !cnt + 1;
    s
  | None -> s

let pst fmt = 
  Core.ksprintf (fun msg -> match Parser.parse_ast msg with Module s -> s) fmt

let part_name v = sprintf "⌥ %s" v
let res_name  v = sprintf "⌘ %s" v
let wrap x = sprintf "$%s$" x

let get_name ctx p =
  match Hashtbl.find ctx p with
  | Some {residue = true; _} -> res_name p
  | _ -> p

let is_residue ctx v =
  match Hashtbl.find ctx v with
  | Some { residue; _ } -> residue
  | _ -> false

let set_name ctx ?(secure=true) ?(residue=false) ?(part=false) ?(parents=[]) n = 
  List.iter parents ~f:(fun p -> 
    match Hashtbl.find ctx p with
    | Some pc ->
      Hashtbl.set ctx ~key:p ~data:{ pc with children = n :: pc.children }
    | None ->
      failwith (sprintf "||> can't find parent %s for var %s" p n));
  Hashtbl.set ctx ~key:n ~data:{ secure; residue; part; children = [] }
  
let get_part ctx v = 
  (* Util.dbg "=>> get part for %s" v; *)
  let bn = part_name v in
  assert (not (is_residue ctx v));
  match Hashtbl.find ctx bn with
  | Some { part; _ } -> 
    assert part;
    bn, []
  | None ->
    set_name ctx bn 
      ~part:true
      ~parents:[v];
    bn, pst "$%s$ := S.BP($%s$)" (get_name ctx bn) (get_name ctx v)
  
let get_val ?(reconstruct=false) ctx v = 
  match Hashtbl.find ctx v with
  | Some ({ residue = true; _ } as s) -> 
    if reconstruct then begin
      Hashtbl.set ctx ~key:v ~data:{ s with residue = false };
      v, pst "$%s$ := S.BR($%s$)" v (res_name v)
    end else res_name v, []
  | _ -> v, []

let rec remove_node ctx p = 
  (* Util.dbg "### removing %s" p; *)
  let ch = Hashtbl.find_exn ctx p in
  Hashtbl.remove ctx p;
  List.iter ch.children ~f:(remove_node ctx)

(* returns Option(secure expr variable name; list of statements to execute) *)
let rec sec_expr ctx ((pos, node): ExprNode.t) = 
  match node with
  | Id v ->
    begin match Hashtbl.find ctx v with
      | Some { secure = true; _ } -> Some(v, [])
      | _ -> None
    end
  | Binary (lh, op, rh) ->
    let lhx = sec_expr ctx lh in
    let rhx = sec_expr ctx rh in
    if (is_none lhx) && (is_none rhx) then None
    else begin
      let op_fn = match op with 
        | "+" -> "add" | "-" -> "sub"
        | "*" -> "mul" 
        (* | "/" -> "div"  *)
        | "@" -> "matmul"
        (* | "<" -> "le" | ">" -> "ge" *)
        | _ -> serr ~pos "[sec] operator %s not supported" op
      in
      match lhx, rhx with
      | Some(l_name, l_st), None -> (* public operation *)
        let n_name = get_var ctx @@ sprintf "%s%s" l_name op in
        set_name ctx n_name
          ~residue:(is_residue ctx l_name) 
          ~parents:[l_name];
        let n_st = pst "$%s$ := S.%s_public($%s$, %s)" 
          (get_name ctx n_name)
          op_fn 
          (get_name ctx l_name) (es rh) 
        in
        Some(n_name, l_st @ n_st)
      | Some(l_name, l_st), Some(r_name, r_st) when op = "+" || op = "-" ->
        let n_name = get_var ctx @@ sprintf "%s%s%s" 
          l_name op r_name
        in
        set_name ctx n_name
          ~residue:(is_residue ctx l_name || is_residue ctx r_name) 
          ~parents:[l_name; r_name];
        let n_st = pst "$%s$ := S.%s($%s$, $%s$)" 
          (get_name ctx n_name)
          op_fn 
          (get_name ctx l_name) (get_name ctx r_name)
        in
        Some(n_name, l_st @ r_st @ n_st)
      | Some(l_name, l_st), Some(r_name, r_st) -> 
        let l_name, lrec_st = get_val ~reconstruct:true ctx l_name in
        let r_name, rrec_st = get_val ~reconstruct:true ctx r_name in
        let lpart_name, lpart_st = get_part ctx l_name in
        let rpart_name, rpart_st = get_part ctx r_name in
        let n_name = get_var ctx @@ sprintf "%s%s%s" 
          l_name op r_name 
        in
        set_name ctx n_name
          ~residue:true 
          ~parents:[lpart_name; rpart_name];
        let n_st = pst "$%s$ := S.BM%s($%s$, $%s$)" 
          (get_name ctx n_name)
          (if op = "*" then "E" else "M")
          (get_name ctx lpart_name) (get_name ctx rpart_name)
        in
        Some(n_name, lrec_st @ lpart_st @ rrec_st @ rpart_st @ n_st)
      | _ -> serr ~pos "cannot do r-public operations"
    end
  | Call(callee, args) ->
    let arg_val = List.map args ~f:(fun (_, { value; _ }) -> value) in
    let arg_sec = List.map arg_val ~f:(sec_expr ctx) in
    if not (List.exists arg_sec ~f:is_some) then None
    else begin match snd callee with
      | Id fn_name ->
        let arg_sts, arg_names = List.zip_exn arg_val arg_sec 
        |> List.fold_map ~init:[] ~f:(fun acc -> function
            | e, None -> acc, es e
            | _, Some (s_name, s_st) ->
              let s_name, srec_st = get_val ~reconstruct:true ctx s_name in
              acc @ s_st @ srec_st, get_name ctx s_name)
        in
        let n_name = get_var ctx @@ sprintf "C%s" fn_name in
        set_name ctx n_name ~parents:arg_names;
        let n_st = pst "$%s$ := %s(%s)" 
          (get_name ctx n_name) 
          fn_name  
          (String.concat ~sep:", " @@ List.map arg_names ~f:wrap)
        in
        Some(n_name, arg_sts @ n_st)
      | _ -> serr ~pos "cannot do this w/ secure args"
    end
  | Dot (lh, _) | Index (lh, _) | TypeOf lh | Ptr lh | Unary (_, lh) ->
    if is_some @@ sec_expr ctx lh then
      serr ~pos "cannot use this on secure variables";
    None
  | Slice(lh, rh, _) ->
    if is_some @@ Option.map lh ~f:(sec_expr ctx) then
      serr ~pos "cannot use this on secure variables";
    if is_some @@ Option.map rh ~f:(sec_expr ctx) then
      serr ~pos "cannot use this on secure variables";
    None
  | (Generator _ | ListGenerator _ | SetGenerator _ | DictGenerator _)
  | (Tuple _ | List _ | Set _ | Dict _ | IfExpr _ | Pipe _) ->
    serr ~pos "do not support these in secure mode yet"
  | _ -> 
    None

let sec_stmt ctx (pos, node: StmtNode.t) =
  match node with 
  | Expr e -> 
    sec_expr ctx e |> Option.value_map ~default:[pos, node] ~f:snd
  | Assign ((_, Id l_name), rh, shadow) ->
    sec_expr ctx rh 
    |> Option.value_map ~default:[pos, node] ~f:(fun (r_name, r_st) ->
        (* Util.dbg "--> %s is res: %b" r_name @@ is_residue ctx r_name; *)
        let child_to_remove = match Hashtbl.find ctx l_name with
          | Some s -> s.children
          | None -> []
        in
        set_name ctx l_name 
          ~residue:(is_residue ctx r_name)  
          ~parents:[];
        let l_st = pst "$%s$ %s= $%s$" 
          (get_name ctx l_name)
          (if shadow then ":" else "") 
          (get_name ctx r_name)
        in
        List.iter child_to_remove ~f:(remove_node ctx);
        r_st @ l_st)
  | Assign _ ->
    serr ~pos "not OK yet in secure mode"
  | Print (exs, ee) -> 
    let sts, args = List.fold_map exs ~init:[] ~f:(fun acc e ->
      match sec_expr ctx e with
      | None -> acc, es e
      | Some (s_name, s_st) ->
        let s_name, srec_st = get_val ~reconstruct:true ctx s_name in
        acc @ s_st @ srec_st, sprintf "S.str($%s$)" (get_name ctx s_name))
    in 
    sts @ pst "print %s%s" 
      (String.concat ~sep:", " args) 
      (if ee = "\n" then "" else ",")
  | node -> 
    [pos, node]

let parse_secure stmts inputs = 
  let ctx = String.Table.create () in

  let st = List.concat @@ List.map inputs ~f:(fun inp ->
    set_name ctx inp;
    pst "%s := S.share(%s)" inp inp)
  in
  let stmts = List.concat @@ List.map stmts ~f:(fun s ->
    Util.dbg "################################################### %s" @@ ss s;
    let stmts = sec_stmt ctx s in
    List.iter stmts ~f:(fun s -> Util.dbg "%s" @@ ss s);
    stmts) 
  in
  (* exit 0; *)
  st @ stmts

