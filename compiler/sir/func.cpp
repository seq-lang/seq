#include "func.h"

#include <algorithm>
#include <unordered_map>

#include "parser/common.h"

#include "util/iterators.h"
#include "util/lambda_visitor.h"
#include "util/visitor.h"

#include "module.h"
#include "var.h"

namespace {

int findAndReplace(int id, seq::ir::Var *newVal, std::list<seq::ir::Var *> &values) {
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

class VarReplacer : public util::LambdaValueVisitor {
private:
  std::unordered_map<Var *, Var *> replacements;

  Var *findReplacement(Var *var) {
    auto iter = replacements.find(var);
    return (iter != replacements.end()) ? iter->second : nullptr;
  }

public:
  explicit VarReplacer(std::unordered_map<Var *, Var *> replacements)
      : util::LambdaValueVisitor(), replacements(std::move(replacements)) {}

  void handle(VarValue *x) override {
    if (auto *r = findReplacement(x->getVar())) {
      x->setVar(r);
    }
  }

  void handle(AssignInstr *x) override {
    if (auto *r = findReplacement(x->getLhs())) {
      x->setLhs(r);
    }
  }

  void handle(ForFlow *x) override {
    if (auto *r = findReplacement(x->getVar())) {
      x->setVar(r);
    }
  }

  void handle(TryCatchFlow *x) override {
    for (auto &c : *x) {
      if (auto *r = findReplacement(c.getVar())) {
        c.setVar(r);
      }
    }
  }
};

const char Func::NodeId = 0;

void Func::realize(types::Type *newType, const std::vector<std::string> &names) {
  auto *funcType = cast<types::FuncType>(newType);
  assert(funcType);

  setType(funcType);
  args.clear();

  auto i = 0;
  for (auto *t : *funcType) {
    args.push_back(getModule()->Nr<Var>(t, false, names[i]));
    ++i;
  }
}

Var *Func::getArgVar(const std::string &n) {
  auto it = std::find_if(args.begin(), args.end(),
                         [n](auto *other) { return other->getName() == n; });
  return (it != args.end()) ? *it : nullptr;
}

std::vector<Var *> Func::doGetUsedVariables() const {
  std::vector<Var *> ret(args.begin(), args.end());
  return ret;
}

int Func::doReplaceUsedVariable(int id, Var *newVar) {
  return findAndReplace(id, newVar, args);
}

const char BodiedFunc::NodeId = 0;

std::string BodiedFunc::getUnmangledName() const {
  auto split = ast::split(getName(), '.');
  return split.back();
}

Var *BodiedFunc::doClone() const {
  auto *ret = getModule()->N<BodiedFunc>(getSrcInfo(), getName());
  std::unordered_map<Var *, Var *> replacements;
  std::vector<std::string> argNames;
  for (auto *arg : args)
    argNames.push_back(arg->getName());
  for (auto *var : symbols) {
    auto *newVar = var->clone();
    replacements.emplace(var, newVar);
    ret->push_back(newVar);
  }
  ret->setGenerator(isGenerator());
  ret->realize(const_cast<types::FuncType *>(cast<types::FuncType>(getType()->clone())),
               argNames);
  if (body)
    ret->setBody(cast<Flow>(body->clone()));
  ret->setBuiltin(builtin);

  auto argsIter1 = arg_begin();
  auto argsIter2 = ret->arg_begin();
  while (argsIter1 != arg_end() && argsIter2 != ret->arg_end()) {
    replacements.emplace(*argsIter1, *argsIter2);
    ++argsIter1;
    ++argsIter2;
  }
  VarReplacer replacer(replacements); // replace vars with clones
  ret->getBody()->accept(replacer);
  return ret;
}

std::ostream &BodiedFunc::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{} {}({}) -> {} [\n{}\n] {{\n{}\n}}"),
             builtin ? "builtin_def" : "def", referenceString(),
             fmt::join(util::dereference_adaptor(args.begin()),
                       util::dereference_adaptor(args.end()), ", "),
             cast<types::FuncType>(getType())->getReturnType()->referenceString(),
             fmt::join(util::dereference_adaptor(symbols.begin()),
                       util::dereference_adaptor(symbols.end()), "\n"),
             *body);
  return os;
}

int BodiedFunc::doReplaceUsedValue(int id, Value *newValue) {
  if (body && body->getId() == id) {
    auto *flow = cast<Flow>(newValue);
    assert(flow);
    body = flow;
    return 1;
  }
  return 0;
}

std::vector<Var *> BodiedFunc::doGetUsedVariables() const {
  auto ret = Func::doGetUsedVariables();
  ret.insert(ret.end(), symbols.begin(), symbols.end());
  return ret;
}

int BodiedFunc::doReplaceUsedVariable(int id, Var *newVar) {
  return Func::doReplaceUsedVariable(id, newVar) + findAndReplace(id, newVar, symbols);
}

const char ExternalFunc::NodeId = 0;

