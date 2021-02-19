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

Value *Value::operator==(const Value &other) const {
  return doBinaryOp(IRModule::EQ_MAGIC_NAME, other, getModule()->getBoolType());
}

Value *Value::operator!=(const Value &other) const {
  return doBinaryOp(IRModule::NE_MAGIC_NAME, other, getModule()->getBoolType());
}

Value *Value::operator<(const Value &other) const {
  return doBinaryOp(IRModule::LT_MAGIC_NAME, other, getModule()->getBoolType());
}

Value *Value::operator>(const Value &other) const {
  return doBinaryOp(IRModule::GT_MAGIC_NAME, other, getModule()->getBoolType());
}

Value *Value::operator<=(const Value &other) const {
  return doBinaryOp(IRModule::LE_MAGIC_NAME, other, getModule()->getBoolType());
}

Value *Value::operator>=(const Value &other) const {
  return doBinaryOp(IRModule::GE_MAGIC_NAME, other, getModule()->getBoolType());
}

Value *Value::operator+() const { return doUnaryOp(IRModule::POS_MAGIC_NAME); }

Value *Value::operator-() const { return doUnaryOp(IRModule::NEG_MAGIC_NAME); }

Value *Value::operator~() const { return doUnaryOp(IRModule::INVERT_MAGIC_NAME); }

Value *Value::operator+(const Value &other) const {
  return doBinaryOp(IRModule::ADD_MAGIC_NAME, other);
}

Value *Value::operator-(const Value &other) const {
  return doBinaryOp(IRModule::SUB_MAGIC_NAME, other);
}

Value *Value::operator*(const Value &other) const {
  return doBinaryOp(IRModule::MUL_MAGIC_NAME, other);
}

Value *Value::floorDiv(const Value &other) const {
  return doBinaryOp(IRModule::FLOOR_DIV_MAGIC_NAME, other);
}

Value *Value::operator/(const Value &other) const {
  return doBinaryOp(IRModule::DIV_MAGIC_NAME, other);
}

Value *Value::operator%(const Value &other) const {
  return doBinaryOp(IRModule::MOD_MAGIC_NAME, other);
}

Value *Value::pow(const Value &other) const {
  return doBinaryOp(IRModule::FLOOR_DIV_MAGIC_NAME, other);
}

Value *Value::operator<<(const Value &other) const {
  return doBinaryOp(IRModule::LSHIFT_MAGIC_NAME, other);
}

Value *Value::operator>>(const Value &other) const {
  return doBinaryOp(IRModule::RSHIFT_MAGIC_NAME, other);
}

Value *Value::operator&(const Value &other) const {
  return doBinaryOp(IRModule::AND_MAGIC_NAME, other);
}

Value *Value::operator|(const Value &other) const {
  return doBinaryOp(IRModule::OR_MAGIC_NAME, other);
}

Value *Value::operator^(const Value &other) const {
  return doBinaryOp(IRModule::XOR_MAGIC_NAME, other);
}

Value *Value::operator||(const Value &other) const {
  auto *module = getModule();
  return module->Nr<TernaryInstr>(toBool(), module->getBoolConstant(true),
                                  other.toBool());
}

Value *Value::operator&&(const Value &other) const {
  auto *module = getModule();
  return module->Nr<TernaryInstr>(toBool(), other.toBool(),
                                  module->getBoolConstant(false));
}

Value *Value::toInt() const {
  return doUnaryOp(IRModule::INT_MAGIC_NAME, getModule()->getIntType());
}

Value *Value::toBool() const {
  return doUnaryOp(IRModule::BOOL_MAGIC_NAME, getModule()->getBoolType());
}

Value *Value::toStr() const {
  return doUnaryOp(IRModule::STR_MAGIC_NAME, getModule()->getStringType());
}

Value *Value::len() const {
  return doUnaryOp(IRModule::LEN_MAGIC_NAME, getModule()->getIntType());
}

Value *Value::doUnaryOp(const string &name, const types::Type *type) const {
  auto *module = getModule();
  auto *resType = type ? type : getType();
  auto *fn = module->getOrRealizeMethod(getType(), name, resType,
                                        std::vector<const types::Type *>{getType()});

  if (!fn)
    return nullptr;

  auto *fnVal = module->Nr<VarValue>(fn);
  return (*fnVal)(*this);
}

Value *Value::doBinaryOp(const string &name, const Value &other,
                         const types::Type *type) const {
  auto *module = getModule();
  auto *resType = type ? type : getType();
  auto *fn = module->getOrRealizeMethod(
      getType(), name, resType,
      std::vector<const types::Type *>{getType(), other.getType()});

  if (!fn)
    return nullptr;

  auto *fnVal = module->Nr<VarValue>(fn);
  return (*fnVal)(*this, other);
}

Value *Value::doCall(const vector<Value *> &args) const {
  auto *module = getModule();
  return module->Nr<CallInstr>(const_cast<Value *>(this), args);
}

} // namespace ir
} // namespace seq
