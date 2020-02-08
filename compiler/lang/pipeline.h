#pragma once

#include "lang/expr.h"
#include "types/func.h"

namespace seq {

class PipeExpr : public Expr {
private:
  std::vector<Expr *> stages;
  std::vector<bool> parallel;
  llvm::BasicBlock *entry;
  llvm::Value *syncReg;

  struct PipelineCodegenState;
  llvm::Value *codegenPipe(BaseFunc *base, PipelineCodegenState &state);

public:
  static const unsigned SCHED_WIDTH_PREFETCH = 16;
  static const unsigned SCHED_WIDTH_INTERALIGN = 2048;
  explicit PipeExpr(std::vector<Expr *> stages,
                    std::vector<bool> parallel = {});
  void setParallel(unsigned which);
  void resolveTypes() override;
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
  types::Type *getType0() const override;
  PipeExpr *clone(Generic *ref) override;

  static types::RecordType *getInterAlignYieldType();
  static types::RecordType *getInterAlignParamsType();
  static types::RecordType *getInterAlignSeqPairType();
  static llvm::Value *validateAndCodegenInterAlignParams(
      types::GenType::InterAlignParams &paramExprs, BaseFunc *base,
      llvm::BasicBlock *block);
};

} // namespace seq
