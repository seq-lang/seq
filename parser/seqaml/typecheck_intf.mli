(* ****************************************************************************
 * Seqaml.Typecheck_intf: Type checking interface
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

open Ast

(** Expression AST codegen interface  *)
module type Expr = sig
  (** Parses an [Ast_expr.t] within a context [Ctx.t]. *)
  val parse : ctx:Typecheck_ctx.t -> Expr.t Ann.ann -> Expr.t Ann.ann
end

(** Statement AST codegen interface *)
module type Stmt = sig
  (** Parses an [Ast_stmt.t] within a context [Ctx.t]. *)
  val parse : ctx:Typecheck_ctx.t -> Stmt.t Ann.ann -> Stmt.t Ann.ann list

  val parse_realized : ctx:Typecheck_ctx.t -> Stmt.t Ann.ann list -> Stmt.t Ann.ann list
end

module type Real = sig
  (** Realizes a type within a context [t] *)
  val realize: ctx:Typecheck_ctx.t -> Ast.Ann.tvar -> Ast.Ann.tvar
  (** Fetches a realized internal type within a context [t]. *)
  val internal: ctx:Typecheck_ctx.t -> ?args: Ast.Ann.tvar list -> string -> Ast.Ann.tvar
  (** ??? *)
  val magic: ctx: Typecheck_ctx.t ->
    ?idx:int ->
    ?args: Ast.Ann.tvar list ->
    Ast.Ann.tvar -> string -> Ast.Ann.tvar option
end
