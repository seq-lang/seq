#pragma once

#include <unordered_map>

#include "sir/types/types.h"

#include "llvm.h"

namespace seq {
namespace ir {
namespace dsl {
namespace llvm {

struct TypeBuilder {
  virtual std::vector<types::Type *> getRequiredIRTypes() = 0;
  virtual ::llvm::Type *getLLVMType(std::unordered_map<std::string, ::llvm::Type *> typeMap) = 0;
};

}
}
}
}