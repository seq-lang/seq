#pragma once

#include "sir/sir.h"

namespace seq {
namespace ir {
namespace util {

/// The result of an outlining operation.
struct OutlineResult {
  /// The outlined function
  BodiedFunc *func = nullptr;
  /// The call to the outlined function
  CallInstr *call = nullptr;
  /// Number of externally-handled control flows.
  /// For example, an outlined function that contains a "break"
  /// of a non-outlined loop will return an integer code that
  /// tells the callee to perform this break. A series of
  /// if-statements are added to the call site to check the
  /// returned code and perform the correct action. This value
  /// is the number of if-statements generated. If it is zero,
  /// the function returns void and no such checks are done.
  int numOutFlows = 0;

  operator bool() const { return bool(func); }
};

/// Outlines a region of IR delineated by begin and end iterators
/// on a particular series flow. The outlined code will be replaced
/// by a call to the outlined function, and possibly extra logic if
/// control flow needs to be handled.
/// @param parent the function containing the series flow
/// @param series the series flow on which outlining will happen
/// @param begin start of outlining
/// @param end end of outlining (non-inclusive like standard iterators)
/// @return the result of outlining
OutlineResult outlineRegion(BodiedFunc *parent, SeriesFlow *series,
                            decltype(series->begin()) begin,
                            decltype(series->end()) end);

/// Outlines a series flow from its parent function. The outlined code
/// will be replaced by a call to the outlined function, and possibly
/// extra logic if control flow needs to be handled.
/// @param parent the function containing the series flow
/// @param series the series flow on which outlining will happen
/// @return the result of outlining
OutlineResult outlineRegion(BodiedFunc *parent, SeriesFlow *series) {
  return outlineRegion(parent, series, series->begin(), series->end());
}

} // namespace util
} // namespace ir
} // namespace seq
