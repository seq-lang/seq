#include "value.h"

#include "instr.h"
#include "module.h"

namespace seq {
namespace ir {

const char Value::NodeId = 0;

Value *Value::clone() const {
  if (hasReplacement())
    return getActual()->clone();

  auto *res = doClone();
  for (auto it = attributes_begin(); it != attributes_end(); ++it) {
    auto *attr = getAttribute(*it);
    if (attr->needsClone())
      res->setAttribute(attr->clone(), *it);
  }
  return res;
}

Value *Value::operator==(Value &other) {
  return doBinaryOp(IRModule::EQ_MAGIC_NAME, other);
}

Value *Value::operator!=(Value &other) {
  return doBinaryOp(IRModule::NE_MAGIC_NAME, other);
}

Value *Value::operator<(Value &other) {
  return doBinaryOp(IRModule::LT_MAGIC_NAME, other);
}

Value *Value::operator>(Value &other) {
  return doBinaryOp(IRModule::GT_MAGIC_NAME, other);
}

Value *Value::operator<=(Value &other) {
  return doBinaryOp(IRModule::LE_MAGIC_NAME, other);
}

Value *Value::operator>=(Value &other) {
  return doBinaryOp(IRModule::GE_MAGIC_NAME, other);
}

Value *Value::operator+() { return doUnaryOp(IRModule::POS_MAGIC_NAME); }

Value *Value::operator-() { return doUnaryOp(IRModule::NEG_MAGIC_NAME); }

Value *Value::operator~() { return doUnaryOp(IRModule::INVERT_MAGIC_NAME); }

Value *Value::operator+(Value &other) {
  return doBinaryOp(IRModule::ADD_MAGIC_NAME, other);
}

Value *Value::operator-(Value &other) {
  return doBinaryOp(IRModule::SUB_MAGIC_NAME, other);
}

Value *Value::operator*(Value &other) {
  return doBinaryOp(IRModule::MUL_MAGIC_NAME, other);
}

Value *Value::trueDiv(Value &other) {
  return doBinaryOp(IRModule::TRUE_DIV_MAGIC_NAME, other);
}

Value *Value::operator/(Value &other) {
  return doBinaryOp(IRModule::FLOOR_DIV_MAGIC_NAME, other);
}

Value *Value::operator%(Value &other) {
  return doBinaryOp(IRModule::MOD_MAGIC_NAME, other);
}

Value *Value::pow(Value &other) { return doBinaryOp(IRModule::POW_MAGIC_NAME, other); }

Value *Value::operator<<(Value &other) {
  return doBinaryOp(IRModule::LSHIFT_MAGIC_NAME, other);
}

Value *Value::operator>>(Value &other) {
  return doBinaryOp(IRModule::RSHIFT_MAGIC_NAME, other);
}

Value *Value::operator&(Value &other) {
  return doBinaryOp(IRModule::AND_MAGIC_NAME, other);
}

Value *Value::operator|(Value &other) {
  return doBinaryOp(IRModule::OR_MAGIC_NAME, other);
}

Value *Value::operator^(Value &other) {
  return doBinaryOp(IRModule::XOR_MAGIC_NAME, other);
}

Value *Value::operator||(Value &other) {
  auto *module = getModule();
  return module->Nr<TernaryInstr>(toBool(), module->getBoolConstant(true),
                                  other.toBool());
}

Value *Value::operator&&(Value &other) {
  auto *module = getModule();
  return module->Nr<TernaryInstr>(toBool(), other.toBool(),
                                  module->getBoolConstant(false));
}

Value *Value::operator[](Value &other) {
  return doBinaryOp(IRModule::GET_MAGIC_NAME, other);
}

Value *Value::toInt() { return doUnaryOp(IRModule::INT_MAGIC_NAME); }

Value *Value::toBool() { return doUnaryOp(IRModule::BOOL_MAGIC_NAME); }

Value *Value::toStr() { return doUnaryOp(IRModule::STR_MAGIC_NAME); }

Value *Value::len() { return doUnaryOp(IRModule::LEN_MAGIC_NAME); }

Value *Value::iter() { return doUnaryOp(IRModule::ITER_MAGIC_NAME); }

Value *Value::doUnaryOp(const std::string &name) {
  auto *module = getModule();
  auto *fn = module->getOrRealizeMethod(getType(), name,
                                        std::vector<types::Type *>{getType()});

  if (!fn)
    return nullptr;

  auto *fnVal = module->Nr<VarValue>(fn);
  return (*fnVal)(*this);
}

Value *Value::doBinaryOp(const std::string &name, Value &other) {
  auto *module = getModule();
  auto *fn = module->getOrRealizeMethod(
      getType(), name, std::vector<types::Type *>{getType(), other.getType()});

  if (!fn)
    return nullptr;

  auto *fnVal = module->Nr<VarValue>(fn);
  return (*fnVal)(*this, other);
}

Value *Value::doCall(const vector<Value *> &args) {
  auto *module = getModule();
  return module->Nr<CallInstr>(this, args);
}

} // namespace ir
} // namespace seq
