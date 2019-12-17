(* *****************************************************************************
 * Seqaml.Parser: Parses strings to AST
 *
 * Author: inumanag
 * License: see LICENSE
 * *****************************************************************************)

(** Main code generation modules.
    As [Codegen_stmt] depends on [Codegen_expr] and vice versa, we need to instantiate these modules recursively. *)
module rec Stmt : Codegen_intf.Stmt = Codegen_stmt.Codegen (Expr)
and Expr : Codegen_intf.Expr = Codegen_expr.Codegen (Stmt)
