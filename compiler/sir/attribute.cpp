#include "value.h"

#include "util/fmt/ostream.h"

namespace seq {
namespace ir {

const std::string FuncAttribute::AttributeName = "funcAttribute";

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

const std::string MemberAttribute::AttributeName = "memberAttribute";

std::ostream &MemberAttribute::doFormat(std::ostream &os) const {
  std::vector<std::string> strings;
  for (auto &val : memberSrcInfo)
    strings.push_back(fmt::format(FMT_STRING("{}={}"), val.first, val.second));
  fmt::print(os, FMT_STRING("({})"), fmt::join(strings.begin(), strings.end(), ","));
  return os;
}

const std::string SrcInfoAttribute::AttributeName = "srcInfoAttribute";

} // namespace ir
} // namespace seq
