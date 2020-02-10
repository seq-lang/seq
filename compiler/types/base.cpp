#include "lang/seq.h"

using namespace seq;
using namespace llvm;

types::BaseType::BaseType() : Type("base", AnyType::get(), true) {}

types::BaseType *types::BaseType::get() noexcept {
  static types::BaseType instance;
  return &instance;
}
