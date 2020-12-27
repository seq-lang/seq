#include "flow.h"

#include <algorithm>

#include "util/iterators.h"

#include "util/fmt/ostream.h"

#include "module.h"

namespace seq {
namespace ir {

const char Flow::NodeId = 0;

const char SeriesFlow::NodeId = 0;

bool SeriesFlow::containsFlows() const {
  return std::any_of(begin(), end(), [](const ValuePtr &child) {
      return child->is<Flow>();
    });
}

std::ostream &SeriesFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: [\n{}\n]"), referenceString(),
             fmt::join(util::dereference_adaptor(series.begin()),
                       util::dereference_adaptor(series.end()), "\n"));
  return os;
}

Value *SeriesFlow::doClone() const {
  auto *newFlow = getModule()->Nrs<SeriesFlow>(getSrcInfo(), getName());
  for (auto &child : *this)
    newFlow->push_back(child->clone());
  return newFlow;
}

const char WhileFlow::NodeId = 0;

std::ostream &WhileFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: while ({}){{\n{}}}"), referenceString(), *cond, *body);
  return os;
}

Value *WhileFlow::doClone() const {
  return getModule()->Nrs<WhileFlow>(getSrcInfo(), cond->clone(), body->clone(), getName());
}

const char ForFlow::NodeId = 0;

std::ostream &ForFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: for ({} : {}){{\n{}}}"), referenceString(),
             var->referenceString(), *iter, *body);
  return os;
}

Value *ForFlow::doClone() const {
  return getModule()->Nrs<ForFlow>(getSrcInfo(), iter->clone(), body->clone(), var, getName());
}

const char IfFlow::NodeId = 0;

std::ostream &IfFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: if ("), referenceString());
  fmt::print(os, FMT_STRING("{}) {{\n{}\n}}"), *cond, *trueBranch);
  if (falseBranch)
    fmt::print(os, FMT_STRING(" else {{\n{}\n}}"), *falseBranch);
  return os;
}

Value *IfFlow::doClone() const {
  return getModule()->Nrs<IfFlow>(getSrcInfo(), cond->clone(),
                                  trueBranch->clone(),
                                  falseBranch ? falseBranch->clone() : nullptr, getName());
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

Value *TryCatchFlow::doClone() const {
  auto *newFlow = getModule()->Nrs<TryCatchFlow>(getSrcInfo(), body->clone(), finally ? finally->clone() : nullptr, getName());
  for (auto &child : *this)
    newFlow->emplace_back(child.handler->clone(), child.type, child.catchVar);
  return newFlow;
}

const char UnorderedFlow::NodeId = 0;

std::ostream &UnorderedFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: {{\n{}\n}}"), referenceString(),
             fmt::join(util::dereference_adaptor(series.begin()),
                       util::dereference_adaptor(series.end()), "\n"));
  return os;
}

Value *UnorderedFlow::doClone() const {
  auto *newFlow = getModule()->Nrs<UnorderedFlow>(getSrcInfo(), getName());
  for (auto &child : *this)
    newFlow->push_back(child->clone());
  return newFlow;
}

} // namespace ir
} // namespace seq