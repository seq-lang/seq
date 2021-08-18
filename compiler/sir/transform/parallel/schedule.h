#pragma once

#include <string>

namespace seq {
namespace ir {

class Value;

namespace transform {
namespace parallel {

struct OMPSched {
  int code;
  bool dynamic;
  Value *threads;
  Value *chunk;

  explicit OMPSched(int code = -1, bool dynamic = false, Value *threads = nullptr,
                    Value *chunk = nullptr);
  explicit OMPSched(const std::string &code, bool dynamic = false,
                    Value *threads = nullptr, Value *chunk = nullptr);
  explicit OMPSched(const OMPSched &s)
      : code(s.code), dynamic(s.dynamic), threads(s.threads), chunk(s.chunk) {}
};

} // namespace parallel
} // namespace transform
} // namespace ir
} // namespace seq
