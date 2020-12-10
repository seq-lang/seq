#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "util/named_list.h"

#include "base.h"
#include "func.h"
#include "var.h"

namespace seq {
namespace ir {

/// SIR object representing a program.
struct SIRModule : public AttributeHolder {
  using SymbolList = util::NamedList<Var>;
  using TypesList = util::NamedList<types::Type>;

  /// the global variable table
  SymbolList globals;
  /// the global types table
  TypesList types;

  /// the module's "main" function
  FuncPtr mainFunc;
  /// the module's argv variable
  VarPtr argVar;

  /// Constructs an SIR module.
  explicit SIRModule(std::string name);

  void accept(util::SIRVisitor &v);

  std::string referenceString() const override { return name; }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

using SIRModulePtr = std::unique_ptr<SIRModule>;

} // namespace ir
} // namespace seq
