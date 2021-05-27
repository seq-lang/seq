#include "global_vars.h"

#include "sir/util/operator.h"

namespace seq {
namespace ir {
namespace analyze {
namespace module {
namespace {
struct GlobalVarAnalyzer : public util::Operator {
  std::unordered_map<id_t, id_t> assignments;

  void handle(PointerValue *v) override {
    if (v->getVar()->isGlobal())
      assignments[v->getVar()->getId()] = -1;
  }

  void handle(AssignInstr *v) override {
    auto *lhs = v->getLhs();
    auto id = lhs->getId();
    if (lhs->isGlobal()) {
      if (assignments.find(id) != assignments.end()) {
        assignments[id] = -1;
      } else {
        assignments[id] = v->getRhs()->getId();
      }
    }
  }
};
} // namespace

std::unique_ptr<Result> GlobalVarsAnalyses::run(const Module *m) {
  GlobalVarAnalyzer gva;
  gva.visit(const_cast<Module *>(m)); // TODO: any way around this cast?
  return std::make_unique<GlobalVarsResult>(std::move(gva.assignments));
}

} // namespace module
} // namespace analyze
} // namespace ir
} // namespace seq
