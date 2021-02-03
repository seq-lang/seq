#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "pass.h"

#include "sir/module.h"

namespace seq {
namespace ir {
namespace passes {

/// Utility class to run a series of passes.
class PassManager {
private:
  /// Container for pass metadata.
  struct PassMetadata {
    /// pointer to the pass instance
    std::unique_ptr<Pass> pass;
    /// vector of required passes
    std::vector<std::string> reqs;
    /// vector of invalidated passes
    std::vector<std::string> invalidates;
    /// true if this is an analysis pass
    bool isAnalysis = false;

    PassMetadata() = default;

    PassMetadata(std::unique_ptr<Pass> pass, std::vector<std::string> reqs,
                 std::vector<std::string> invalidates, bool isAnalysis = false)
        : pass(std::move(pass)), reqs(std::move(reqs)),
          invalidates(std::move(invalidates)), isAnalysis(isAnalysis) {}

    PassMetadata(PassMetadata &&) = default;
    PassMetadata &operator=(PassMetadata &&) = default;
  };

  /// map of keys to passes
  std::unordered_map<std::string, PassMetadata> passes;
  /// execution order of non-analysis passes
  std::vector<std::string> executionOrder;

  /// set of valid passes
  std::unordered_set<std::string> valid;

public:
  /// Registers a pass. If this is a non-analysis pass, append it to the execution
  /// order.
  /// @param pass the pass
  /// @param key the pass's key
  /// @param reqs keys of passes that must be run before the current one
  /// @param invalidates keys of passes that are invalidated by the current one
  void registerPass(std::unique_ptr<Pass> pass, const std::string &key,
                    std::vector<std::string> reqs = {},
                    std::vector<std::string> invalidates = {});

  /// Run all passes.
  /// @param module the module
  void run(IRModule *module);

private:
  void runPass(IRModule *module, const std::string &name);
};

} // namespace passes
} // namespace ir
} // namespace seq
