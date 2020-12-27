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
  /// list of arguments
  std::list<VarPtr> args;

  /// list of variables defined and used within the function
  std::list<VarPtr> symbols;

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

  virtual ~Func() = default;

  /// Re-initializes the function with a new type and names.
  /// @param newType the function's new type
  /// @param names the function's new argument names
  void realize(types::FuncType *newType, const std::vector<std::string> &names);

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
  bool isGenerator() const;

  Var *getArgVar(const std::string &n);

  /// @return the unmangled function name
  virtual std::string getUnmangledName() const = 0;
};

using FuncPtr = std::unique_ptr<Func>;

class BodiedFunc : public AcceptorExtend<BodiedFunc, Func> {
private:
  /// the function body
  FlowPtr body;
  /// whether the function is builtin
  bool builtin;

public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  std::string getUnmangledName() const override;

  /// @return the function body
  const FlowPtr &getBody() const { return body; }
  /// Sets the function's body.
  /// @param b the new body
  void setBody(FlowPtr b) { body = std::move(b); }

  /// @return true if the function is builtin
  bool isBuiltin() const { return builtin; }
  /// Changes the function's builtin status.
  /// @param v true if builtin, false otherwise
  void setBuiltin(bool v = true) {
    builtin = v;
  }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

class ExternalFunc : public AcceptorExtend<ExternalFunc, Func> {
private:
  std::string unmangledName;

public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  std::string getUnmangledName() const override { return unmangledName; }
  /// Sets the unmangled name.
  /// @param v the new value
  void setUnmangledName(std::string v) { unmangledName = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

class InternalFunc : public AcceptorExtend<ExternalFunc, Func> {
private:
  /// parent type of the function if it is magic
  types::Type *parentType = nullptr;

public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  std::string getUnmangledName() const override;

  /// @return the parent type
  types::Type *getParentType() const { return parentType; }
  /// Sets the parent type.
  /// @param p the new parent
  void setParentType(types::Type *p) { parentType = p; }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

class LLVMFunc : public AcceptorExtend<ExternalFunc, Func> {
public:
  struct LLVMLiteral {
    union {
      int64_t staticVal;
      types::Type *type;
    } val;
    enum {STATIC, TYPE} tag;

    explicit LLVMLiteral(int64_t v) : val{v}, tag(STATIC) {}
    explicit LLVMLiteral(types::Type *t) : val{}, tag(TYPE) { val.type = t; }
  };

  using literal_const_iterator = std::vector<LLVMLiteral>::const_iterator;
  using literal_const_reference = std::vector<LLVMLiteral>::const_reference;

private:
  /// literals that must be formatted into the body
  std::vector<LLVMLiteral> llvmLiterals;
  /// declares for llvm-only function
  std::string llvmDeclares;
  /// body of llvm-only function
  std::string llvmBody;

public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  std::string getUnmangledName() const override;

  /// Sets the LLVM literals.
  /// @param v the new values.
  void setLLVMLiterals(std::vector<LLVMLiteral> v) { llvmLiterals = std::move(v); }

  /// @return iterator to the first literal
  literal_const_iterator literal_begin() const { return llvmLiterals.begin(); }
  /// @return iterator beyond the last literal
  literal_const_iterator literal_end() const { return llvmLiterals.end(); }

  /// @return a reference to the first literal
  literal_const_reference literal_front() const { return llvmLiterals.front(); }
  /// @return a reference to the last literal
  literal_const_reference literal_back() const { return llvmLiterals.back(); }

  /// @return the LLVM declarations
  const std::string &getLLVMDeclarations() const { return llvmDeclares; }
  /// Sets the LLVM declarations.
  /// @param v the new value
  void setLLVMDeclarations(std::string v) { llvmDeclares = std::move(v); }
  /// @return the LLVM body
  const std::string &getLLVMBody() const { return llvmBody; }
  /// Sets the LLVM body.
  /// @param v the new value
  void setLLVMBody(std::string v) { llvmBody = std::move(v); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

} // namespace ir
} // namespace seq
