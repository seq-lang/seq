#ifndef SEQ_PIPELINE_H
#define SEQ_PIPELINE_H

#include "expr.h"

namespace seq {

class PipeExpr : public Expr {
private:
  std::vector<Expr *> stages;
  std::vector<bool> parallel;

public:
  static const unsigned SCHED_WIDTH = 16;
  explicit PipeExpr(std::vector<Expr *> stages,
                    std::vector<bool> parallel = {});
  void setParallel(unsigned which);
  void resolveTypes() override;
  llvm::Value *codegen0(BaseFunc *base, llvm::BasicBlock *&block) override;
  types::Type *getType0() const override;
  PipeExpr *clone(Generic *ref) override;
};

} // namespace seq

#endif /* SEQ_PIPELINE_H */
