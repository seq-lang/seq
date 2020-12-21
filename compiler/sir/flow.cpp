#include "flow.h"

#include "util/iterators.h"

#include "util/fmt/ostream.h"

namespace seq {
namespace ir {

const char Flow::NodeId = 0;

const char SeriesFlow::NodeId = 0;

std::ostream &SeriesFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: {{\n{}\n}}"), referenceString(),
             fmt::join(util::dereference_adaptor(series.begin()),
                       util::dereference_adaptor(series.end()), "\n"));
  return os;
}

const char WhileFlow::NodeId = 0;

std::ostream &WhileFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: while ({}){{\n{}}}"), referenceString(), *cond, *body);
  return os;
}

const char ForFlow::NodeId = 0;

std::ostream &ForFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: for ({} : {}){{\n{}}}"), referenceString(),
             var->referenceString(), *iter, *body);
  return os;
}

const char IfFlow::NodeId = 0;

std::ostream &IfFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: if ("), referenceString());
  fmt::print(os, FMT_STRING("{}) {{\n{}\n}}"), *cond, *trueBranch);
  if (falseBranch)
    fmt::print(os, FMT_STRING(" else {{\n{}\n}}"), *falseBranch);
  return os;
}

const char TryCatchFlow::NodeId = 0;

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