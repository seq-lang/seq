#pragma once

#include <algorithm>
#include <utility>

#include "sir/transform/pass.h"
#include "sir/util/irtools.h"

namespace seq {
namespace ir {
namespace transform {
namespace folding {

/// Base for folding rules.
class FoldingRule {
public:
  virtual ~FoldingRule() noexcept = default;

  /// Apply the rule.
  /// @param v the instruction
  /// @return nullptr if no match, the replacement otherwise
  virtual Value *apply(CallInstr *v) = 0;
};

/// Commutative, binary rule that matches a single constant value.
template <typename T> class SingleConstantCommutativeRule : public FoldingRule {
public:
  using Calculator = std::function<Value *(Value *)>;

private:
  /// the value being matched against
  T val;
  /// the type being matched
  types::Type *type;
  /// the magic method name
  std::string magic;
  /// the calculator
  Calculator calc;
  /// the index for the constant
  int i;

public:
  /// Constructs a commutative rule.
  /// @param val the matched value
  /// @param newVal the result
  /// @param magic the magic name
  /// @param i 0 for left, 1 for right, -1 if commutative
  /// @param type the matched type
  SingleConstantCommutativeRule(T val, T newVal, std::string magic, int i,
                                types::Type *type)
      : val(val), type(type), magic(std::move(magic)), i(i) {
    calc = [=](Value *v) -> Value * {
      return v->getModule()->Nr<TemplatedConst<T>>(val, type);
    };
  }
  /// Constructs a commutative rule.
  /// @param val the matched value
  /// @param newVal the result
  /// @param magic the magic name
  /// @param calc the calculator
  /// @param i 0 for left, 1 for right, -1 if commutative
  /// @param type the matched type
  SingleConstantCommutativeRule(T val, std::string magic, Calculator calc, int i,
                                types::Type *type)
      : val(val), type(type), magic(std::move(magic)), calc(std::move(calc)), i(i) {}

  virtual ~SingleConstantCommutativeRule() noexcept = default;

  Value *apply(CallInstr *v) override {
    auto *fn = util::getFunc(v->getCallee());
    if (!fn)
      return nullptr;

    if (fn->getUnmangledName() != magic)
      return nullptr;

    if (std::distance(v->begin(), v->end()) != 2)
      return nullptr;

    auto *left = v->front();
    auto *right = v->back();

    if (left->getType()->getName() != type->getName() ||
        right->getType()->getName() != type->getName())
      return nullptr;

    auto *leftConst = cast<TemplatedConst<T>>(left);
    auto *rightConst = cast<TemplatedConst<T>>(right);

    if ((i == -1 || i == 0) && leftConst && leftConst->getVal() == val)
      return calc(right);
    if ((i == -1 || i == 1) && rightConst && rightConst->getVal() == val)
      return calc(left);

    return nullptr;
  }
};

} // namespace folding
} // namespace transform
} // namespace ir
} // namespace seq