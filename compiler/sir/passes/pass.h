#pragma once

#include "sir/sir.h"

#include "sir/util/lambda_visitor.h"

namespace seq {
namespace ir {
namespace passes {

/// General pass base class.
class Pass {
  /// Execute the pass.
  /// @param module the module
  virtual void run(IRModule *module) = 0;
};

/// Pass that runs a single LambdaValueVisitor.
class LambdaValuePass : public Pass, public util::LambdaValueVisitor {
  void run(IRModule *module) override { process(module); }
};

/// Analysis pass base class.
class Analysis : public Pass {
public:
  void run(IRModule *module) override { runAnalysis(module); }

  /// Execute the analysis.
  /// @param module the module
  virtual void runAnalysis(const IRModule *module) = 0;
};

} // namespace passes
} // namespace ir
} // namespace seq
