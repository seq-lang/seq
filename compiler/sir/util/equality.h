#pragma once

#include "sir/base.h"

namespace seq {
namespace ir {
namespace util {

/// Checks if two IR nodes are equal.
/// @param a the first IR node
/// @param b the second IR node
/// @return true if the nodes are equal
bool areEqual(IRNode *a, IRNode *b);

} // namespace util
} // namespace ir
} // namespace seq
