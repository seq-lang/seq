#pragma once

#include "manager.h"

#include "sir/module.h"
#include "sir/util/lambda_visitor.h"

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
  virtual void run(IRModule *module) = 0;

  /// Sets the manager.
  /// @param mng the new manager
  void setManager(PassManager *mng) { manager = mng; }
  template <typename AnalysisType>
  const AnalysisType *getAnalysisResult(const std::string &key) {
    return static_cast<const AnalysisType *>(manager->getAnalysisResult(key));
  }
};

/// Pass that runs a single LambdaValueVisitor.
class LambdaValuePass : public Pass, public util::LambdaValueVisitor {
public:
  void run(IRModule *module) override { process(module); }
};

} // namespace transform
} // namespace ir
} // namespace seq
