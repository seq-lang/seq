#pragma once

#include <vector>

namespace seq {
namespace ir {
namespace util {

template <typename Desired>
void stripPack(std::vector<Desired *> &dst, Desired &first) {
  dst.push_back(&first);
}

template <typename Desired> void stripPack(std::vector<Desired *> &dst) {}

template <typename Desired, typename... Args>
void stripPack(std::vector<Desired *> &dst, Desired &first, Args &&... args) {
  dst.push_back(&first);
  stripPack<Desired>(dst, std::forward<Args>(args)...);
}

} // namespace util
} // namespace ir
} // namespace seq
