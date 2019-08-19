(******************************************************************************
 *
 * Seq OCaml 
 * runner.ml: Main execution module
 *
 * Author: inumanag
 *
 ******************************************************************************)

open Core
open Parser__

(** [parse_string ~file ~debug context code] parses a code
    within string [code] as a module and returns parsed module AST.
    [file] is code filename used for error reporting. *)
let exec_string ctx ?(file="<internal>") ?(debug=false) ?(jit=false) code =
  ignore @@ Parser.parse_ast ~file code ~fn:(Generator.SeqS.parse_module ~jit ctx)

(** [init file error_handler] initializes Seq session with file [file].
    [error_handler typ position] is a callback called upon encountering
    [Err.CompilerError]. Returns [Module] if successful. *)
let init file error_handler =
  let mdl = Llvm.Module.init () in
  try
    let ctx = Ctx.init_module
      ~filename:file
      ~mdl
      ~base:mdl
      ~block:(Llvm.Module.block mdl)
      (exec_string ~debug:false ~jit:false)
    in 
    (* parse the file *)
    Ctx.parse_file ctx file;
    Some ctx.mdl
  with Err.CompilerError (typ, pos) ->
    (* Ctx.dump ctx; *)
    error_handler typ pos;
    None

(** [parse_c file] is a C callback that wraps [init].
    Error handler relies on [caml_error_callback] C FFI 
    to pass errors upstream.
    Returns pointer to [Module] or zero if unsuccessful. *)
let parse_c fname =
  let error_handler typ (pos: Ast.Pos.t list) =
    let Ast.Pos.{ file; line; col; len } = List.hd_exn pos in
    let msg = match typ with
      | Err.Parser -> "parsing error"
      | Lexer s | Descent s | Compiler s | Internal s -> s
    in
    Ctypes.(Foreign.foreign "caml_error_callback"
      (string @-> int @-> int @-> string @-> returning void)) 
      msg line col file
  in
  let seq_module = init fname error_handler in
  match seq_module with
  | Some seq_module -> 
    Ctypes.raw_address_of_ptr (Ctypes.to_voidp seq_module)
  | None -> 
    Nativeint.zero
