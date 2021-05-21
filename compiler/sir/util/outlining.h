#pragma once

#include "sir/sir.h"

namespace seq {
namespace ir {
namespace util {

struct OutlineResult {
  /// The outlined function
  BodiedFunc *func = nullptr;
  /// The call to the outlined function
  CallInstr *call = nullptr;
  /// Whether the above call indicates complex control flow.
  /// For example, an outlined function that contains a "break"
  /// of a non-outlined loop will return an integer code that
  /// tells the callee to perform this break. A series of
  /// if-statements are added to the call site to check the
  /// returned code and perform the correct action.
  bool callIndicatesControl = false;

  operator bool() const { return bool(func); }
};

OutlineResult outlineRegion(BodiedFunc *parent, SeriesFlow *series,
                            decltype(series->begin()) begin,
                            decltype(series->end()) end);
OutlineResult outlineRegion(BodiedFunc *parent, SeriesFlow *series) {
  return outlineRegion(parent, series, series->begin(), series->end());
}

} // namespace util
} // namespace ir
} // namespace seq
