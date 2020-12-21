#include "func.h"

#include <algorithm>

#include "util/iterators.h"
#include "util/visitor.h"
#include "var.h"

namespace seq {
namespace ir {

const char Func::NodeId = 0;

Func::Func(types::Type *type, std::vector<std::string> argNames, std::string name)
    : AcceptorExtend(type, std::move(name)), type(type) {
  auto *funcType = type->as<types::FuncType>();
  assert(funcType);

  auto i = 0;
  for (auto *t : *funcType) {
    auto *newVar = new Var(t, argNames[i]);
    args.emplace_back(argNames[i], newVar);
    symbols.push_back(VarPtr(newVar));
    ++i;
  }
}

void Func::realize(types::FuncType *newType, const std::vector<std::string> &names) {
  type = newType;
  args.clear();

  auto i = 0;
  for (auto *t : *newType) {
    auto *newVar = new Var(t, names[i]);
    args.emplace_back(names[i], newVar);
    symbols.push_back(VarPtr(newVar));
    ++i;
  }
}

Var *Func::getArgVar(const std::string &n) {
  return std::find_if(args.begin(), args.end(),
                      [n](Arg &other) { return other.name == n; })
      ->var;
}

std::ostream &Func::doFormat(std::ostream &os) const {
  std::vector<std::string> argNames;
  for (auto &arg : args)
    argNames.push_back(arg.name);

  fmt::print(os, FMT_STRING("def {}({}) -> {} [\n{}\n] {{\n"), referenceString(),
             fmt::join(argNames, ", "),
             type->as<types::FuncType>()->getReturnType()->referenceString(),
             fmt::join(util::dereference_adaptor(symbols.begin()),
                       util::dereference_adaptor(symbols.end()), "\n"));

  if (internal) {
    fmt::print(os, FMT_STRING("internal: {}.{}\n"), parentType->referenceString(),
               unmangledName);
  } else if (external) {
    fmt::print(os, FMT_STRING("external\n"));
  } else if (llvm) {
    fmt::print(os, FMT_STRING("llvm:\n{}\n"), llvmBody);
  } else {
    fmt::print(os, FMT_STRING("{}\n"), *body);
  }

  os << '}';
  return os;
}

} // namespace ir
} // namespace seq