Var *ExternalFunc::doClone() const {
  auto *ret = getModule()->N<ExternalFunc>(getSrcInfo(), getName());
  std::vector<std::string> argNames;
  for (auto *arg : args)
    argNames.push_back(arg->getName());
  ret->setGenerator(isGenerator());
  ret->realize(const_cast<types::FuncType *>(cast<types::FuncType>(getType()->clone())),
               argNames);
  ret->setUnmangledName(unmangledName);
  return ret;
}

std::ostream &ExternalFunc::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("external_def {} ~ {}({}) -> {}"), getUnmangledName(),
             referenceString(),
             fmt::join(util::dereference_adaptor(args.begin()),
                       util::dereference_adaptor(args.end()), ", "),
             cast<types::FuncType>(getType())->getReturnType()->referenceString());
  return os;
}

const char InternalFunc::NodeId = 0;

std::string InternalFunc::getUnmangledName() const {
  auto names = ast::split(getName(), '.');
  auto name = names.back();
  if (std::isdigit(name[0])) // TODO: get rid of this hack
    name = names[names.size() - 2];
  return name;
}

Var *InternalFunc::doClone() const {
  auto *ret = getModule()->N<InternalFunc>(getSrcInfo(), getName());
  std::vector<std::string> argNames;
  for (auto *arg : args)
    argNames.push_back(arg->getName());
  ret->setGenerator(isGenerator());
  ret->realize(const_cast<types::FuncType *>(cast<types::FuncType>(getType()->clone())),
               argNames);
  ret->setParentType(parentType);
  return ret;
}

std::ostream &InternalFunc::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("internal_def {}.{} ~ {}({}) -> {}"),
             parentType->referenceString(), getUnmangledName(), referenceString(),
             fmt::join(util::dereference_adaptor(args.begin()),
                       util::dereference_adaptor(args.end()), ", "),
             cast<types::FuncType>(getType())->getReturnType()->referenceString());
  return os;
}

std::vector<types::Type *> InternalFunc::doGetUsedTypes() const {
  std::vector<types::Type *> ret;

  for (auto *t : Func::getUsedTypes())
    ret.push_back(const_cast<types::Type *>(t));

  if (parentType)
    ret.push_back(parentType);

  return ret;
}

int InternalFunc::doReplaceUsedType(const std::string &name, types::Type *newType) {
  auto count = Func::replaceUsedType(name, newType);
  if (parentType && parentType->getName() == name) {
    parentType = newType;
    ++count;
  }
  return count;
}

const char LLVMFunc::NodeId = 0;

std::string LLVMFunc::getUnmangledName() const {
  auto names = ast::split(getName(), '.');
  auto name = names.back();
  if (std::isdigit(name[0])) // TODO: get rid of this hack
    name = names[names.size() - 2];
  return name;
}

Var *LLVMFunc::doClone() const {
  auto *ret = getModule()->N<LLVMFunc>(getSrcInfo(), getName());
  std::vector<std::string> argNames;
  for (auto *arg : args)
    argNames.push_back(arg->getName());
  ret->setGenerator(isGenerator());
  ret->realize(const_cast<types::FuncType *>(cast<types::FuncType>(getType()->clone())),
               argNames);
  ret->setLLVMBody(llvmBody);
  ret->setLLVMDeclarations(llvmDeclares);
  ret->setLLVMLiterals(llvmLiterals);
  return ret;
}

std::ostream &LLVMFunc::doFormat(std::ostream &os) const {
  fmt::dynamic_format_arg_store<fmt::format_context> store;
  for (auto &l : llvmLiterals) {
    if (l.isStatic())
      store.push_back(l.getStaticValue());
    else
      store.push_back(
          fmt::format(FMT_STRING("(type_of {})"), l.getTypeValue()->referenceString()));
  }

  auto body = fmt::vformat(llvmDeclares + llvmBody, store);

  fmt::print(os, FMT_STRING("llvm_def {}({}) -> {} {{\n{}\n}}"), getName(),
             fmt::join(util::dereference_adaptor(args.begin()),
                       util::dereference_adaptor(args.end()), ", "),
             cast<types::FuncType>(getType())->getReturnType()->referenceString(),
             body);
  return os;
}

std::vector<types::Type *> LLVMFunc::doGetUsedTypes() const {
  std::vector<types::Type *> ret;

  for (auto *t : Func::getUsedTypes())
    ret.push_back(const_cast<types::Type *>(t));

  for (auto &l : llvmLiterals)
    if (l.isType())
      ret.push_back(const_cast<types::Type *>(l.getTypeValue()));

  return ret;
}

int LLVMFunc::doReplaceUsedType(const std::string &name, types::Type *newType) {
  auto count = Var::doReplaceUsedType(name, newType);
  for (auto &l : llvmLiterals)
    if (l.isType() && l.getTypeValue()->getName() == name) {
      l.setTypeValue(newType);
      ++count;
    }
  return count;
}

} // namespace ir
} // namespace seq
