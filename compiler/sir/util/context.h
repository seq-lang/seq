#pragma once

#include <vector>

namespace seq {
namespace ir {
namespace util {

/// Base for SIR visitor contexts.
template <typename Frame> class IRContext {
private:
  std::vector<Frame> frames;

public:
  template <typename... Args> void pushFrame(Args... args) {
    frames.emplace_back(args...);
  }
  std::vector<Frame> &getFrames() { return frames; }
  Frame &getFrame() { return frames.back(); }
  void popFrame() { return frames.pop_back(); }
};

} // namespace util
} // namespace ir
} // namespace seq