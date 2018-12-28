(******************************************************************************
 * 
 * Seq OCaml 
 * intf.ml: AST parsing interface
 *
 * Author: inumanag
 *
 ******************************************************************************)

(** Expression AST parsing interface  *)
module type ExprIntf =
sig
   (** Parses expression ([Ast.ExprNode.t]) AST *)
   val parse: 
      Ctx.t -> Ast.ExprNode.t -> 
      Llvm.Types.expr_t
   
   (** Parses expression ([Ast.ExprNode.t]) AST and returns 
       [Llvm.Types.typ] if it is a type; otherwise raises exception. *)
   val parse_type: 
      Ctx.t -> Ast.ExprNode.t -> 
      Llvm.Types.typ_t
end

(** Statement AST parsing interface *)
module type StmtIntf =
sig
   (** Parses statement ([Ast.StmtNode.t]) AST *)
   val parse: 
      ?toplevel : bool -> ?jit : bool ->
      Ctx.t -> Ast.StmtNode.t ->
      Llvm.Types.stmt_t
   
   (** Parses module ([Ast.t]) AST *)
   val parse_module: 
      ?jit : bool ->
      Ctx.t -> Ast.t -> 
      unit
   
   (** Parses For statement AST. 
       Public in order to allow simple generator expression generation. *)
   val parse_for:
      ?next: (Ctx.t -> Ctx.t -> Llvm.Types.stmt_t -> unit) ->
      Ctx.t -> Ast.Pos.t -> 
      string list * Ast.ExprNode.t * Ast.StmtNode.t list ->
      Llvm.Types.stmt_t
   
   (** Finalizes parsed statement ([Llvm.Types.stmt]) *)
   val finalize: 
      ?add:bool -> Ctx.t -> Llvm.Types.stmt_t -> Ast.Pos.t -> 
      Llvm.Types.stmt_t
end
