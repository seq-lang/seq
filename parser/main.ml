(* *****************************************************************************
 * Seq.Main: Entry point module
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Seqaml

(** Main entry point. *)
let () =
  Callback.register "parse_c" Runner.parse_c;
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
         | Err.SeqCError (msg, pos) -> raise @@ Err.CompilerError (Compiler msg, [ pos ]))
       | None -> raise Caml.Not_found
     with
    | Err.CompilerError (typ, pos_lst) as err ->
      eprintf "%s\n%!" @@ Err.to_string ~pos_lst typ;
      exit 1)
  | Some fn ->
    eprintf "%s does not exist" fn;
    exit 1
