(* *****************************************************************************
 * Seq.Main: Entry point module
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Seqaml

module rec S : Typecheck_intf.Stmt = Typecheck_stmt.Typecheck (E) (R)
and E : Typecheck_intf.Expr = Typecheck_expr.Typecheck (R)
and R : Typecheck_intf.Real = Typecheck_realize.Typecheck (E) (S)

exception TestExcOK of Ast.Ann.t
exception TestExcFail of string

let rec ann_to_string ~type_to_string ann = type_to_string ann

and e_to_string ~type_to_string (ann, el) =
  let open Util in
  let e_to_string = e_to_string ~type_to_string in
  let s =
    match el with
    | Ast.Expr.Tuple l -> sprintf "(%s)" (ppl l ~f:e_to_string)
    | List l -> sprintf "[%s]" (ppl l ~f:e_to_string)
    | Set l -> sprintf "{%s}" (ppl l ~f:e_to_string)
    | Dict l ->
      sprintf "{%s}"
      @@ ppl l ~f:(fun (a, b) -> sprintf "%s: %s" (e_to_string a) (e_to_string b))
    | IfExpr (x, i, e) ->
      sprintf "%s if %s else %s" (e_to_string i) (e_to_string x) (e_to_string e)
    | Pipe l -> sprintf "%s" (ppl l ~sep:" |> " ~f:(fun (_, e) -> e_to_string e))
    | Binary (l, o, r) -> sprintf "%s %s %s" (e_to_string l) o (e_to_string r)
    | Unary (o, x) -> sprintf "%s %s" o (e_to_string x)
    | Index (x, l) -> sprintf "%s[[%s]]" (e_to_string x) (ppl l ~f:e_to_string)
    | Dot (x, s) -> sprintf "%s.%s" (e_to_string x) s
    | Call (x, l) ->
      let l =
        ppl l ~f:(fun Ast.Expr.{ name; value } ->
            sprintf
              "%s%s"
              (Option.value_map name ~default:"" ~f:(fun x -> x ^ " = "))
              (e_to_string value))
      in
      sprintf "%s (%s)" (e_to_string x) l
    | Ptr x -> sprintf "ptr(%s)" (e_to_string x)
    | Slice (a, b, c) ->
      let l = List.map [ a; b; c ] ~f:(Option.value_map ~default:"" ~f:e_to_string) in
      sprintf "%s" (ppl l ~sep:":" ~f:Fn.id)
    | Generator (x, c) ->
      sprintf "(%s %s)" (e_to_string x) (comprehension_to_string ~type_to_string c)
    | ListGenerator (x, c) ->
      sprintf "[%s %s]" (e_to_string x) (comprehension_to_string ~type_to_string c)
    | SetGenerator (x, c) ->
      sprintf "{%s %s}" (e_to_string x) (comprehension_to_string ~type_to_string c)
    | DictGenerator ((x1, x2), c) ->
      sprintf
        "{%s: %s %s}"
        (e_to_string x1)
        (e_to_string x2)
        (comprehension_to_string ~type_to_string c)
    | Lambda (l, x) -> sprintf "lambda (%s): %s" (ppl l ~f:Fn.id) (e_to_string x)
    | _ -> ""
  in
  if s = ""
  then ann_to_string ~type_to_string ann
  else sprintf "|%s|:%s" s @@ ann_to_string ~type_to_string ann

and comprehension_to_string ~type_to_string { var; gen; cond; next } =
  let open Util in
  sprintf
    "for %s in %s%s%s"
    (ppl var ~f:Fn.id)
    (e_to_string ~type_to_string gen)
    (Option.value_map cond ~default:"" ~f:(fun x ->
         sprintf "if (%s)" (e_to_string ~type_to_string x)))
    (Option.value_map next ~default:"" ~f:(fun x ->
         " " ^ comprehension_to_string ~type_to_string x))

and s_to_string ~type_to_string ?(indent = 0) (ann, s) =
  let open Util in
  let to_string = s_to_string ~type_to_string in
  let e_to_string = e_to_string ~type_to_string in
  let s =
    match s with
    | Ast.Stmt.Pass _ -> sprintf "pass"
    | Break _ -> sprintf "break"
    | Continue _ -> sprintf "continue"
    | Expr x -> e_to_string x
    | Assign (l, r, _, q) -> sprintf "%s = %s" (e_to_string l) (e_to_string r)
    | Print (x, n) -> sprintf "print %s" (ppl x ~f:e_to_string)
    | Del x -> sprintf "del %s" (e_to_string x)
    | Assert x -> sprintf "assert %s" (e_to_string x)
    | Yield x -> sprintf "yield %s" (Option.value_map x ~default:"" ~f:e_to_string)
    | Return x -> sprintf "return %s" (Option.value_map x ~default:"" ~f:e_to_string)
    | While (x, l) ->
      sprintf
        "while %s:\n%s"
        (e_to_string x)
        (ppl l ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))
    | For (v, x, l) ->
      sprintf
        "for %s in %s:\n%s"
        (ppl v ~f:Fn.id)
        (e_to_string x)
        (ppl l ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))
    | If l ->
      String.concat ~sep:("\n" ^ pad indent)
      @@ List.mapi l ~f:(fun i { cond; cond_stmts } ->
             let cond = Option.value_map cond ~default:"" ~f:e_to_string in
             let case =
               if i = 0 then "if " else if cond = "" then "else" else "elif "
             in
             sprintf
               "%s%s:\n%s"
               case
               cond
               (ppl cond_stmts ~sep:"\n" ~f:(to_string ~indent:(indent + 1))))
    | Match (x, l) ->
      sprintf
        "match %s:\n%s"
        (e_to_string x)
        (ppl l ~sep:"\n" ~f:(fun Ast.Stmt.{ pattern; case_stmts } ->
             sprintf
               "case <?>:\n%s"
               (ppl case_stmts ~sep:"\n" ~f:(to_string ~indent:(indent + 1)))))
    | Function f ->
      (* let args, ret = match (Ast.Ann.real_type ann).typ with
      | Ast.Ann.Func t ->
        ppl (List.zip_exn f.fn_args t.f_args)
          ~f:(fun (Ast.Stmt.{ name; _ }, (_, t)) -> sprintf "%s=%s" name (type_to_string t)),
        sprintf " -> %s" (type_to_string t.f_ret)
      | _ -> failwith (sprintf "naaah %s" (Ast.Ann.to_string ann))
    in
    let generics = ppl f.fn_generics ~f:Fn.id in
    sprintf "def %s%s(%s)%s:\n%s"
      f.fn_name.name
      (if generics = "" then "" else sprintf "[%s]" generics)
      args ret
      (ppl f.fn_stmts ~sep:"\n" ~f:(to_string ~indent:(indent + 1))) *)
      (* Ast.Ann.typ_to_string ann *)
      sprintf "<fun %s>" f.fn_name.name
    | Class f ->
      (* Ast.Ann.typ_to_string ann *)
      sprintf "<cls %s>" f.class_name
    | Special ("%typ", [ s ], _) -> to_string s
    | Special ("%err", [ s ], _) -> to_string s
    (* | Realize (_, cache_name) ->
    let _, s2r = Hashtbl.find_exn generics cache_name in
    ppl ~sep:"\n" ~f:Fn.id @@ List.map (Hashtbl.to_alist s2r) ~f:(fun (r, en) ->
      sprintf "\027[35mREALIZE-CTX: %s\027[34m\n%s" r @@
        s_to_string ~type_to_string (Option.value_exn en.realized_ast) ~indent) *)
    | _ -> "?"
  in
  sprintf "%s%s" (pad indent) s

