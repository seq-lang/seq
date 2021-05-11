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

/// Commutative, binary rule that requires a single constant.
template <typename ConstantType>
class SingleConstantCommutativeRule : public FoldingRule {
public:
  using Calculator = std::function<Value *(Value *)>;

private:
  /// the value being matched against
  ConstantType val;
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
  SingleConstantCommutativeRule(ConstantType val, ConstantType newVal,
                                std::string magic, int i, types::Type *type)
      : val(val), type(type), magic(std::move(magic)), i(i) {
    calc = [=](Value *v) -> Value * {
      return v->getModule()->N<TemplatedConst<ConstantType>>(v->getSrcInfo(), val,
                                                             type);
    };
  }
  /// Constructs a commutative rule.
  /// @param val the matched value
  /// @param newVal the result
  /// @param magic the magic name
  /// @param calc the calculator
  /// @param i 0 for left, 1 for right, -1 if commutative
  /// @param type the matched type
  SingleConstantCommutativeRule(ConstantType val, Calculator calc, std::string magic,
                                int i, types::Type *type)
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

    auto *leftConst = cast<TemplatedConst<ConstantType>>(left);
    auto *rightConst = cast<TemplatedConst<ConstantType>>(right);

    if ((i == -1 || i == 0) && leftConst && leftConst->getVal() == val)
      return calc(right);
    if ((i == -1 || i == 1) && rightConst && rightConst->getVal() == val)
      return calc(left);

    return nullptr;
  }
};

/// Binary rule that requires two constants.
template <typename ConstantType, typename Func, typename OutputType = ConstantType>
class DoubleConstantBinaryRule : public FoldingRule {
private:
  /// the calculator
  Func f;
  /// the input type
  types::Type *inputType;
  /// the output type
  types::Type *resultType;
  /// the magic method name
  std::string magic;

public:
  /// Constructs a binary rule.
  /// @param f the calculator
  /// @param magic the magic method name
  /// @param inputType the input type
  /// @param resultType the output type
  DoubleConstantBinaryRule(Func f, std::string magic, types::Type *inputType,
                           types::Type *resultType)
      : f(std::move(f)), inputType(inputType), resultType(resultType),
        magic(std::move(magic)) {}

  virtual ~DoubleConstantBinaryRule() noexcept = default;

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

    if (left->getType()->getName() != inputType->getName() ||
        right->getType()->getName() != inputType->getName())
      return nullptr;

    auto *leftConst = cast<TemplatedConst<ConstantType>>(left);
    auto *rightConst = cast<TemplatedConst<ConstantType>>(right);

    if (leftConst && rightConst)
      return toValue(v, f(leftConst->getVal(), rightConst->getVal()));

    return nullptr;
  }

private:
  Value *toValue(Value *, Value *v) { return v; }

  Value *toValue(Value *og, OutputType v) {
    return og->getModule()->template N<TemplatedConst<OutputType>>(og->getSrcInfo(), v,
                                                                   resultType);
  }
};

/// Unary rule that requires one constant.
template <typename ConstantType, typename Func>
class SingleConstantUnaryRule : public FoldingRule {
private:
  /// the calculator
  Func f;
  /// the input type
  types::Type *inputType;
  /// the output type
  types::Type *resultType;
  /// the magic method name
  std::string magic;

public:
  /// Constructs a unary rule.
  /// @param f the calculator
  /// @param magic the magic method name
  /// @param inputType the input type
  /// @param resultType the output type
  SingleConstantUnaryRule(Func f, std::string magic, types::Type *inputType,
                          types::Type *resultType)
      : f(std::move(f)), inputType(inputType), resultType(resultType),
        magic(std::move(magic)) {}

  virtual ~SingleConstantUnaryRule() noexcept = default;

  Value *apply(CallInstr *v) override {
    auto *fn = util::getFunc(v->getCallee());
    if (!fn)
      return nullptr;

    if (fn->getUnmangledName() != magic)
      return nullptr;

    if (std::distance(v->begin(), v->end()) != 1)
      return nullptr;

    auto *arg = v->front();

    if (arg->getType()->getName() != inputType->getName())
      return nullptr;

    auto *argConst = cast<TemplatedConst<ConstantType>>(arg);

    if (argConst) {
      return toValue(v, f(argConst->getVal()));
    }

    return nullptr;
  }

private:
  Value *toValue(Value *, Value *v) { return v; }

  template <typename NewType> Value *toValue(Value *og, NewType v) {
    return og->getModule()->template N<TemplatedConst<NewType>>(og->getSrcInfo(), v,
                                                                resultType);
  }
};

/// Unary rule that requires no constant.
template <typename Func> class UnaryRule : public FoldingRule {
private:
  /// the calculator
  Func f;
  /// the input type
  types::Type *inputType;
  /// the magic method name
  std::string magic;

public:
  /// Constructs a unary rule.
  /// @param f the calculator
  /// @param magic the magic method name
  /// @param inputType the input type
  UnaryRule(Func f, std::string magic, types::Type *inputType)
      : f(std::move(f)), inputType(inputType), magic(std::move(magic)) {}

  virtual ~UnaryRule() noexcept = default;

  Value *apply(CallInstr *v) override {
    auto *fn = util::getFunc(v->getCallee());
    if (!fn)
      return nullptr;

    if (fn->getUnmangledName() != magic)
      return nullptr;

    if (std::distance(v->begin(), v->end()) != 1)
      return nullptr;

    auto *arg = v->front();

    if (arg->getType()->getName() != inputType->getName())
      return nullptr;

    return f(arg);
  }
};

} // namespace folding
} // namespace transform
} // namespace ir
} // namespace seq
