#pragma once

#include <memory>

#include "sir/module.h"
#include "sir/transform/pass.h"

namespace seq {
namespace ir {
namespace analyze {

/// Analysis result base struct.
struct Result {
  virtual ~Result() noexcept = default;
};

/// Base class for IR analyses.
class Analysis {
private:
  transform::PassManager *manager = nullptr;

public:
  virtual ~Analysis() noexcept = default;

  /// Execute the analysis.
  /// @param module the module
  virtual std::unique_ptr<Result> run(const IRModule *module) = 0;

  /// Sets the manager.
  /// @param mng the new manager
  void setManager(transform::PassManager *mng) { manager = mng; }
  /// Returns the result of a given analysis.
  /// @param key the analysis key
  template <typename AnalysisType>
  const AnalysisType *getAnalysisResult(const std::string &key) {
    return static_cast<const AnalysisType *>(doGetAnalysis(key));
  }

private:
  const analyze::Result *doGetAnalysis(const std::string &key);
};

} // namespace analyze
} // namespace ir
} // namespace seq
