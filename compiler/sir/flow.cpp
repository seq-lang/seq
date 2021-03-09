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

types::Type *Flow::doGetType() const { return getModule()->getVoidType(); }

const char SeriesFlow::NodeId = 0;

std::ostream &SeriesFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: [\n{}\n]"), referenceString(),
             fmt::join(util::dereference_adaptor(series.begin()),
                       util::dereference_adaptor(series.end()), "\n"));
  return os;
}

int SeriesFlow::doReplaceUsedValue(int id, Value *newValue) {
  return findAndReplace(id, newValue, series);
}

const char WhileFlow::NodeId = 0;

std::ostream &WhileFlow::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{}: while ({}){{\n{}}}"), referenceString(), *cond, *body);
  return os;
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

types::Type *PipelineFlow::Stage::getOutputType() const {
  if (args.empty()) {
    return func->getType();
  } else {
    auto *funcType = cast<types::FuncType>(func->getType());
    assert(funcType);
    return funcType->getReturnType();
  }
}

types::Type *PipelineFlow::Stage::getOutputElementType() const {
  if (isGenerator()) {
    types::GeneratorType *genType = nullptr;
    if (args.empty()) {
      genType = cast<types::GeneratorType>(func->getType());
      return genType->getBase();
    } else {
      auto *funcType = cast<types::FuncType>(func->getType());
      assert(funcType);
      genType = cast<types::GeneratorType>(funcType->getReturnType());
    }
    assert(genType);
    return genType->getBase();
  } else if (args.empty()) {
    return func->getType();
  } else {
    auto *funcType = cast<types::FuncType>(func->getType());
    assert(funcType);
    return funcType->getReturnType();
  }
}

std::ostream &PipelineFlow::doFormat(std::ostream &os) const {
  os << "pipeline(";
  for (const auto &stage : *this) {
    os << *stage.getFunc() << "[" << (stage.isGenerator() ? "g" : "f") << "](";
    std::string sep = "";
    for (const auto *arg : stage) {
      os << sep;
      if (arg) {
        os << *arg;
      } else {
        os << "...";
      }
      sep = ", ";
    }
    os << ")";
    if (&stage != &back())
      os << (stage.isParallel() ? " ||> " : " |> ");
  }
  os << ")";
  return os;
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
