#include "value.h"

#include "util/fmt/ostream.h"

namespace seq {
namespace ir {

const std::string kFuncAttribute = "funcAttribute";
const std::string kMemberAttribute = "memberAttribute";
const std::string kSrcInfoAttribute = "srcInfoAttribute";

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

std::ostream &MemberAttribute::doFormat(std::ostream &os) const {
  std::vector<std::string> strings;
  for (auto &val : memberSrcInfo)
    strings.push_back(fmt::format(FMT_STRING("{}={}"), val.first, val.second));
  fmt::print(os, FMT_STRING("({})"), fmt::join(strings.begin(), strings.end(), ","));
  return os;
}

} // namespace ir
} // namespace seq
