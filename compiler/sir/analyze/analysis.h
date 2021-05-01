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

  /// @return a unique key for this pass
  virtual std::string getKey() const = 0;

  /// Execute the analysis.
  /// @param module the module
  virtual std::unique_ptr<Result> run(const Module *module) = 0;
};

} // namespace analyze
} // namespace ir
} // namespace seq
