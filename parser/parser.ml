(******************************************************************************
 *
 * Seq OCaml 
 * parser.ml: Main parsing module
 *
 * Author: inumanag
 *
 ******************************************************************************)

open Core
open Err

(* As [StmtParser] depends on [ExprParser] and vice versa,
   we need to instantiate these modules recursively *)
module rec SeqS : Intf.StmtIntf = Stmt.StmtParser (SeqE)
       and SeqE : Intf.ExprIntf = Expr.ExprParser (SeqS) 

(** [parse_string ~file ~debug context code] parses a code
    within string [code] as a module and returns parsed module AST.
    [file] is code filename used for error reporting. *)
let parse_string ctx ?file ?(debug=false) ?(jit=false) code =
  let file = Option.value file ~default:"" in
  let lexbuf = Lexing.from_string (code ^ "\n") in
  try
    let state = Lexer.stack_create file in
    let ast = Grammar.program (Lexer.token state) lexbuf in
    if debug then 
      Util.dbg "%s" (Ast.to_string ast);
    SeqS.parse_module ~jit ctx ast
  with
  | SyntaxError (msg, pos) ->
    raise @@ CompilerError (Lexer msg, [pos])
  | Grammar.Error ->
    let pos = Ast.Pos.
      { file;
        line = lexbuf.lex_start_p.pos_lnum;
        col = lexbuf.lex_start_p.pos_cnum - lexbuf.lex_start_p.pos_bol;
        len = 1 } 
    in
    (* Printexc.print_backtrace stderr; *)
    raise @@ CompilerError (Parser, [pos])
  | SeqCamlError (msg, pos) ->
    raise @@ CompilerError (Descent msg, pos)
  | SeqCError (msg, pos) ->
    raise @@ CompilerError (Compiler msg, [pos])

(** [init file error_handler] initializes Seq session with file [file].
    [error_handler typ position] is a callback called upon encountering
    [Err.CompilerError]. Returns [Module] if successful. *)
let init file error_handler =
  let mdl = Llvm.Module.init () in
  let ctx = Ctx.init_module
    ~filename:file
    ~mdl
    ~base:mdl
    ~block:(Llvm.Module.block mdl)
    (parse_string ~debug:false ~jit:false)
  in 
  try
    (* parse the file *)
    Ctx.parse_file ctx file;
    Some ctx.mdl
  with CompilerError (typ, pos) ->
    Ctx.dump ctx;
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
      | Lexer s -> s
      | Parser -> "parsing error"
      | Descent s -> s
      | Compiler s -> s 
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
