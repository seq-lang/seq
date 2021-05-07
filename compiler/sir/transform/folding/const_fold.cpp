#include "const_fold.h"

#include "sir/util/cloning.h"

namespace {
using namespace seq::ir;

template <typename... Args>
std::unique_ptr<transform::folding::SingleConstantCommutativeRule<int64_t>>
intSingleRule(Module *m, Args &&...args) {
  return std::make_unique<transform::folding::SingleConstantCommutativeRule<int64_t>>(
      std::forward<Args>(args)..., m->getIntType());
}

auto identity(Module *m) {
  return [=](Value *v) -> Value * {
    util::CloneVisitor cv(m);
    return cv.clone(v);
  };
}
} // namespace

namespace seq {
namespace ir {
namespace transform {
namespace folding {

void FoldingPass::run(Module *m) {
  registerStandardRules(m);
  OperatorPass::run(m);
}

void FoldingPass::handle(CallInstr *v) {
  for (auto &r : rules)
    if (auto *rep = r.second->apply(v)) {
      v->replaceAll(rep);
      continue;
    }
}

void FoldingPass::registerStandardRules(Module *m) {
  registerRule("int-multiply-by-zero",
               intSingleRule(m, 0, 0, Module::MUL_MAGIC_NAME, -1));
  registerRule("int-multiply-by-one",
               intSingleRule(m, 1, Module::MUL_MAGIC_NAME, identity(m), -1));
  registerRule("int-subtract-zero",
               intSingleRule(m, 0, Module::SUB_MAGIC_NAME, identity(m), 1));
  registerRule("int-add-zero",
               intSingleRule(m, 0, Module::ADD_MAGIC_NAME, identity(m), -1));
  registerRule("int-floor-div-by-one",
               intSingleRule(m, 1, Module::FLOOR_DIV_MAGIC_NAME, identity(m), 1));
  registerRule("int-zero-floor-div",
               intSingleRule(m, 0, 0, Module::FLOOR_DIV_MAGIC_NAME, 0));
}

} // namespace folding
} // namespace transform
} // namespace ir
} // namespace seq