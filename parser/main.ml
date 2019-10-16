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
      List.map stmts ~f:(function
             | ann, Ast.Stmt.Special ("%typ", [ e ], [ r ]) ->
                [ ann, Ast.Stmt.Special ("%typ", S.parse ~ctx e, [r]) ]
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
                           (Ast.Ann.t_to_string @@ (fst @@ List.last_exn e).typ))
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
      |> List.concat
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
      | _, Ast.Stmt.Special ("%typ", [ _, Ast.Stmt.Expr (e) ], [ r ]) ->
        (* let s = Ast.Ann.t_to_string (fst e).typ in *)
        let typ = match (fst e).typ with
          | (None | Some Import _) as t -> t
          | Some (Type t) ->
            Some (Type (Typecheck_infer.generalize ~level:(-1) t))
          | Some (Var t) ->
            Some (Var (Typecheck_infer.generalize ~level:(-1) t))
        in
        let s = Ast.Ann.t_to_string typ in
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
        let s = Ast.Ann.t_to_string (fst e).typ in
        Util.A.db ~force:true ~el:false "%s" s;
        test_ok "[FAILED OK]";
        incr lines_ok;
        true
      | ann, Special _ ->
        Typecheck_ctx.err "wrong special"
      | s ->
        Util.A.db ~force:true "%s"
        @@ Ast.Ann.t_to_string (fst s).typ;
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
     match Sys.getenv "SEQ_TEST" with
     | Some s -> huuurduuuur ast s
     | None -> failwith "no SEQ_TEST"
   with
  | Err.CompilerError (typ, pos_lst) ->
    Util.dbg ">> %s\n%!" @@ Err.to_string ~pos_lst:(List.map pos_lst ~f:(fun x -> x.pos)) typ
  | Err.SeqCamlError (msg, pos_lst) ->
    Util.dbg ">> %s\n%!" @@ Err.to_string ~pos_lst:(List.map pos_lst ~f:(fun x -> x.pos)) (Err.Compiler msg));
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
      eprintf "%s\n%!" @@ Err.to_string ~pos_lst:(List.map pos_lst ~f:(fun x -> x.pos)) typ;
      exit 1)
  | Some fn ->
    eprintf "%s does not exist" fn;
    exit 1
