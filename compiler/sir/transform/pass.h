#pragma once

#include "sir/module.h"
#include "sir/util/lambda_visitor.h"

namespace seq {
namespace ir {

namespace analyze {
struct Result;
}

namespace transform {

class PassManager;

/// General pass base class.
class Pass {
private:
  PassManager *manager = nullptr;

public:
  virtual ~Pass() = default;

  /// Execute the pass.
  /// @param module the module
  virtual void run(IRModule *module) = 0;

  /// Sets the manager.
  /// @param mng the new manager
  void setManager(PassManager *mng) { manager = mng; }
  /// Returns the result of a given analysis.
  /// @param key the analysis key
  template <typename AnalysisType>
  const AnalysisType *getAnalysisResult(const std::string &key) {
    return static_cast<const AnalysisType *>(doGetAnalysis(key));
  }

private:
  const analyze::Result *doGetAnalysis(const std::string &key);
};

/// Pass that runs a single LambdaValueVisitor.
class LambdaValuePass : public Pass, public util::LambdaValueVisitor {
public:
  void run(IRModule *module) override { process(module); }
};

} // namespace transform
} // namespace ir
} // namespace seq
