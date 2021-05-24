#include "canonical.h"

#include <algorithm>
#include <functional>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "sir/util/irtools.h"

namespace seq {
namespace ir {
namespace transform {
namespace cleanup {
namespace {
struct NodeRanker : public util::Operator {
  using Rank = std::tuple<int, uint64_t>;
  int maxDepth = 0;
  uint64_t hash = 0;

  // boost's hash_combine
  template <class T> void hash_combine(const T &v) {
    std::hash<T> hasher;
    hash ^= hasher(v) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  }

  void preHook(Node *node) {
    maxDepth = std::max(maxDepth, depth());
    hash_combine(node->nodeId());
    for (auto *v : node->getUsedVariables()) {
      hash_combine(v->getName());
    }
    for (auto *v : node->getUsedTypes()) {
      hash_combine(v->getName());
    }
  }

  Rank getRank() { return {maxDepth, hash}; }
};

bool isAssociativeOp(const std::string &name) {
  static const std::unordered_set<std::string> ops = {
      Module::ADD_MAGIC_NAME, Module::MUL_MAGIC_NAME, Module::AND_MAGIC_NAME,
      Module::OR_MAGIC_NAME, Module::XOR_MAGIC_NAME};
  return ops.find(name) != ops.end();
}

bool isCommutativeOp(const std::string &name) {
  static const std::unordered_set<std::string> ops = {
      Module::ADD_MAGIC_NAME, Module::MUL_MAGIC_NAME, Module::AND_MAGIC_NAME,
      Module::OR_MAGIC_NAME,  Module::XOR_MAGIC_NAME, Module::EQ_MAGIC_NAME,
      Module::NE_MAGIC_NAME};
  return ops.find(name) != ops.end();
}

void extractAssociativeOpChain(Value *v, const std::string &op, types::Type *type,
                               std::vector<Value *> &result) {
  if (util::isCallOf(v, op, {type, type}, type, /*method=*/true)) {
    auto *call = cast<CallInstr>(v);
    extractAssociativeOpChain(call->front(), op, type, result);
    extractAssociativeOpChain(call->back(), op, type, result);
  } else {
    result.push_back(v);
  }
}

void orderOperands(std::vector<Value *> &operands) {
  std::vector<std::pair<NodeRanker::Rank, Value *>> rankedOperands;
  for (auto *v : operands) {
    NodeRanker ranker;
    v->accept(ranker);
    rankedOperands.push_back({ranker.getRank(), v});
  }
  std::sort(rankedOperands.begin(), rankedOperands.end());

  operands.clear();
  for (auto &p : rankedOperands) {
    operands.push_back(std::get<1>(p));
  }
}
} // namespace

void CanonicalizationPass::handle(CallInstr *v) {
  auto *fn = util::getFunc(v->getCallee());
  if (!fn)
    return;

  std::string op = fn->getUnmangledName();
  if (!isAssociativeOp(op))
    return;

  types::Type *type = v->getType();
  if (!util::isCallOf(v, op, {type, type}, type, /*method=*/true))
    return;

  std::vector<Value *> operands;
  if (isAssociativeOp(op)) {
    extractAssociativeOpChain(v, op, type, operands);
  } else {
    operands.push_back(v->front());
    operands.push_back(v->back());
  }
  seqassert(operands.size() >= 2, "bad call canonicalization");

  if (isCommutativeOp(op))
    orderOperands(operands);

  CallInstr *newCall = util::call(fn, {operands[0], operands[1]});
  for (auto it = operands.begin() + 2; it != operands.end(); ++it) {
    newCall = util::call(fn, {newCall, *it});
  }

  v->replaceAll(newCall);
}

void CanonicalizationPass::handle(SeriesFlow *v) {
  auto it = v->begin();
  while (it != v->end()) {
    if (auto *series = cast<SeriesFlow>(*it)) {
      it = v->erase(it);
      for (auto *x : *series) {
        it = v->insert(it, x);
        ++it;
      }
    } else {
      ++it;
    }
  }
}

} // namespace cleanup
} // namespace transform
} // namespace ir
} // namespace seq
