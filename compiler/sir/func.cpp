#include "func.h"

#include <algorithm>

#include "util/fmt/format.h"
#include "util/visitor.h"

namespace seq {
namespace ir {

Func::Func(std::vector<std::string> argNames, types::FuncType *type)
    : Var(type, true), vars(this), body(std::make_unique<SeriesFlow>("body")) {
  for (int i = 0; i < argNames.size(); ++i) {
    auto newVar = std::make_unique<Var>(argNames[i], type->argTypes[i]);
    args.emplace_back(argNames[i], newVar.get());
    vars.push_back(std::move(newVar));
  }
  this->body->parent = this;
}

Func::Func(std::string name)
    : Var(std::move(name), nullptr, true), vars(this),
      body(std::make_unique<SeriesFlow>("body")) {
  this->body->parent = this;
}

void Func::accept(util::SIRVisitor &v) { v.visit(this); }

void Func::realize(types::FuncType *newType, const std::vector<std::string> &names) {
  type = newType;
  args.clear();

  for (int i = 0; i < names.size(); ++i) {
    auto newVar = std::make_unique<Var>(names[i], newType->argTypes[i]);
    args.emplace_back(names[i], newVar.get());
    vars.push_back(std::move(newVar));
  }
}

Var *Func::getArgVar(const std::string &n) {
  return std::find_if(args.begin(), args.end(),
                      [n](Arg &other) { return other.name == n; })
      ->var;
}

std::ostream &Func::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("def {}(\n"), name);
  for (const auto &argVar : args) {
    fmt::print(os, FMT_STRING("{}\n"), argVar.name);
  }
  fmt::print(os, FMT_STRING(") -> {} [\n"),
             type ? dynamic_cast<types::FuncType *>(type)->rType->referenceString()
                  : "internal");

  for (const auto &var : vars) {
    fmt::print(os, FMT_STRING("{}\n"), *var);
  }

  os << "]{\n";

  if (internal) {
    fmt::print(os, FMT_STRING("internal: {}.{}\n"), parentType->referenceString(),
               unmangledName);
  } else if (external) {
    fmt::print(os, FMT_STRING("external\n"));
  } else if (llvm) {
    fmt::print(os, FMT_STRING("llvm:\n{}"), llvmBody);
  } else {
    fmt::print(os, FMT_STRING("{}\n"), *body);
  }

  os << '}';
  return os;
}

} // namespace ir
} // namespace seq
