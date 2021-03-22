#include "reaching.h"

namespace seq {
namespace ir {
namespace analyze {
namespace dataflow {

void RDInspector::analyze(CFGraph *cfg) {
  std::unordered_set<CFBlock *> workset(cfg->begin(), cfg->end());
  while (!workset.empty()) {
    std::unordered_set<CFBlock *> newWorkset;
    for (auto *blk : workset) {
      initializeIfNecessary(blk);
      calculateIn(blk);
      if (calculateOut(blk))
        newWorkset.insert(blk->successors_begin(), blk->successors_end());
    }
    workset = std::move(newWorkset);
  }
}

void RDInspector::initializeIfNecessary(CFBlock *blk) {
  if (sets.find(blk->getId()) != sets.end())
    return;
  auto &entry = sets[blk->getId()];
  for (auto *val : *blk) {
    if (auto *assign = cast<AssignInstr>(val)) {
      entry.generated[assign->getLhs()->getId()] = {assign->getRhs()->getId()};
      entry.killed.insert(assign->getLhs()->getId());
    } else if (auto *synthAssign = cast<SyntheticAssignInstr>(val)) {
      entry.killed.insert(synthAssign->getLhs()->getId());
      if (synthAssign->getKind() == SyntheticAssignInstr::KNOWN) {
        entry.generated[synthAssign->getLhs()->getId()] = {synthAssign->getArg()->getId()};
      } else {
        entry.generated[synthAssign->getLhs()->getId()] = {};
      }
    }
  }
}

void RDInspector::calculateIn(CFBlock *blk) {
  auto &curEntry = sets[blk->getId()];
  std::unordered_map<int, std::unordered_set<int>> newVal;
  for (auto it = blk->predecessors_begin(); it != blk->predecessors_end(); ++it) {
    auto *pred = *it;
    auto &predEntry = sets[pred->getId()];
    for (auto it2 = predEntry.out.begin(); it2 != predEntry.out.end(); ++it) {
      auto &loc = newVal[it2->first];
      loc.insert(it2->second.begin(), it2->second.end());
    }
  }
  curEntry.in = std::move(newVal);
}

bool RDInspector::calculateOut(CFBlock *blk) {
  auto &entry = sets[blk->getId()];
  std::unordered_map<int, std::unordered_set<int>> newOut = entry.generated;
  for (auto it = entry.in.begin(); it != entry.in.end(); ++it) {
    if (entry.killed.find(it->first) == entry.killed.end()) {
      newOut[it->first].insert(it->second.begin(), it->second.end());
    }
  }
  auto res = entry.out == newOut;
  entry.out = std::move(newOut);
  return res;
}

std::unique_ptr<Result> RDAnalysis::run(const Module *m) {
  assert(false);
  return nullptr;
}

} // namespace dataflow
} // namespace analyze
} // namespace ir
} // namespace seq
