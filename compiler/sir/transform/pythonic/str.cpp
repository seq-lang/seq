#include "str.h"

#include <algorithm>

#include "sir/util/cloning.h"
#include "sir/util/irtools.h"

namespace {

using namespace seq::ir;

struct InspectionResult {
  bool valid = true;
  std::vector<Value *> args;
};

bool isString(Value *v) {
  auto *M = v->getModule();
  return v->getType()->is(M->getStringType());
}

void inspect(Value *v, InspectionResult &r) {
  // check if add first then go from there
  if (isString(v)) {
    if (auto *c = cast<CallInstr>(v)) {
      auto *func = util::getFunc(c->getCallee());
      if (func && func->getUnmangledName() == "__add__" &&
          std::distance(c->begin(), c->end()) == 2 && isString(c->front()) &&
          isString(c->back())) {
        inspect(c->front(), r);
        inspect(c->back(), r);
        return;
      }
    }
    r.args.push_back(v);
  } else {
    r.valid = false;
  }
}

} // namespace

namespace seq {
namespace ir {
namespace transform {
namespace pythonic {

void StrAdditionOptimization::handle(CallInstr *v) {
  auto *M = v->getModule();

  auto *f = util::getFunc(v->getCallee());
  if (!f || f->getUnmangledName() != "__add__")
    return;

  InspectionResult r;
  inspect(v, r);

  if (r.valid && r.args.size() > 2) {
    std::vector<Value *> args;
    util::CloneVisitor cv(M);

    for (auto *arg : r.args) {
      args.push_back(cv.clone(arg));
    }

    auto *arg = util::makeTuple(args, M);
    args = {arg};
    auto *replacementFunc =
        M->getOrRealizeMethod(M->getStringType(), "cat", {arg->getType()});
    seqassert(replacementFunc, "could not find cat function");
    v->replaceAll(util::call(replacementFunc, args));
  }
}

} // namespace pythonic
} // namespace transform
} // namespace ir
} // namespace seq
