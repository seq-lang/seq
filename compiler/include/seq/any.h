#ifndef SEQ_ANY_H
#define SEQ_ANY_H

#include "types.h"

namespace seq {
namespace types {
class AnyType : public Type {
  AnyType();

public:
  AnyType(AnyType const &) = delete;
  void operator=(AnyType const &) = delete;
  static AnyType *get() noexcept;
};

} // namespace types
} // namespace seq

#endif /* SEQ_ANY_H */
