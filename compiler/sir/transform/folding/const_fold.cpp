#include "const_fold.h"

#include <cmath>

#include "sir/util/cloning.h"
#include "sir/util/irtools.h"

#define BINOP(o)                                                                       \
  [](auto x, auto y) -> auto { return x o y; }
#define UNOP(o)                                                                        \
  [](auto x) -> auto { return o x; }
#define ID                                                                             \
  [](auto x) -> auto { return x; }

namespace {
using namespace seq::ir;

template <typename Func, typename Out>
class IntFloatBinaryRule : public transform::folding::FoldingRule {
private:
  Func f;
  std::string magic;
  types::Type *out;

public:
  IntFloatBinaryRule(Func f, std::string magic, types::Type *out)
      : f(std::move(f)), magic(std::move(magic)), out(out) {}
  virtual ~IntFloatBinaryRule() noexcept = default;
  Value *apply(CallInstr *v) override {
    auto *fn = util::getFunc(v->getCallee());
    if (!fn)
      return nullptr;

    if (fn->getUnmangledName() != magic)
      return nullptr;

    if (std::distance(v->begin(), v->end()) != 2)
      return nullptr;

    auto *leftConst = cast<Const>(v->front());
    auto *rightConst = cast<Const>(v->back());

    if (!leftConst || !rightConst)
      return nullptr;

    auto *M = v->getModule();
    if (isA<FloatConst>(leftConst) && isA<IntConst>(rightConst)) {
      auto left = cast<FloatConst>(leftConst)->getVal();
      auto right = cast<IntConst>(rightConst)->getVal();
      return M->template N<TemplatedConst<Out>>(v->getSrcInfo(), f(left, (double)right),
                                                out);
    } else if (isA<IntConst>(leftConst) && isA<FloatConst>(rightConst)) {
      auto left = cast<IntConst>(leftConst)->getVal();
      auto right = cast<FloatConst>(rightConst)->getVal();
      return M->template N<TemplatedConst<Out>>(v->getSrcInfo(), f((double)left, right),
                                                out);
    } else {
      return nullptr;
    }
  }
};

auto id_val(Module *m) {
  return [=](Value *v) -> Value * {
    util::CloneVisitor cv(m);
    return cv.clone(v);
  };
}

int64_t int_pow(int64_t base, int64_t exp) {
  if (exp < 0)
    return 0;
  int64_t result = 1;
  while (true) {
    if (exp & 1) {
      result *= base;
    }
    exp = exp >> 1;
    if (!exp)
      break;
    base = base * base;
  }
  return result;
}

template <typename... Args> auto intSingleRule(Module *m, Args &&...args) {
  return std::make_unique<transform::folding::SingleConstantCommutativeRule<int64_t>>(
      std::forward<Args>(args)..., m->getIntType());
}

template <typename Func> auto intToIntBinary(Module *m, Func f, std::string magic) {
  return std::make_unique<
      transform::folding::DoubleConstantBinaryRule<int64_t, Func, int64_t>>(
      std::move(f), std::move(magic), m->getIntType(), m->getIntType());
}

template <typename Func> auto intToBoolBinary(Module *m, Func f, std::string magic) {
  return std::make_unique<
      transform::folding::DoubleConstantBinaryRule<int64_t, Func, bool>>(
      std::move(f), std::move(magic), m->getIntType(), m->getBoolType());
}

template <typename Func> auto boolToBoolBinary(Module *m, Func f, std::string magic) {
  return std::make_unique<
      transform::folding::DoubleConstantBinaryRule<bool, Func, bool>>(
      std::move(f), std::move(magic), m->getBoolType(), m->getBoolType());
}

template <typename Func> auto floatToFloatBinary(Module *m, Func f, std::string magic) {
  return std::make_unique<
      transform::folding::DoubleConstantBinaryRule<double, Func, double>>(
      std::move(f), std::move(magic), m->getFloatType(), m->getFloatType());
}

template <typename Func> auto floatToBoolBinary(Module *m, Func f, std::string magic) {
  return std::make_unique<
      transform::folding::DoubleConstantBinaryRule<double, Func, bool>>(
      std::move(f), std::move(magic), m->getFloatType(), m->getBoolType());
}

template <typename Func>
auto intFloatToFloatBinary(Module *m, Func f, std::string magic) {
  return std::make_unique<IntFloatBinaryRule<Func, double>>(
      std::move(f), std::move(magic), m->getFloatType());
}

template <typename Func>
auto intFloatToBoolBinary(Module *m, Func f, std::string magic) {
  return std::make_unique<IntFloatBinaryRule<Func, bool>>(
      std::move(f), std::move(magic), m->getBoolType());
}

template <typename Func> auto intToIntUnary(Module *m, Func f, std::string magic) {
  return std::make_unique<transform::folding::SingleConstantUnaryRule<int64_t, Func>>(
      std::move(f), std::move(magic), m->getIntType(), m->getIntType());
}

template <typename Func> auto boolToBoolUnary(Module *m, Func f, std::string magic) {
  return std::make_unique<transform::folding::SingleConstantUnaryRule<bool, Func>>(
      std::move(f), std::move(magic), m->getBoolType(), m->getBoolType());
}

} // namespace

