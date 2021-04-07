#pragma once

#include "manager.h"

#include "sir/module.h"
#include "sir/util/operator.h"

namespace seq {
namespace ir {
namespace transform {

/// General pass base class.
class Pass {
private:
  PassManager *manager;

public:
  virtual ~Pass() = default;

  /// Execute the pass.
  /// @param module the module
  virtual void run(Module *module) = 0;

  /// Sets the manager.
  /// @param mng the new manager
  void setManager(PassManager *mng) { manager = mng; }
  /// Returns the result of a given analysis.
  /// @param key the analysis key
  template <typename AnalysisType>
  const AnalysisType *getAnalysisResult(const std::string &key) {
    return static_cast<const AnalysisType *>(manager->getAnalysisResult(key));
  }
};

/// Pass that runs a single Operator.
class OperatorPass : public Pass, public util::Operator {
public:
  /// Constructs an operator pass.
  /// @param childrenFirst true if children should be iterated first
  OperatorPass(bool childrenFirst = false) : util::Operator(childrenFirst) {}

  void run(Module *module) override { process(module); }
};

} // namespace transform
} // namespace ir
} // namespace seq
