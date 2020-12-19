#include "flow.h"

#include "util/iterators.h"

#include "util/fmt/ostream.h"

namespace seq {
namespace ir {

std::ostream &SeriesFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: {{\n{}\n}}"), referenceString(),
             fmt::join(util::dereference_adaptor(series.begin()),
                       util::dereference_adaptor(series.end()), "\n"));
  return os;
}

std::ostream &WhileFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: while ({}){{\n{}}}"), referenceString(), *cond, *body);
  return os;
}

std::ostream &ForFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: for ("), referenceString());
  if (setup)
    fmt::print(os, FMT_STRING("{} "), *setup);
  os << "; ";
  fmt::print(os, FMT_STRING("{}; "), *cond);

  if (update)
    fmt::print(os, FMT_STRING("{}"), *update);

  fmt::print(os, FMT_STRING(") {{\n{}\n}}"), *body);
  return os;
}

std::ostream &IfFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: if ("), referenceString());
  fmt::print(os, FMT_STRING("{}) {{\n{}\n}}"), *cond, *trueBranch);
  if (falseBranch)
    fmt::print(os, FMT_STRING(" else {{\n{}\n}}"), *falseBranch);
  return os;
}

std::ostream &TryCatchFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: try {{\n{}\n}}"), referenceString(), *body);
  for (auto &c : catches) {
    fmt::print(os, FMT_STRING("catch ({}{}{}) {{\n{}\n}} "), *c.type,
               c.catchVar ? " -> " : "",
               c.catchVar ? c.catchVar->referenceString() : "", *c.handler);
  }
  if (finally)
    fmt::print(os, FMT_STRING("finally {{\n{}\n}}"), *finally);
  return os;
}

} // namespace ir
} // namespace seq