#pragma once

#include "sir/sir.h"
#include "sir/transform/manager.h"
#include "sir/transform/pass.h"
#include <functional>
#include <string>
#include <vector>

namespace seq {

class DSL {
public:
  template <typename Callback> struct Keyword {
    std::string name;
    Callback callback;
  };

  using ExprKeywordCallback =
      std::function<ir::Node *(ir::Module *M, const std::vector<ir::Value *> &values)>;
  using BlockKeywordCallback = std::function<ir::Node *(
      ir::Module *M, const std::vector<ir::Value *> &values, ir::SeriesFlow *block)>;
  using BinaryKeywordCallback =
      std::function<ir::Node *(ir::Module *M, ir::Value *lhs, ir::Value *rhs)>;

  using ExprKeyword = Keyword<ExprKeywordCallback>;
  using BlockKeyword = Keyword<BlockKeywordCallback>;
  using BinaryKeyword = Keyword<BinaryKeywordCallback>;

  virtual ~DSL() = default;
  virtual std::string getName() = 0;
  virtual bool isVersionSupported(unsigned major, unsigned minor, unsigned patch) {
    return true;
  }
  virtual void addIRPasses(ir::transform::PassManager *) {}
  virtual std::vector<ExprKeyword> getExprKeywords() { return {}; }
  virtual std::vector<BlockKeyword> getBlockKeywords() { return {}; }
  virtual std::vector<BinaryKeyword> getBinaryKeywords() { return {}; }
};

} // namespace seq
