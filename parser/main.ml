(******************************************************************************
 *
 * Seq OCaml 
 * main.ml: Entry point module
 *
 * Author: inumanag
 *
 ******************************************************************************)

open Core
open Parser__

(** Entry point *)
let () =
  Callback.register "parse_c" Runner.parse_c;
  match List.nth (Array.to_list Sys.argv) 1 with
  | None ->
    Jit.repl ()
  | Some "--parse" ->
    () 
  | Some "--jupyter" ->
    let kerneldir = Filename.dirname Sys.argv.(0) in
    let settings_json = Sys.argv.(2) in
    ()
    (* Jupyter.jupyter kerneldir settings_json *)
  | Some fn when Caml.Sys.file_exists fn ->
    begin try
      let err_handler = fun a b -> 
        raise (Err.CompilerError (a, b)) 
      in
      let m = Runner.init fn err_handler in
      match m with
      | Some m -> 
        begin try
          Llvm.Module.exec m (Array.to_list Sys.argv) false
        with Err.SeqCError (msg, pos) -> 
          raise @@ Err.CompilerError (Compiler msg, [pos])
        end
      | None -> 
        raise Caml.Not_found
    with Err.CompilerError (typ, pos_lst) as err ->
      Err.print_error typ pos_lst;
      exit 1
    end
  | Some fn ->
    eprintf "%s does not exist" fn;
    exit 1