let typecheck_test_block block_id stmts lines_fail lines_ok =
  let test_ok =
    Util.A.dcol ~force:true ANSITerminal.[ Background Green; Foreground White; Bold ]
  in
  let test_fail =
    Util.A.dcol ~force:true ANSITerminal.[ Background Red; Foreground White; Bold ]
  in
  let set = Hash_set.Poly.create () in
  let parser ~ctx ?(file = "<internal>") code =
    ignore
    @@ Codegen.parse ~file code
  in
  let rec try_stmts stmts =
    try
      let ctx = Typecheck_ctx.init_module ~filename:"test" parser in
      List.concat
      @@ List.map stmts ~f:(function
             | ann, Ast.Stmt.Special ("%typ", [ e ], [ r ]) ->
               [ ann, Ast.Stmt.Special ("%typ", S.parse ~ctx e, [ r ]) ]
             | ann, Special ("%err", [ e ], [ r ]) ->
               if is_some (Hash_set.find set ~f:(( = ) ann))
               then [ ann, Special ("%err", [ e ], [ r ]) ]
               else (
                 try
                   let e = S.parse ~ctx e in
                   raise
                   @@ TestExcFail
                        (sprintf
                           "[FAIL: did not crash, type is '%s']"
                           (s_to_string ~type_to_string:Ast.Ann.typ_to_string
                           @@ List.last_exn e))
                 with
                 | Err.SeqCamlError (msg, pos) as e ->
                   if msg = r
                   then (
                     test_ok "[OK]";
                     raise (TestExcOK ann))
                   else (
                     test_fail "[FAIL: got '%s' instead]" msg;
                     raise e))
             | s -> S.parse ~ctx s)
    with
    | TestExcOK ann ->
      Hash_set.add set ann;
      try_stmts stmts
    | Err.SeqCamlError (msg, pos) ->
      Printexc.print_backtrace stderr;
      raise @@ Err.SeqCamlError (msg, pos)
  in
  let stmts = try_stmts stmts in
  let result =
    List.for_alli stmts ~f:(fun i -> function
      | _, Ast.Stmt.Special ("%typ", [ e ], [ r ]) ->
        let s = s_to_string ~type_to_string:Ast.Ann.typ_to_string e in
        Util.A.db ~force:true ~el:false "%s" s;
        (match s = r with
        | true ->
          test_ok "[OK]";
          incr lines_ok
        | false ->
          test_fail "[FAIL: expected %s]" r;
          lines_fail := (block_id, !lines_ok + List.length !lines_fail) :: !lines_fail);
        s = r
      | _, Special ("%err", [ e ], [ r ]) ->
        let s = s_to_string ~type_to_string:Ast.Ann.typ_to_string e in
        Util.A.db ~force:true ~el:false "%s" s;
        test_ok "[FAILED OK]";
        incr lines_ok;
        true
      | ann, Special _ -> Typecheck_ctx.err "wrong special"
      | s ->
        Util.A.db ~force:true "%s"
        @@ s_to_string ~type_to_string:Ast.Ann.typ_to_string s;
        true)
  in
  Util.A.db ~force:true "";
  result

