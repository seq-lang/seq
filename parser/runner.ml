(* *****************************************************************************
 * Seq.Runner: Main execution module
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Seqaml

(** [exec_string ~file ~debug context code] parses a code
    within string [code] as a module and returns parsed module AST.
    [file] is code filename used for error reporting. *)
let exec_string ctx ?(file = "<internal>") ?(debug = false) ?(jit = false) ?(cell=false) code =
  ignore @@ Codegen.parse ~file code ~f:(Codegen.Stmt.parse_module ~cell ~jit ~ctx)

(** [init file error_handler] initializes Seq session with file [file].
    [error_handler typ position] is a callback called upon encountering
    [Err.CompilerError]. Returns [Module] if successful. *)
let init ~filename code error_handler =
  let mdl = Llvm.Module.init () in
  try
    let exec = exec_string ~debug:false ~jit:false ~cell:false in
    let ctx = Ctx.init_module ~filename ~mdl ~base:mdl ~block:(Llvm.Module.block mdl) exec in
    (* parse the file *)
    Ctx.parse_code ~ctx ~file:filename code;
    Some ctx.mdl
  with
  | Err.CompilerError (typ, pos) ->
    (* Ctx.dump ctx; *)
    error_handler typ pos;
    None
  | exc ->
    let s = Exn.to_string exc in
    error_handler (Internal (sprintf "internal parser error: %s" s)) [Ast.Ann.default];
    None

(** [parse_c file] is a C callback that wraps [init].
    Error handler relies on [caml_error_callback] C FFI
    to pass errors upstream.
    Returns pointer to [Module] or zero if unsuccessful. *)
let parse_c fname =
  let error_handler typ (pos : Ast.Ann.t list) =
    let Ast.Ann.{ file; line; col; len } = List.hd_exn pos in
    let msg =
      match typ with
      | Err.Parser -> "parsing error"
      | Lexer s | Descent s | Compiler s | Internal s -> s
    in
    Ctypes.(
      Foreign.foreign
        "caml_error_callback"
        (string @-> int @-> int @-> string @-> returning void))
      msg
      line
      col
      file
  in
  match fname with
  | "-" ->
    let finish acc = String.concat ~sep:"\n" @@ List.rev acc in
    let code = List.fold_until
      (In_channel.input_lines In_channel.stdin)
      ~init:[] ~finish
      ~f:(fun acc l -> Continue_or_stop.Continue (l :: acc))
    in
    let seq_module = init ~filename:"<stdin>" code error_handler in
    ( match seq_module with
      | Some seq_module -> Ctypes.raw_address_of_ptr (Ctypes.to_voidp seq_module)
      | None -> Nativeint.zero )
  | f when Caml.Sys.file_exists f ->
    let _t = Unix.gettimeofday () in
    let lines = In_channel.read_lines f in
    let code = String.concat ~sep:"\n" lines in
    let seq_module = init ~filename:(Filename.realpath f) code error_handler in
    let ret = match seq_module with
      | Some seq_module -> Ctypes.raw_address_of_ptr (Ctypes.to_voidp seq_module)
      | None -> Nativeint.zero
    in
    Util.dbg "... took %f for the whole OCaml part" (Unix.gettimeofday() -. _t);
    ret
  | f ->
    error_handler (Lexer (sprintf "file '%s' does not exist" f)) [Ast.Ann.default];
    Nativeint.zero
