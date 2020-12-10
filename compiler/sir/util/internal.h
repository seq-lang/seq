#pragma once

#include "sir/operand.h"
#include "sir/types/types.h"
#include "visitor.h"

namespace seq {
namespace ir {
namespace util {

/// Special operand wrapping LLVM values
struct LLVMOperand : public Operand {
  /// operand type
  types::Type *type;
  /// value
  llvm::Value *val;

  /// Constructs an llvm operand.
  /// @param type the type
  /// @param val the internal value
  LLVMOperand(types::Type *type, llvm::Value *val) : type(type), val(val) {}

  void accept(util::SIRVisitor &v) override { v.visit(this); }

  types::Type *getType() const override { return type; }

private:
  std::ostream &doFormat(std::ostream &os) const override { return os << "internal"; }
};

} // namespace util
} // namespace ir
} // namespace seq
