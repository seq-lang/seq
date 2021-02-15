#include "flow.h"

#include "util/iterators.h"

#include "util/fmt/ostream.h"

#include "module.h"

namespace {

int findAndReplace(int id, seq::ir::Value *newVal,
                   std::list<seq::ir::Value *> &values) {
  auto replacements = 0;
  for (auto &value : values) {
    if (value->getId() == id) {
      value = newVal;
      ++replacements;
    }
  }
  return replacements;
}

} // namespace

namespace seq {
namespace ir {

const char Flow::NodeId = 0;

const types::Type *Flow::doGetType() const { return getModule()->getVoidType(); }

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

int SeriesFlow::doReplaceUsedValue(int id, Value *newValue) {
  return findAndReplace(id, newValue, series);
}

const char WhileFlow::NodeId = 0;

std::ostream &WhileFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: while ({}){{\n{}}}"), referenceString(), *cond, *body);
  return os;
}

Value *WhileFlow::doClone() const {
  return getModule()->N<WhileFlow>(getSrcInfo(), cond->clone(),
                                   cast<Flow>(body->clone()), getName());
}

int WhileFlow::doReplaceUsedValue(int id, Value *newValue) {
  auto replacements = 0;

  if (cond->getId() == id) {
    cond = newValue;
    ++replacements;
  }
  if (body->getId() == id) {
    auto *f = cast<Flow>(newValue);
    assert(f);
    body = f;
    ++replacements;
  }
  return replacements;
}

const char ForFlow::NodeId = 0;

std::ostream &ForFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: for ({} : {}){{\n{}}}"), referenceString(),
             var->referenceString(), *iter, *body);
  return os;
}

Value *ForFlow::doClone() const {
  return getModule()->N<ForFlow>(getSrcInfo(), iter->clone(), cast<Flow>(body->clone()),
                                 var, getName());
}

int ForFlow::doReplaceUsedValue(int id, Value *newValue) {
  auto count = 0;
  if (iter->getId() == id) {
    iter = newValue;
    ++count;
  }
  if (body->getId() == id) {
    auto *f = cast<Flow>(newValue);
    assert(f);
    body = f;
    ++count;
  }
  return count;
}

int ForFlow::doReplaceUsedVariable(int id, Var *newVar) {
  if (var->getId() == id) {
    var = newVar;
    return 1;
  }
  return 0;
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
  return getModule()->N<IfFlow>(
      getSrcInfo(), cond->clone(), cast<Flow>(trueBranch->clone()),
      falseBranch ? cast<Flow>(falseBranch->clone()) : nullptr, getName());
}

std::vector<Value *> IfFlow::doGetUsedValues() const {
  std::vector<Value *> ret = {cond, trueBranch};
  if (falseBranch)
    ret.push_back(falseBranch);
  return ret;
}

int IfFlow::doReplaceUsedValue(int id, Value *newValue) {
  auto replacements = 0;

  if (cond->getId() == id) {
    cond = newValue;
    ++replacements;
  }
  if (trueBranch->getId() == id) {
    auto *f = cast<Flow>(newValue);
    assert(f);
    trueBranch = f;
    ++replacements;
  }
  if (falseBranch && falseBranch->getId() == id) {
    auto *f = cast<Flow>(newValue);
    assert(f);
    falseBranch = f;
    ++replacements;
  }

  return replacements;
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
      getSrcInfo(), cast<Flow>(body->clone()),
      finally ? cast<Flow>(finally->clone()) : nullptr, getName());
  for (auto &child : *this)
    newFlow->emplace_back(cast<Flow>(child.getHandler()->clone()),
                          const_cast<types::Type *>(child.getType()),
                          const_cast<Var *>(child.getVar()));
  return newFlow;
}

std::vector<Value *> TryCatchFlow::doGetUsedValues() const {
  std::vector<Value *> ret = {body};
  if (finally)
    ret.push_back(finally);

  for (auto &c : catches)
    ret.push_back(const_cast<Value *>(static_cast<const Value *>(c.getHandler())));
  return ret;
}

int TryCatchFlow::doReplaceUsedValue(int id, Value *newValue) {
  auto replacements = 0;

  if (body->getId() == id) {
    auto *f = cast<Flow>(newValue);
    assert(f);
    body = f;
    ++replacements;
  }
  if (finally && finally->getId() == id) {
    auto *f = cast<Flow>(newValue);
    assert(f);
    finally = f;
    ++replacements;
  }

  for (auto &c : catches) {
    if (c.getHandler()->getId() == id) {
      auto *f = cast<Flow>(newValue);
      assert(f);
      c.setHandler(f);
      ++replacements;
    }
  }

  return replacements;
}

std::vector<types::Type *> TryCatchFlow::doGetUsedTypes() const {
  std::vector<types::Type *> ret;
  for (auto &c : catches) {
    if (auto *t = c.getType())
      ret.push_back(const_cast<types::Type *>(t));
  }
  return ret;
}

int TryCatchFlow::doReplaceUsedType(const std::string &name, types::Type *newType) {
  auto count = 0;
  for (auto &c : catches) {
    if (c.getType()->getName() == name) {
      c.setType(newType);
      ++count;
    }
  }
  return count;
}

std::vector<Var *> TryCatchFlow::doGetUsedVariables() const {
  std::vector<Var *> ret;
  for (auto &c : catches) {
    if (auto *t = c.getVar())
      ret.push_back(const_cast<Var *>(t));
  }
  return ret;
}

int TryCatchFlow::doReplaceUsedVariable(int id, Var *newVar) {
  auto count = 0;
  for (auto &c : catches) {
    if (c.getVar()->getId() == id) {
      c.setVar(newVar);
      ++count;
    }
  }
  return count;
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

std::vector<Value *> PipelineFlow::doGetUsedValues() const {
  std::vector<Value *> ret;
  for (auto &s : stages) {
    ret.push_back(const_cast<Value *>(s.getFunc()));
    for (auto *arg : s.args)
      if (arg)
        ret.push_back(arg);
  }
  return ret;
}

int PipelineFlow::doReplaceUsedValue(int id, Value *newValue) {
  auto replacements = 0;

  for (auto &c : stages) {
    if (c.getFunc()->getId() == id) {
      c.func = newValue;
      ++replacements;
    }
    for (auto &s : c.args)
      if (s && s->getId() == id) {
        s = newValue;
        ++replacements;
      }
  }

  return replacements;
}

} // namespace ir
} // namespace seq
