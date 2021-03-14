#pragma once

#include <memory>

#include "sir/module.h"

namespace seq {
namespace ir {
namespace analyze {

/// Analysis result base class.
class Result {
public:
  virtual ~Result() noexcept = default;
};

/// Base class for IR analyses.
class Analysis {
public:
  virtual ~Analysis() noexcept = default;

  /// Execute the analysis.
  /// @param module the module
  virtual std::unique_ptr<Result> run(const Module *module) = 0;
};

} // namespace analyze
} // namespace ir
} // namespace seq
