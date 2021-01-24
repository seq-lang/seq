#include "func.h"

#include <algorithm>

#include "parser/common.h"

#include "util/iterators.h"
#include "util/visitor.h"

#include "module.h"
#include "var.h"

namespace seq {
namespace ir {

const char Func::NodeId = 0;

void Func::realize(types::FuncType *newType, const std::vector<std::string> &names) {
  setType(newType);
  args.clear();

  auto i = 0;
  for (auto *t : *newType) {
    args.push_back(getModule()->Nr<Var>(t, false, names[i]));
    ++i;
  }
}

Var *Func::getArgVar(const std::string &n) {
  auto it = std::find_if(args.begin(), args.end(),
                         [n](auto *other) { return other->getName() == n; });
  return (it != args.end()) ? *it : nullptr;
}

const char BodiedFunc::NodeId = 0;

std::string BodiedFunc::getUnmangledName() const {
  auto split = ast::split(getName(), '.');
  return builtin ? split.back() : split[split.size() - 1];
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

const char ExternalFunc::NodeId = 0;

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

std::ostream &InternalFunc::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("internal_def {}.{} ~ {}({}) -> {}"),
             parentType->referenceString(), getUnmangledName(), referenceString(),
             fmt::join(util::dereference_adaptor(args.begin()),
                       util::dereference_adaptor(args.end()), ", "),
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
      store.push_back(
          fmt::format(FMT_STRING("(type_of {})"), l.val.type->referenceString()));
  }

  auto body = fmt::vformat(llvmDeclares + llvmBody, store);

  fmt::print(os, FMT_STRING("llvm_def {}({}) -> {} {{\n{}\n}}"), getName(),
             fmt::join(util::dereference_adaptor(args.begin()),
                       util::dereference_adaptor(args.end()), ", "),
             cast<types::FuncType>(getType())->getReturnType()->referenceString(),
             body);
  return os;
}

} // namespace ir
} // namespace seq
