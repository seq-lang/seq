#pragma once

#include <unordered_map>

#include "sir/types/types.h"

#include "sir/llvm/llvm.h"

namespace seq {
namespace ir {

class LLVMVisitor;

namespace dsl {
namespace codegen {

struct TypeBuilder {
  virtual ~TypeBuilder() noexcept = default;

  virtual llvm::Type *buildType(LLVMVisitor *visitor) = 0;
  virtual llvm::DIType *buildDebugType(LLVMVisitor *visitor) = 0;
};

struct ValueBuilder {
  virtual ~ValueBuilder() noexcept = default;

  virtual llvm::Value *buildValue(LLVMVisitor *visitor) = 0;
};

} // namespace codegen
} // namespace dsl
} // namespace ir
} // namespace seq
