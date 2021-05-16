#include "global_vars.h"

namespace {
using namespace seq::ir;

template <typename NodeType>
void validate(NodeType *n, analyze::module::GlobalVarsResult res) {
  if (auto *ptr = cast<PointerValue>(n)) {
    if (ptr->getVar()->isGlobal())
      res.assignments[ptr->getVar()->getId()] = -1;
  } else if (auto *assign = cast<AssignInstr>(n)) {
    if (assign->getLhs()->isGlobal()) {
      if (res.assignments.find(assign->getLhs()->getId()) != res.assignments.end()) {
        res.assignments[assign->getLhs()->getId()] = -1;
      } else {
        res.assignments[assign->getLhs()->getId()] = assign->getLhs()->getId();
      }
    }
  }

  for (auto *child : n->getUsedValues())
    validate(child, res);
}

}

namespace seq {
namespace ir {
namespace analyze {
namespace module {

std::unique_ptr<Result> GlobalVarsAnalyses::run(const Module *m) {
  auto res = std::make_unique<GlobalVarsResult>();
  for (auto *v : *m) {
    if (auto *f = cast<Func>(v))
      validate(f, *res);
  }
  validate(m->getMainFunc(), *res);
  return res;
}

}
}
}
}
