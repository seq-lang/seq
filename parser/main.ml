(* *****************************************************************************
 * Seq.Main: Entry point module
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core

(** Main entry point. *)
let () =
  Callback.register "parse_c" Runner.parse_c;
  Callback.register "jit_init_c" Jit.c_init;
  Callback.register "jit_exec_c" Jit.c_exec;
  match List.nth (Array.to_list Sys.argv) 1 with
  | None -> Jit.repl ()
  | Some "--parse" -> ()
  | _ ->
    eprintf "please use 'seqc'";
    exit 1
