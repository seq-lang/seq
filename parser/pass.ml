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

type ctx_t = 
  { secure:  bool;
    residue: bool; }

let get_var ctx s = 
  match Hashtbl.find ctx s with
  | Some _ ->
    let s = sprintf "%s_%d" s !cnt in
    cnt := !cnt + 1;
    s
  | None -> s

let pst fmt = 
  Core.ksprintf (fun msg -> match Parser.parse_ast msg with Module s -> s) fmt

let get_beaver_name ctx v = 
  ("__B__"^v)
let get_beaver ctx v = 
  let bn = get_beaver_name ctx v in
  match Hashtbl.find ctx bn with
  | Some s -> []
  | None -> 
    Hashtbl.set ctx ~key:bn ~data:{ secure=true; residue=true };
    pst "%s := S.BP(%s)" bn v
let ret_beaver_name ctx v = 
  ("__BR__"^v)
let rec_beaver ctx v = 
  match Hashtbl.find ctx v with
  | Some { secure = _; residue = true } ->
    let bn = ret_beaver_name ctx v in
    begin match Hashtbl.find ctx bn with
      | Some s -> bn, []
      | None -> 
        Hashtbl.set ctx ~key:bn ~data:{ secure=true; residue=false };
        bn, pst "%s := S.BR(%s)" bn v
    end
  | _ -> v, []
let is_residue ctx v =
  match Hashtbl.find ctx v with
  | Some { residue; _ } -> residue
  | _ -> false

(* pst returns list of stmts *)
(* returns (Option(var_name * stmts); None if not secure) *)
let rec sec_expr ctx (pos, (node: ExprNode.node)) : (string * StmtNode.t list) option = 
  match node with
  | Id v ->
    begin match Hashtbl.find ctx v with
      | Some { secure = true; _ } -> Some(v, [])
      | _ -> None
    end
  | Binary (lh, op, rh) ->
    let lhx = sec_expr ctx lh in
    let rhx = sec_expr ctx rh in
    if (is_none lhx) && (is_none rhx) then
      None
    else begin
      let fn = match op with 
        | "+" -> "add" | "-" -> "sub"
        | "*" -> "mul" | "/" -> "div" 
        | "@" -> "matmul"
        | "<" -> "le" | ">" -> "ge"
        | _ -> serr ~pos "[sec] operator %s not supported" op
      in
      let nn, ns, residue = match lhx, rhx with
        | Some(vn, vs), None -> 
          let nn = get_var ctx @@ sprintf "%s_%s_public" vn fn in
          let ns = pst "%s := S.%s_public(%s, %s)" nn fn vn (es rh) in
          nn, vs @ ns, is_residue ctx vn
        | Some(vn, vs), Some(wn, ws) when op = "*" || op = "@" -> 
          let vn, bvs = rec_beaver ctx vn in
          let wn, bws = rec_beaver ctx wn in
          let bvs = bvs @ get_beaver ctx vn in
          let bws = bws @ get_beaver ctx wn in
          let nn = get_var ctx @@ sprintf "%s_%s_%s" vn fn wn in
          let ns = pst "%s := S.BM%s(%s, %s)" nn 
            (if op = "*" then "E" else "M")
            (get_beaver_name ctx vn) (get_beaver_name ctx wn) 
          in
          nn, vs @ ws @ bvs @ bws @ ns, true
        | Some(vn, vs), Some(wn, ws) ->
          let nn = get_var ctx @@ sprintf "%s_%s_%s" vn fn wn in
          let ns = pst "%s := S.%s(%s, %s)" nn fn vn wn in
          nn, vs @ ws @ ns, (is_residue ctx vn) || (is_residue ctx wn)
        | _ -> 
          serr ~pos "cannot do r-public operations"
      in
      Hashtbl.set ctx ~key:nn ~data:{ secure = true; residue };
      Some(nn, ns)
    end
  | Call(callee, arg) ->
    let arg = List.map arg ~f:(fun (_, { value; _ }) -> value) in
    let argx = List.map arg ~f:(sec_expr ctx) in
    if not (List.exists argx ~f:is_some) then
      None
    else begin 
      match snd callee with
      | Id fn ->
        let nn = get_var ctx @@ sprintf "%s_call" fn in
        let ax = 
          List.zip_exn arg argx
           |> List.map ~f:(function
              | _, Some(nv, _) -> nv
              | e, None -> es e)
           |> String.concat ~sep:", "
        in
        let ns = pst "%s := %s(%s)" nn fn ax in
        let ls = List.concat_map argx 
          ~f:(Option.value_map ~f:snd ~default:[]) 
        in
        (* let rs = List.concat_map argx 
          ~f:(Option.value_map ~default:[] ~f:(fun (v, _) -> rec_beaver ctx v)) 
        in *)
        Some(nn, ls)  (* @ ns *)
      | _ -> serr ~pos "cannot do this in secure mode"
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
  | Assign ((_, Id vv), rh, shadow) ->
    (* clear ALL residues *)
    sec_expr ctx rh |> Option.value_map ~default:[pos, node]  
      ~f:(fun (v, stmts) ->
        Hashtbl.set ctx ~key:vv ~data:{ secure=true; residue=(is_residue ctx v) };
        let ns = pst "%s %s= %s" vv (if shadow then ":" else "") v in
        if shadow then Hashtbl.remove ctx (ret_beaver_name ctx vv);
        stmts @ ns)
  | Assign _ ->
    serr ~pos "not OK yet in secure mode"
  | Print e ->
    sec_expr ctx e |> Option.value_map ~default:[pos, node]  
      ~f:(fun (v, stmts) -> 
        let v, rs = rec_beaver ctx v in
        stmts @ rs @ (pst "print S.str(%s)" v))
  | node -> 
    [pos, node]

let parse_secure stmts = 
  let ctx = String.Table.create () in

  Hashtbl.set ctx ~key:"a" ~data:{ secure=true; residue=false };
  Hashtbl.set ctx ~key:"b" ~data:{ secure=true; residue=false };

  let stmts = List.concat @@ List.map stmts ~f:(fun s ->
    let stmts = sec_stmt ctx s in
    List.iter stmts ~f:(fun s -> Util.dbg "%s" @@ ss s);
    stmts) 
  in
  exit 0;
  stmts

