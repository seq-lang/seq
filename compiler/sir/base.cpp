#include "base.h"

#include "util/fmt/ostream.h"

namespace seq {
namespace ir {

const std::string kSrcInfoAttribute = "srcInfoAttribute";
const std::string kFuncAttribute = "funcAttribute";

bool FuncAttribute::has(const std::string &val) const {
  return attributes.find(val) != attributes.end();
}

std::ostream &FuncAttribute::doFormat(std::ostream &os) const {
  std::vector<std::string> keys;
  for (auto &val : attributes)
    keys.push_back(val.second);
  fmt::print(os, FMT_STRING("{}"), fmt::join(keys.begin(), keys.end(), ","));
  return os;
}

} // namespace ir
} // namespace seq
