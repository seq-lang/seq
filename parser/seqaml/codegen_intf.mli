(* ****************************************************************************
 * Seqaml.Codegen_intf: Code generation interface
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Ast

(** Expression AST codegen interface  *)
module type Expr = sig
  (** Parses an [Ast_expr.t] within a context [Ctx.t] and returns an LLVM handle. *)
  val parse : ctx:Codegen_ctx.t -> Expr.t Ann.ann -> Llvm.Types.expr_t

  (** Parses an [Ast_expr.t] within context [Ctx.t] and returns an LLVM handle if it describes a type;
      otherwise raises an exception *)
  val parse_type : ctx:Codegen_ctx.t -> Expr.t Ann.ann -> Llvm.Types.typ_t
end

(** Statement AST codegen interface *)
module type Stmt = sig
  (** Parses an [Ast_stmt.t] within a context [Ctx.t] and returns a LLVM handle. *)
  val parse
    :  ?toplevel:bool
    -> ?jit:bool
    -> ctx:Codegen_ctx.t
    -> Stmt.t Ann.ann
    -> Llvm.Types.stmt_t

  (** Parses a module ([Ast.t]) AST *)
  val parse_module : ?jit:bool -> ctx:Codegen_ctx.t -> Stmt.t Ann.ann list -> unit

  (** Parses a [For] statement AST. Public in order to allow access to it from [ExprIntf]. *)
  val parse_for
    :  ?next:(Codegen_ctx.t -> Codegen_ctx.t -> Llvm.Types.stmt_t -> unit)
    -> ctx:Codegen_ctx.t
    -> Ann.t
    -> string list * Expr.t Ann.ann * Stmt.t Ann.ann list
    -> Llvm.Types.stmt_t

  (** Finalizes the construction of a [Llvm.Types.stmt] handle. *)
  val finalize
    :  ?add:bool
    -> ctx:Codegen_ctx.t
    -> Llvm.Types.stmt_t
    -> Ann.t
    -> Llvm.Types.stmt_t
end
