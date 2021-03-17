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

int SeriesFlow::doReplaceUsedValue(int id, Value *newValue) {
  return findAndReplace(id, newValue, series);
}

const char WhileFlow::NodeId = 0;

int WhileFlow::doReplaceUsedValue(int id, Value *newValue) {
  auto replacements = 0;

  if (cond->getId() == id) {
    cond = newValue;
    ++replacements;
  }
  if (body->getId() == id) {
    auto *f = cast<Flow>(newValue);
    seqassert(f, "{} is not a flow", *newValue);
    body = f;
    ++replacements;
  }
  return replacements;
}

const char ForFlow::NodeId = 0;

int ForFlow::doReplaceUsedValue(int id, Value *newValue) {
  auto count = 0;
  if (iter->getId() == id) {
    iter = newValue;
    ++count;
  }
  if (body->getId() == id) {
    auto *f = cast<Flow>(newValue);
    seqassert(f, "{} is not a flow", *newValue);
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
    seqassert(f, "{} is not a flow", *newValue);
    trueBranch = f;
    ++replacements;
  }
  if (falseBranch && falseBranch->getId() == id) {
    auto *f = cast<Flow>(newValue);
    seqassert(f, "{} is not a flow", *newValue);
    falseBranch = f;
    ++replacements;
  }

  return replacements;
}

const char TryCatchFlow::NodeId = 0;

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
    seqassert(f, "{} is not a flow", *newValue);
    body = f;
    ++replacements;
  }
  if (finally && finally->getId() == id) {
    auto *f = cast<Flow>(newValue);
    seqassert(f, "{} is not a flow", *newValue);
    finally = f;
    ++replacements;
  }

  for (auto &c : catches) {
    if (c.getHandler()->getId() == id) {
      auto *f = cast<Flow>(newValue);
      seqassert(f, "{} is not a flow", *newValue);
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
    seqassert(funcType, "{} is not a function type", *func->getType());
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
      seqassert(funcType, "{} is not a function type", *func->getType());
      genType = cast<types::GeneratorType>(funcType->getReturnType());
    }
    seqassert(genType, "generator type not found");
    return genType->getBase();
  } else if (args.empty()) {
    return func->getType();
  } else {
    auto *funcType = cast<types::FuncType>(func->getType());
    seqassert(funcType, "{} is not a function type", *func->getType());
    return funcType->getReturnType();
  }
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