namespace seq {
namespace ir {
namespace transform {
namespace folding {

void FoldingPass::run(Module *m) {
  registerStandardRules(m);
  numReplacements = 0;
  OperatorPass::run(m);
}

void FoldingPass::handle(CallInstr *v) {
  for (auto &r : rules)
    if (auto *rep = r.second->apply(v)) {
      ++numReplacements;
      v->replaceAll(rep);
      continue;
    }
}

void FoldingPass::registerStandardRules(Module *m) {
  // binary, single constant, int->int
  registerRule("int-multiply-by-zero",
               intSingleRule(m, 0, 0, Module::MUL_MAGIC_NAME, -1));
  registerRule("int-multiply-by-one",
               intSingleRule(m, 1, id_val(m), Module::MUL_MAGIC_NAME, -1));
  registerRule("int-subtract-zero",
               intSingleRule(m, 0, id_val(m), Module::SUB_MAGIC_NAME, 1));
  registerRule("int-add-zero",
               intSingleRule(m, 0, id_val(m), Module::ADD_MAGIC_NAME, -1));
  registerRule("int-floor-div-by-one",
               intSingleRule(m, 1, id_val(m), Module::FLOOR_DIV_MAGIC_NAME, 1));
  registerRule("int-zero-floor-div",
               intSingleRule(m, 0, 0, Module::FLOOR_DIV_MAGIC_NAME, 0));

  // binary, double constant, int->int
  registerRule("int-constant-addition",
               intToIntBinary(m, BINOP(+), Module::ADD_MAGIC_NAME));
  registerRule("int-constant-subtraction",
               intToIntBinary(m, BINOP(-), Module::SUB_MAGIC_NAME));
  registerRule("int-constant-floor-div",
               intToIntBinary(m, BINOP(/), Module::FLOOR_DIV_MAGIC_NAME));
  registerRule("int-constant-mul", intToIntBinary(m, BINOP(*), Module::MUL_MAGIC_NAME));
  registerRule("int-constant-lshift",
               intToIntBinary(m, BINOP(<<), Module::LSHIFT_MAGIC_NAME));
  registerRule("int-constant-rshift",
               intToIntBinary(m, BINOP(>>), Module::RSHIFT_MAGIC_NAME));
  registerRule("int-constant-pow", intToIntBinary(m, int_pow, Module::POW_MAGIC_NAME));
  registerRule("int-constant-xor", intToIntBinary(m, BINOP(^), Module::XOR_MAGIC_NAME));
  registerRule("int-constant-or", intToIntBinary(m, BINOP(|), Module::OR_MAGIC_NAME));
  registerRule("int-constant-and", intToIntBinary(m, BINOP(&), Module::AND_MAGIC_NAME));
  registerRule("int-constant-mod", intToIntBinary(m, BINOP(%), Module::MOD_MAGIC_NAME));

  // binary, double constant, int->bool
  registerRule("int-constant-eq", intToBoolBinary(m, BINOP(==), Module::EQ_MAGIC_NAME));
  registerRule("int-constant-ne", intToBoolBinary(m, BINOP(!=), Module::NE_MAGIC_NAME));
  registerRule("int-constant-gt", intToBoolBinary(m, BINOP(>), Module::GT_MAGIC_NAME));
  registerRule("int-constant-ge", intToBoolBinary(m, BINOP(>=), Module::GE_MAGIC_NAME));
  registerRule("int-constant-lt", intToBoolBinary(m, BINOP(<), Module::LT_MAGIC_NAME));
  registerRule("int-constant-le", intToBoolBinary(m, BINOP(<=), Module::LE_MAGIC_NAME));

  // binary, double constant, bool->bool
  registerRule("bool-constant-xor",
               boolToBoolBinary(m, BINOP(^), Module::XOR_MAGIC_NAME));
  registerRule("bool-constant-or",
               boolToBoolBinary(m, BINOP(|), Module::OR_MAGIC_NAME));
  registerRule("bool-constant-and",
               boolToBoolBinary(m, BINOP(&), Module::AND_MAGIC_NAME));

  // unary, single constant, int->int
  registerRule("int-constant-int",
               std::make_unique<UnaryRule<decltype(id_val(m))>>(
                   id_val(m), Module::INT_MAGIC_NAME, m->getIntType()));
  registerRule("int-constant-pos", intToIntUnary(m, UNOP(+), Module::POS_MAGIC_NAME));
  registerRule("int-constant-neg", intToIntUnary(m, UNOP(-), Module::NEG_MAGIC_NAME));
  registerRule("int-constant-inv",
               intToIntUnary(m, UNOP(~), Module::INVERT_MAGIC_NAME));

  // unary, single constant, bool->bool
  registerRule("bool-constant-bool",
               std::make_unique<UnaryRule<decltype(id_val(m))>>(
                   id_val(m), Module::BOOL_MAGIC_NAME, m->getBoolType()));
  registerRule("bool-constant-inv",
               boolToBoolUnary(m, UNOP(!), Module::INVERT_MAGIC_NAME));

  // binary, double constant, float->float
  registerRule("float-constant-addition",
               floatToFloatBinary(m, BINOP(+), Module::ADD_MAGIC_NAME));
  registerRule("float-constant-subtraction",
               floatToFloatBinary(m, BINOP(-), Module::SUB_MAGIC_NAME));
  registerRule("float-constant-floor-div",
               floatToFloatBinary(m, BINOP(/), Module::TRUE_DIV_MAGIC_NAME));
  registerRule("float-constant-mul",
               floatToFloatBinary(m, BINOP(*), Module::MUL_MAGIC_NAME));
  registerRule(
      "float-constant-pow",
      floatToFloatBinary(
          m, [](auto a, auto b) { return std::pow(a, b); }, Module::POW_MAGIC_NAME));

  // binary, double constant, float->bool
  registerRule("float-constant-eq",
               floatToBoolBinary(m, BINOP(==), Module::EQ_MAGIC_NAME));
  registerRule("float-constant-ne",
               floatToBoolBinary(m, BINOP(!=), Module::NE_MAGIC_NAME));
  registerRule("float-constant-gt",
               floatToBoolBinary(m, BINOP(>), Module::GT_MAGIC_NAME));
  registerRule("float-constant-ge",
               floatToBoolBinary(m, BINOP(>=), Module::GE_MAGIC_NAME));
  registerRule("float-constant-lt",
               floatToBoolBinary(m, BINOP(<), Module::LT_MAGIC_NAME));
  registerRule("float-constant-le",
               floatToBoolBinary(m, BINOP(<=), Module::LE_MAGIC_NAME));

  // binary, double constant, int,float->float
  registerRule("int-float-constant-addition",
               intFloatToFloatBinary(m, BINOP(+), Module::ADD_MAGIC_NAME));
  registerRule("int-float-constant-subtraction",
               intFloatToFloatBinary(m, BINOP(-), Module::SUB_MAGIC_NAME));
  registerRule("int-float-constant-floor-div",
               intFloatToFloatBinary(m, BINOP(/), Module::TRUE_DIV_MAGIC_NAME));
  registerRule("int-float-constant-mul",
               intFloatToFloatBinary(m, BINOP(*), Module::MUL_MAGIC_NAME));

  // binary, double constant, int,float->bool
  registerRule("int-float-constant-eq",
               intFloatToBoolBinary(m, BINOP(==), Module::EQ_MAGIC_NAME));
  registerRule("int-float-constant-ne",
               intFloatToBoolBinary(m, BINOP(!=), Module::NE_MAGIC_NAME));
  registerRule("int-float-constant-gt",
               intFloatToBoolBinary(m, BINOP(>), Module::GT_MAGIC_NAME));
  registerRule("int-float-constant-ge",
               intFloatToBoolBinary(m, BINOP(>=), Module::GE_MAGIC_NAME));
  registerRule("int-float-constant-lt",
               intFloatToBoolBinary(m, BINOP(<), Module::LT_MAGIC_NAME));
  registerRule("int-float-constant-le",
               intFloatToBoolBinary(m, BINOP(<=), Module::LE_MAGIC_NAME));
}

} // namespace folding
} // namespace transform
} // namespace ir
} // namespace seq

#undef BINOP
#undef UNOP
#undef ID
