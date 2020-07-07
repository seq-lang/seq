#pragma once

#include "lang/expr.h"
#include "lang/patterns.h"
#include "lang/stmt.h"
#include "lang/var.h"

namespace seq {
class Print : public Stmt {
private:
  Expr *expr;
  bool nopOnVoid; // for REPL top-level print wraps
public:
  explicit Print(Expr *expr, bool nopOnVoid = false);
  void codegen0(llvm::BasicBlock *&block) override;
};

class ExprStmt : public Stmt {
private:
  Expr *expr;

public:
  explicit ExprStmt(Expr *expr);
  void codegen0(llvm::BasicBlock *&block) override;
};

class VarStmt : public Stmt {
private:
  Expr *init;
  types::Type *type;
  Var *var;

public:
  explicit VarStmt(Expr *init, types::Type *type = nullptr);
  Var *getVar();
  void codegen0(llvm::BasicBlock *&block) override;
};

class FuncStmt : public Stmt {
private:
  Func *func;

public:
  explicit FuncStmt(Func *func);
  void codegen0(llvm::BasicBlock *&block) override;
};

class Assign : public Stmt {
private:
  Var *var;
  Expr *value;
  bool atomic;

public:
  Assign(Var *var, Expr *value, bool atomic = false);
  void setAtomic();
  void codegen0(llvm::BasicBlock *&block) override;
};

class AssignIndex : public Stmt {
private:
  Expr *array;
  Expr *idx;
  Expr *value;

public:
  AssignIndex(Expr *array, Expr *idx, Expr *value);
  void codegen0(llvm::BasicBlock *&block) override;
};

class Del : public Stmt {
private:
  Var *var;

public:
  explicit Del(Var *var);
  void codegen0(llvm::BasicBlock *&block) override;
};

class DelIndex : public Stmt {
private:
  Expr *array;
  Expr *idx;

public:
  DelIndex(Expr *array, Expr *idx);
  void codegen0(llvm::BasicBlock *&block) override;
};

class AssignMember : public Stmt {
private:
  Expr *expr;
  std::string memb;
  Expr *value;

public:
  AssignMember(Expr *expr, std::string memb, Expr *value);
  AssignMember(Expr *expr, seq_int_t idx, Expr *value);
  void codegen0(llvm::BasicBlock *&block) override;
};

class If : public Stmt {
private:
  std::vector<Expr *> conds;
  std::vector<Block *> branches;
  bool elseAdded;

public:
  If();
  Block *addCond(Expr *cond);
  Block *addElse();
  Block *getBlock(unsigned idx = 0);
  void codegen0(llvm::BasicBlock *&block) override;
};

class TryCatch : public Stmt {
private:
  enum State { NOT_THROWN = 0, THROWN, CAUGHT, RETURN, BREAK, CONTINUE };

  Block *scope;
  std::vector<types::Type *> catchTypes;
  std::vector<Block *> catchBlocks;
  std::vector<Var *> catchVars;
  Block *finally;
  llvm::BasicBlock *exceptionBlock;
  llvm::BasicBlock *exceptionRouteBlock;
  llvm::BasicBlock *finallyStart;
  std::vector<llvm::BasicBlock *> handlers;
  llvm::Value *excFlag;
  llvm::Value *catchStore;
  llvm::Value *delegateDepth;
  llvm::Value *retStore;

  llvm::ConstantInt *state(llvm::LLVMContext &context, State s);

public:
  TryCatch();
  Block *getBlock();
  Var *getVar(unsigned idx);
  Block *addCatch(types::Type *type);
  Block *getFinally();
  llvm::BasicBlock *getExceptionBlock();
  void codegenReturn(Expr *expr, llvm::BasicBlock *&block);
  void codegenBreak(llvm::BasicBlock *&block);
  void codegenContinue(llvm::BasicBlock *&block);
  void codegen0(llvm::BasicBlock *&block) override;
  static llvm::StructType *getPadType(llvm::LLVMContext &context);
  static llvm::StructType *getExcType(llvm::LLVMContext &context);
  static llvm::GlobalVariable *getTypeIdxVar(llvm::Module *module,
                                             const std::string &name);
  static llvm::GlobalVariable *getTypeIdxVar(llvm::Module *module,
                                             types::Type *catchType);
};

class Throw : public Stmt {
private:
  Expr *expr;

public:
  explicit Throw(Expr *expr);
  void codegen0(llvm::BasicBlock *&block) override;
};

class Match : public Stmt {
private:
  Expr *value;
  std::vector<Pattern *> patterns;
  std::vector<Block *> branches;

public:
  Match();
  void setValue(Expr *value);
  Block *addCase(Pattern *pattern);
  void codegen0(llvm::BasicBlock *&block) override;
};

class While : public Stmt {
private:
  Expr *cond;
  Block *scope;

public:
  explicit While(Expr *cond);
  Block *getBlock();
  void codegen0(llvm::BasicBlock *&block) override;
};

class For : public Stmt {
private:
  Expr *gen;
  Block *scope;
  Var *var;

public:
  explicit For(Expr *gen);
  Expr *getGen();
  Block *getBlock();
  Var *getVar();
  void setGen(Expr *gen);
  void codegen0(llvm::BasicBlock *&block) override;
};

class Return : public Stmt {
private:
  Expr *expr;

public:
  explicit Return(Expr *expr);
  Expr *getExpr();
  void codegen0(llvm::BasicBlock *&block) override;
};

class Yield : public Stmt {
private:
  Expr *expr;

public:
  explicit Yield(Expr *expr);
  Expr *getExpr();
  void codegen0(llvm::BasicBlock *&block) override;
};

class Break : public Stmt {
public:
  Break();
  void codegen0(llvm::BasicBlock *&block) override;
};

class Continue : public Stmt {
public:
  Continue();
  void codegen0(llvm::BasicBlock *&block) override;
};

class Assert : public Stmt {
private:
  Expr *expr;
  Expr *msg;

public:
  explicit Assert(Expr *expr, Expr *msg = nullptr);
  void codegen0(llvm::BasicBlock *&block) override;
};

} // namespace seq
