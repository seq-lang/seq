#pragma once

#include <list>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "util/named_list.h"

#include "flow.h"
#include "types/types.h"
#include "var.h"

namespace seq {
namespace ir {

/// SIR function
struct Func : public Var {
public:
  struct Arg {
    std::string name;
    Var *var;

    Arg(std::string name, Var *var) : name(std::move(name)), var(var) {}
  };

  using ArgList = std::list<Arg>;
  using SymbolList = util::NamedList<Var>;
  using BodyPtr = std::unique_ptr<SeriesFlow>;

  /// list of arguments
  ArgList args;

  /// map of variables defined and used within the function
  SymbolList vars;

  /// function body
  BodyPtr body;

  /// true if external, false otherwise
  bool external = false;
  /// true if the function is a generator, false otherwise
  bool generator = false;

  /// true if the function is internal, false otherwise
  bool internal = false;
  /// true if the function is builtin, false otherwise
  bool builtin = false;
  /// true if the function is LLVM based
  bool llvm = false;
  /// declares for llvm-only function
  std::string llvmDeclares;
  /// body of llvm-only function
  std::string llvmBody;
  /// parent type of the function if it is magic
  types::Type *parentType = nullptr;
  /// unmangled name of the function
  std::string unmangledName;

public:
  /// Constructs an SIR function.
  /// @param argNames the function's argument names
  /// @param type the function's type
  Func(std::vector<std::string> argNames, types::FuncType *type);

  /// Constructs an SIR function with no args and void return type.
  /// @param name the function's name
  explicit Func(std::string name);

  void accept(util::SIRVisitor &v) override;

  /// Re-initializes the function with a new type and names.
  /// @param newType the function's new type
  /// @param names the function's new argument names
  void realize(types::FuncType *newType, const std::vector<std::string> &names);

  /// Makes the function internal.
  /// @param p the function's parent type
  /// @param n the function's unmangled name
  void setInternal(types::Type *p, std::string n) {
    internal = true;
    parentType = p;
    unmangledName = std::move(n);
  }

  /// Makes the function builtin.
  /// @param n the function's unmangled name
  void setBuiltin(std::string n) {
    builtin = true;
    unmangledName = std::move(n);
  }

  Var *getArgVar(const std::string &n);

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

using FuncPtr = std::unique_ptr<Func>;

} // namespace ir
} // namespace seq
