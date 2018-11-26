(* 786 *)

module type Expr =
sig
   val parse: 
      Ctx.t -> Ast.ExprNode.t -> 
      Llvm.Types.expr_t
   val parse_type: 
      Ctx.t -> Ast.ExprNode.t -> 
      Llvm.Types.typ_t
end

module type Stmt =
sig
   val parse: 
      Ctx.t -> Ast.StmtNode.t -> 
      Llvm.Types.stmt_t
   val parse_module: 
      Ctx.t -> Ast.t -> 
      unit
   val finalize_stmt: 
      ?add:bool -> Ctx.t -> Llvm.Types.stmt_t -> Ast.Pos.t -> 
      Llvm.Types.stmt_t
end
