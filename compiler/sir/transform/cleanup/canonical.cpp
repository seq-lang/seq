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

  Rank getRank() { return std::make_tuple(-maxDepth, hash); }
};

NodeRanker::Rank getRank(Node *node) {
  NodeRanker ranker;
  node->accept(ranker);
  return ranker.getRank();
}

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

bool isInequalityOp(const std::string &name) {
  static const std::unordered_set<std::string> ops = {
      Module::EQ_MAGIC_NAME, Module::NE_MAGIC_NAME, Module::LT_MAGIC_NAME,
      Module::LE_MAGIC_NAME, Module::GT_MAGIC_NAME, Module::GE_MAGIC_NAME};
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
    rankedOperands.push_back({getRank(v), v});
  }
  std::sort(rankedOperands.begin(), rankedOperands.end());

  operands.clear();
  for (auto &p : rankedOperands) {
    operands.push_back(std::get<1>(p));
  }
}

bool varMatch(Value *a, Value *b) {
  auto *v1 = cast<VarValue>(a);
  auto *v2 = cast<VarValue>(b);
  return v1 && v2 && v1->getVar()->getId() == v2->getVar()->getId();
}

// (a + b) * x, or null if invalid
Value *addMul(Value *a, Value *b, Value *x) {
  if (!a || !b || !x)
    return nullptr;
  auto *y = (*a + *b);
  if (!y)
    y = (*b + *a);
  if (!y)
    return nullptr;
  auto *z = (*y) * (*x);
  if (!z)
    z = (*x) * (*y);
  return z;
}

//  a*x + b*x --> (a + b) * x
CallInstr *convertAddMul(CallInstr *v) {
  auto *M = v->getModule();
  auto *type = v->getType();

  if (!util::isCallOf(v, Module::ADD_MAGIC_NAME, {type, type}, type, /*method=*/true))
    return v;

  // decompose the operation
  Value *lhs = v->front();
  Value *rhs = v->back();
  Value *lhs1 = nullptr, *lhs2 = nullptr, *rhs1 = nullptr, *rhs2 = nullptr;

  if (util::isCallOf(lhs, Module::MUL_MAGIC_NAME, {type, type}, type,
                     /*method=*/true)) {
    auto *lhsCall = cast<CallInstr>(lhs);
    lhs1 = lhsCall->front();
    lhs2 = lhsCall->back();
  } else {
    lhs1 = lhs;
    lhs2 = M->getInt(1);
  }

  if (util::isCallOf(rhs, Module::MUL_MAGIC_NAME, {type, type}, type,
                     /*method=*/true)) {
    auto *rhsCall = cast<CallInstr>(rhs);
    rhs1 = rhsCall->front();
    rhs2 = rhsCall->back();
  } else {
    rhs1 = rhs;
    rhs2 = M->getInt(1);
  }

  Value *newCall = nullptr;
  if (varMatch(lhs1, rhs1)) {
    newCall = addMul(lhs2, rhs2, lhs1);
  } else if (varMatch(lhs1, rhs2)) {
    newCall = addMul(lhs2, rhs1, lhs1);
  } else if (varMatch(lhs2, rhs1)) {
    newCall = addMul(lhs1, rhs2, lhs2);
  } else if (varMatch(lhs2, rhs2)) {
    newCall = addMul(lhs1, rhs1, lhs2);
  }

  if (newCall && newCall->getType()->is(type))
    return cast<CallInstr>(newCall);
  return v;
}
} // namespace

void CanonicalizationPass::handle(CallInstr *v) {
  auto *fn = util::getFunc(v->getCallee());
  if (!fn)
    return;

  std::string op = fn->getUnmangledName();
  types::Type *type = v->getType();
  const bool isAssociative = isAssociativeOp(op);
  const bool isCommutative = isCommutativeOp(op);
  const bool isInequality = isInequalityOp(op);

  // canonicalize inequalities
  if (v->numArgs() == 2 && isInequality) {
    Value *newCall = nullptr;
    auto *lhs = v->front();
    auto *rhs = v->back();
    if (getRank(lhs) > getRank(rhs)) { // are we out of order?
      // re-order
      if (op == Module::EQ_MAGIC_NAME) { // lhs == rhs
        newCall = *rhs == *lhs;
      } else if (op == Module::NE_MAGIC_NAME) { // lhs != rhs
        newCall = *rhs != *lhs;
      } else if (op == Module::LT_MAGIC_NAME) { // lhs < rhs
        newCall = *rhs > *lhs;
      } else if (op == Module::LE_MAGIC_NAME) { // lhs <= rhs
        newCall = *rhs >= *lhs;
      } else if (op == Module::GT_MAGIC_NAME) { // lhs > rhs
        newCall = *rhs < *lhs;
      } else if (op == Module::GE_MAGIC_NAME) { // lhs >= rhs
        newCall = *rhs <= *lhs;
      } else {
        seqassert(false, "unknown comparison op: {}", op);
      }

      if (newCall && newCall->getType()->is(type))
        v->replaceAll(newCall);
    }
    return;
  }

  // convert [a*x + b*x] --> (a + b) * x
  CallInstr *newCall = convertAddMul(v);

  // rearrange associative/commutative ops
  if (util::isCallOf(newCall, op, {type, type}, type, /*method=*/true)) {
    std::vector<Value *> operands;
    if (isAssociative) {
      extractAssociativeOpChain(newCall, op, type, operands);
    } else {
      operands.push_back(newCall->front());
      operands.push_back(newCall->back());
    }
    seqassert(operands.size() >= 2, "bad call canonicalization");

    if (isCommutative)
      orderOperands(operands);

    newCall = util::call(fn, {operands[0], operands[1]});
    for (auto it = operands.begin() + 2; it != operands.end(); ++it) {
      newCall = util::call(fn, {newCall, *it});
    }
  }

  if (newCall != v) {
    seqassert(newCall->getType()->is(type), "inconsistent types");
    v->replaceAll(newCall);
  }
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
