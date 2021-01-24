#include "flow.h"

#include "util/iterators.h"

#include "util/fmt/ostream.h"

#include "module.h"

namespace seq {
namespace ir {

const char Flow::NodeId = 0;

const types::Type *Flow::getType() const { return getModule()->getVoidType(); }

const char SeriesFlow::NodeId = 0;

std::ostream &SeriesFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: [\n{}\n]"), referenceString(),
             fmt::join(util::dereference_adaptor(series.begin()),
                       util::dereference_adaptor(series.end()), "\n"));
  return os;
}

Value *SeriesFlow::doClone() const {
  auto *newFlow = getModule()->N<SeriesFlow>(getSrcInfo(), getName());
  for (auto *child : *this)
    newFlow->push_back(child->clone());
  return newFlow;
}

const char WhileFlow::NodeId = 0;

std::ostream &WhileFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: while ({}){{\n{}}}"), referenceString(), *cond, *body);
  return os;
}

Value *WhileFlow::doClone() const {
  return getModule()->N<WhileFlow>(getSrcInfo(), cond->clone(), body->clone(),
                                   getName());
}

const char ForFlow::NodeId = 0;

std::ostream &ForFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: for ({} : {}){{\n{}}}"), referenceString(),
             var->referenceString(), *iter, *body);
  return os;
}

Value *ForFlow::doClone() const {
  return getModule()->N<ForFlow>(getSrcInfo(), iter->clone(), body->clone(), var,
                                 getName());
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
  return getModule()->N<IfFlow>(getSrcInfo(), cond->clone(), trueBranch->clone(),
                                falseBranch ? falseBranch->clone() : nullptr,
                                getName());
}

const char TryCatchFlow::NodeId = 0;

std::ostream &TryCatchFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: try {{\n{}\n}}"), referenceString(), *body);
  for (auto &c : catches) {
    fmt::print(os, FMT_STRING("catch ({}{}{}) {{\n{}\n}} "),
               c.getType() ? c.getType()->referenceString() : "all",
               c.getVar() ? " -> " : "",
               c.getVar() ? c.getVar()->referenceString() : "", *c.getHandler());
  }
  if (finally)
    fmt::print(os, FMT_STRING("finally {{\n{}\n}}"), *finally);
  return os;
}

Value *TryCatchFlow::doClone() const {
  auto *newFlow = getModule()->N<TryCatchFlow>(
      getSrcInfo(), body->clone(), finally ? finally->clone() : nullptr, getName());
  for (auto &child : *this)
    newFlow->emplace_back(child.getHandler()->clone(), child.getType(),
                          const_cast<Var *>(child.getVar()));
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
  auto *newFlow = getModule()->N<UnorderedFlow>(getSrcInfo(), getName());
  for (auto *child : *this)
    newFlow->push_back(child->clone());
  return newFlow;
}

const char PipelineFlow::NodeId = 0;

const types::Type *PipelineFlow::Stage::getOutputType() const {
  if (args.empty()) {
    return func->getType();
  } else {
    auto *funcType = func->getType()->as<types::FuncType>();
    assert(funcType);
    return funcType->getReturnType();
  }
}

std::ostream &PipelineFlow::doFormat(std::ostream &os) const {
  os << "pipeline(";
  for (const auto &stage : *this) {
    os << *stage.getFunc() << "[" << (stage.isGenerator() ? "g" : "f") << "](";
    for (const auto *arg : stage) {
      if (arg) {
        os << *arg;
      } else {
        os << "...";
      }
    }
    os << ")";
    if (&stage != &back())
      os << (stage.isParallel() ? " ||> " : " |> ");
  }
  os << ")";
  return os;
}

PipelineFlow::Stage PipelineFlow::Stage::clone() const {
  std::vector<Value *> clonedArgs;
  for (const auto *arg : *this)
    clonedArgs.push_back(arg->clone());
  return {func->clone(), std::move(clonedArgs), generator, parallel};
}

Value *PipelineFlow::doClone() const {
  std::vector<Stage> clonedStages;
  for (const auto &stage : *this)
    clonedStages.emplace_back(stage.clone());
  return getModule()->N<PipelineFlow>(getSrcInfo(), std::move(clonedStages), getName());
}

} // namespace ir
} // namespace seq
