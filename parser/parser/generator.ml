(* As [StmtParser] depends on [ExprParser] and vice versa,
   we need to instantiate these modules recursively *)
module rec SeqS : Intf.StmtIntf = Stmt.StmtParser (SeqE)
       and SeqE : Intf.ExprIntf = Expr.ExprParser (SeqS) 