let huuurduuuur stmts tests =
  let tests = String.split ~on:',' tests in
  let lines_fail, lines_ok = ref [], ref 0 in
  let l, f =
    List.foldi stmts ~init:(0, 0) ~f:(fun i acc s ->
        let is_ok que pat =
          if String.suffix pat 1 = "*"
          then String.is_prefix que ~prefix:(String.drop_suffix pat 1)
          else pat = que
        in
        match snd s with
        | Ast.Stmt.Special ("%test", stmts, [ test ]) ->
          (match List.find tests ~f:(is_ok test) with
          | Some _ ->
            Util.A.dcol
              ~force:true
              ANSITerminal.[ Background Blue; Foreground White; Bold ]
              "## Test %s (%d): %80s"
              test
              i
              " ";
            let nl, nf = acc in
            if typecheck_test_block test stmts lines_fail lines_ok
            then nl + 1, nf
            else nl + 1, nf + 1
          | None -> acc)
        | _ -> acc)
  in
  let back =
    if f = 0 then ANSITerminal.(Background Green) else ANSITerminal.(Background Red)
  in
  Util.A.dcol
    ~force:true
    ANSITerminal.[ back; Foreground White; Bold ]
    "%d tests, %d failed (%d lines, %d failed [%s])"
    l
    f
    (!lines_ok + List.length !lines_fail)
    (List.length !lines_fail)
  @@ Util.ppl !lines_fail ~f:(fun (x, y) -> sprintf "%s:%d" x y)

let pax file =
  (try
     let lines = In_channel.read_lines file in
     let code = String.concat ~sep:"\n" lines ^ "\n" in
     let ast = Codegen.parse ~file:(Filename.realpath file) code in
     huuurduuuur ast "e*"
   with
  | Err.CompilerError (typ, pos_lst) ->
    Util.dbg ">> %s\n%!" @@ Err.to_string ~pos_lst typ
  | Err.SeqCamlError (msg, pos_lst) ->
    Util.dbg ">> %s\n%!" @@ Err.to_string ~pos_lst (Err.Compiler msg));
  exit 0

(** Main entry point. *)
let () =
  Callback.register "parse_c" pax;
  (* Runner.parse_c; *)
  match List.nth (Array.to_list Sys.argv) 1 with
  | None -> Jit.repl ()
  | Some "--parse" -> ()
  | Some fn when Caml.Sys.file_exists fn ->
    (try
       let err_handler a b = raise (Err.CompilerError (a, b)) in
       let m = Runner.init fn err_handler in
       match m with
       | Some m ->
         (try Llvm.Module.exec m (Array.to_list Sys.argv) false with
         | Err.SeqCError (msg, pos) ->
           raise @@ Err.CompilerError (Compiler msg, [ pos ]))
       | None -> raise Caml.Not_found
     with
    | Err.CompilerError (typ, pos_lst) as err ->
      eprintf "%s\n%!" @@ Err.to_string ~pos_lst typ;
      exit 1)
  | Some fn ->
    eprintf "%s does not exist" fn;
    exit 1
