#pragma once

#include "types/types.h"

namespace seq {
namespace types {
class BaseType : public Type {
private:
  BaseType();

public:
  BaseType(BaseType const &) = delete;
  void operator=(BaseType const &) = delete;
  static BaseType *get() noexcept;
};

} // namespace types
} // namespace seq
