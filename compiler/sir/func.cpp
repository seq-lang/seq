#include "func.h"

#include <algorithm>

#include "parser/common.h"

#include "util/iterators.h"
#include "util/visitor.h"

#include "var.h"

namespace seq {
namespace ir {

const char Func::NodeId = 0;

Func::Func(types::Type *type, std::vector<std::string> argNames, std::string name)
    : AcceptorExtend(type, std::move(name)) {
  auto *funcType = cast<types::FuncType>(getType());
  assert(funcType);

  auto i = 0;
  for (auto *t : *funcType) {
    args.push_back(std::make_unique<Var>(t, argNames[i]));
    ++i;
  }
}

void Func::realize(types::FuncType *newType, const std::vector<std::string> &names) {
  setType(newType);
  args.clear();

  auto i = 0;
  for (auto *t : *newType) {
    args.push_back(std::make_unique<Var>(t, names[i]));
    ++i;
  }

  if (isA<LLVMFunc>(this) && referenceString() == ".Int.__add__")
    printf("foo");
}

Var *Func::getArgVar(const std::string &n) {
  auto it = std::find_if(args.begin(), args.end(),
                         [n](const VarPtr &other) { return other->getName() == n; });
  return (it != args.end()) ? it->get() : nullptr;
}

bool Func::isGenerator() const {
  auto *funcType = cast<types::FuncType>(getType());
  assert(funcType);
  return isA<types::GeneratorType>(funcType->getReturnType());
}

const char BodiedFunc::NodeId = 0;

std::string BodiedFunc::getUnmangledName() const {
  return builtin ? ast::split(getName(), '.').back() : getName();
}

std::ostream &BodiedFunc::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("{} {}({}) -> {} [\n{}\n] {{\n{}\n}}"),
             builtin ? "builtin_def" : "def",
             referenceString(),
             fmt::join(util::dereference_adaptor(arg_begin()), util::dereference_adaptor(arg_end()), ", "),
             cast<types::FuncType>(getType())->getReturnType()->referenceString(),
             fmt::join(util::dereference_adaptor(begin()), util::dereference_adaptor(end()), "\n"),
             *body);
  return os;
}

const char ExternalFunc::NodeId = 0;

std::ostream &ExternalFunc::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("external_def {} ~ {}({}) -> {}"),
             getUnmangledName(),
             referenceString(),
             fmt::join(util::dereference_adaptor(arg_begin()), util::dereference_adaptor(arg_end()), ", "),
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

std::ostream &InternalFunc::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("internal_def {}.{} ~ {}({}) -> {}"),
             parentType->referenceString(),
             getUnmangledName(),
             referenceString(),
             fmt::join(util::dereference_adaptor(arg_begin()), util::dereference_adaptor(arg_end()), ", "),
             cast<types::FuncType>(getType())->getReturnType()->referenceString());
  return os;
}

const char LLVMFunc::NodeId = 0;

std::string LLVMFunc::getUnmangledName() const {
  auto names = ast::split(getName(), '.');
  auto name = names.back();
  if (std::isdigit(name[0])) // TODO: get rid of this hack
    name = names[names.size() - 2];
  return name;
}

std::ostream &LLVMFunc::doFormat(std::ostream &os) const {
  fmt::dynamic_format_arg_store<fmt::format_context> store;
  for (auto &l : llvmLiterals) {
    if (l.tag == LLVMLiteral::STATIC)
      store.push_back(l.val.staticVal);
    else
      store.push_back(fmt::format(FMT_STRING("(type_of {})"), l.val.type->referenceString()));
  }

  auto body = fmt::vformat(llvmDeclares + llvmBody, store);

  fmt::print(os, FMT_STRING("llvm_def {}({}) -> {} {{\n{}\n}}"),
             getName(),
             fmt::join(util::dereference_adaptor(arg_begin()), util::dereference_adaptor(arg_end()), ", "),
             cast<types::FuncType>(getType())->getReturnType()->referenceString(),
             body);
  return os;
}

} // namespace ir
} // namespace seq
