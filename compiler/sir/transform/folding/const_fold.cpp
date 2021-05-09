#include "const_fold.h"

#include "sir/util/cloning.h"

#define BINOP(o)                                                                       \
  [](auto x, auto y) -> auto { return x o y; }
#define UNOP(o)                                                                        \
  [](auto x) -> auto { return o x; }
#define ID                                                                             \
  [](auto x) -> auto { return x; }

namespace {
using namespace seq::ir;

template <typename... Args> auto intSingleRule(Module *m, Args &&...args) {
  return std::make_unique<transform::folding::SingleConstantCommutativeRule<int64_t>>(
      std::forward<Args>(args)..., m->getIntType());
}

auto id_val(Module *m) {
  return [=](Value *v) -> Value * {
    util::CloneVisitor cv(m);
    return cv.clone(v);
  };
}

template <typename T, typename Func>
auto doubleConstantBinaryRule(Func f, std::string magic, types::Type *input,
                              types::Type *output) {
  return std::make_unique<transform::folding::DoubleConstantBinaryRule<T, Func>>(
      std::move(f), std::move(magic), input, output);
}

template <typename Func> auto intToIntBinary(Module *m, Func f, std::string magic) {
  return doubleConstantBinaryRule<int64_t>(f, std::move(magic), m->getIntType(),
                                           m->getIntType());
}

template <typename Func> auto intToBoolBinary(Module *m, Func f, std::string magic) {
  return doubleConstantBinaryRule<int64_t>(f, std::move(magic), m->getIntType(),
                                           m->getBoolType());
}

template <typename Func> auto boolToBool(Module *m, Func f, std::string magic) {
  return doubleConstantBinaryRule<bool>(f, std::move(magic), m->getBoolType(),
                                        m->getBoolType());
}

template <typename T, typename Func>
auto singleConstantUnaryRule(Func f, std::string magic, types::Type *input,
                             types::Type *output) {
  return std::make_unique<transform::folding::SingleConstantUnaryRule<T, Func>>(
      std::move(f), std::move(magic), input, output);
}

template <typename Func> auto intToIntUnary(Module *m, Func f, std::string magic) {
  return singleConstantUnaryRule<int64_t>(f, std::move(magic), m->getIntType(),
                                          m->getIntType());
}

template <typename Func> auto boolToBoolUnary(Module *m, Func f, std::string magic) {
  return singleConstantUnaryRule<bool>(f, std::move(magic), m->getBoolType(),
                                       m->getBoolType());
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
}

} // namespace folding
} // namespace transform
} // namespace ir
} // namespace seq

#undef BINOP
#undef UNOP
#undef ID
