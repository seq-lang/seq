(* *****************************************************************************
 * Seq.Runner: Main execution module
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Core
open Seqaml

(* (** [exec_string ~file ~debug context code] parses a code
    within string [code] as a module and returns parsed module AST.
    [file] is code filename used for error reporting. *)
let exec_string ~ctx ?(file = "<internal>") ?(debug = false) ?(jit = false) code =
  ignore @@ Codegen.parse ~file code ~f:(Codegen.Stmt.parse_module ~jit ~ctx) *)

(** [init file error_handler] initializes Seq session with file [file].
    [error_handler typ position] is a callback called upon encountering
    [Err.CompilerError]. Returns [Module] if successful. *)
let init file error_handler =
  try
    let parse = Codegen.parse ~f:ignore in
    let filename = Filename.realpath file in
    let tctx = Typecheck_ctx.init_module ~filename Typecheck_stmt.parse in

    let mdl = Llvm.Module.init () in
    let block = Llvm.Module.block mdl in
    let lctx = Codegen_ctx.init_module ~filename ~mdl ~base:mdl ~block (Codegen_stmt.parse ~jit:false) in

    let _ =
      file
      |> In_channel.read_lines
      |> String.concat ~sep:"\n"
      |> parse ~file:filename
      |> List.map ~f:(Typecheck_stmt.parse ~ctx:tctx)
      |> List.concat
      |> List.map ~f:(Codegen_stmt.parse ~ctx:lctx ~toplevel:true)
    in
    Some mdl
  with Err.CompilerError (typ, pos) ->
    error_handler typ pos;
    None

(** [parse_c file] is a C callback that wraps [init].
    Error handler relies on [caml_error_callback] C FFI
    to pass errors upstream.
    Returns pointer to [Module] or zero if unsuccessful. *)
let parse_c fname =
  let error_handler typ (pos : Ast.Ann.t list) =
    let Ast.Ann.{ file; line; col; len } = (List.hd_exn pos).pos in
    let msg =
      match typ with
      | Err.Parser -> "parsing error"
      | Lexer s | Descent s | Compiler s | Internal s -> s
    in
    let open Ctypes in
    let f = Foreign.foreign "caml_error_callback" (string @-> int @-> int @-> string @-> returning void) in
    f msg line col file
  in
  let seq_module = init fname error_handler in
  match seq_module with
  | Some seq_module -> Ctypes.raw_address_of_ptr (Ctypes.to_voidp seq_module)
  | None -> Nativeint.zero
