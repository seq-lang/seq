#pragma once

#include "flow.h"
#include "var.h"

namespace seq {
namespace ir {

/// SIR function
class Func : public AcceptorExtend<Func, Var> {
public:
  using arg_const_iterator = std::list<VarPtr>::const_iterator;
  using arg_const_reference = std::list<VarPtr>::const_reference;

  using symbol_iterator = std::list<VarPtr>::iterator;
  using symbol_const_iterator = std::list<VarPtr>::const_iterator;
  using symbol_reference = std::list<VarPtr>::reference;
  using symbol_const_reference = std::list<VarPtr>::const_reference;

private:
  /// the function type
  types::Type *type;

  /// list of arguments
  std::list<VarPtr> args;

  /// list of variables defined and used within the function
  std::list<VarPtr> symbols;

  /// function body
  ValuePtr body;

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
  static const char NodeId;

  /// Constructs an SIR function.
  /// @param type the function's type
  /// @param argNames the function's argument names
  /// @param name the function's name
  Func(types::Type *type, std::vector<std::string> argNames, std::string name = "");

  /// Constructs an SIR function.
  /// @param type the function's type
  /// @param name the function's name
  explicit Func(types::Type *type, std::string name = "")
      : Func(type, {}, std::move(name)) {}

  /// Re-initializes the function with a new type and names.
  /// @param newType the function's new type
  /// @param names the function's new argument names
  void realize(types::FuncType *newType, const std::vector<std::string> &names);

  /// @return the function body
  const ValuePtr &getBody() const { return body; }
  /// Sets the function's body.
  /// @param b the new body
  void setBody(ValuePtr b) { body = std::move(b); }

  /// @return iterator to the first arg
  arg_const_iterator arg_begin() const { return args.begin(); }
  /// @return iterator beyond the last arg
  arg_const_iterator arg_end() const { return args.end(); }

  /// @return a reference to the first arg
  arg_const_reference arg_front() const { return args.front(); }
  /// @return a reference to the last arg
  arg_const_reference arg_back() const { return args.back(); }

  /// @return iterator to the first symbol
  symbol_iterator begin() { return symbols.begin(); }
  /// @return iterator beyond the last symbol
  symbol_iterator end() { return symbols.end(); }
  /// @return iterator to the first symbol
  symbol_const_iterator begin() const { return symbols.begin(); }
  /// @return iterator beyond the last symbol
  symbol_const_iterator end() const { return symbols.end(); }

  /// @return a reference to the first symbol
  symbol_reference front() { return symbols.front(); }
  /// @return a reference to the last symbol
  symbol_reference back() { return symbols.back(); }
  /// @return a reference to the first symbol
  symbol_const_reference front() const { return symbols.front(); }
  /// @return a reference to the last symbol
  symbol_const_reference back() const { return symbols.back(); }

  /// Inserts an symbol at the given position.
  /// @param pos the position
  /// @param v the symbol
  /// @return an iterator to the newly added symbol
  symbol_iterator insert(symbol_iterator pos, VarPtr v) {
    return symbols.insert(pos, std::move(v));
  }
  /// Inserts an symbol at the given position.
  /// @param pos the position
  /// @param v the symbol
  /// @return an symbol_iterator to the newly added symbol
  symbol_iterator insert(symbol_const_iterator pos, VarPtr v) {
    return symbols.insert(pos, std::move(v));
  }
  /// Appends an symbol.
  /// @param v the new symbol
  void push_back(VarPtr v) { symbols.push_back(std::move(v)); }

  /// Erases the symbol at the given position.
  /// @param pos the position
  /// @return symbol_iterator following the removed symbol.
  symbol_iterator erase(symbol_iterator pos) { return symbols.erase(pos); }
  /// Erases the symbol at the given position.
  /// @param pos the position
  /// @return symbol_iterator following the removed symbol.
  symbol_iterator erase(symbol_const_iterator pos) { return symbols.erase(pos); }

  /// @return true if the function is a generator
  bool isGenerator() const { return generator; }
  /// Sets whether the function is a generator.
  /// @param g true or false
  void setIsGenerator(bool g) { generator = g; }

  /// @return true if the function is internal
  bool isInternal() const { return internal; }
  /// @return the unmangled name
  const std::string &getUnmangledName() const { return unmangledName; }
  /// @return the parent type
  types::Type *getParentType() const { return parentType; }
  /// Makes the function internal.
  /// @param p the function's parent type
  /// @param n the function's unmangled name
  void setInternal(types::Type *p, std::string n) {
    internal = true;
    parentType = p;
    unmangledName = std::move(n);
  }

  /// @return true if the function is external
  bool isExternal() const { return external; }
  /// Makes the function external.
  /// @param n the function's unmangled name
  void setExternal(std::string n) {
    external = true;
    unmangledName = std::move(n);
  }

  /// @return true if the function is builtin
  bool isBuiltin() const { return builtin; }
  /// Makes the function builtin.
  /// @param n the function's unmangled name
  void setBuiltin(std::string n) {
    builtin = true;
    unmangledName = std::move(n);
  }

  /// @return true if the function is LLVM-implemented
  bool isLLVM() const { return llvm; }
  /// @return the LLVM declarations
  const std::string &getLLVMDeclarations() const { return llvmDeclares; }
  /// @return the LLVM body
  const std::string &getLLVMBody() const { return llvmBody; }
  /// Makes the function LLVM implemented.
  /// @param decl LLVM declarations
  /// @param b LLVM body
  void setLLVM(std::string decl = "", std::string b = "") {
    llvm = true;
    llvmDeclares = std::move(decl);
    llvmBody = std::move(b);
  }

  Var *getArgVar(const std::string &n);

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

using FuncPtr = std::unique_ptr<Func>;

} // namespace ir
} // namespace seq
