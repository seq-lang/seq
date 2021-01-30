#pragma once

#include "flow.h"
#include "util/iterators.h"
#include "var.h"

namespace seq {
namespace ir {

/// SIR function
class Func : public AcceptorExtend<Func, Var> {
private:
  /// whether the function is a generator
  bool generator;

protected:
  /// list of arguments
  std::list<Var *> args;
  /// list of variables defined and used within the function
  std::list<Var *> symbols;

public:
  static const char NodeId;

  /// Constructs an SIR function.
  /// @param type the function's type
  /// @param name the function's name
  explicit Func(types::Type *type, std::string name = "")
      : AcceptorExtend(type, false, std::move(name)), generator(false) {}

  virtual ~Func() = default;

  /// Re-initializes the function with a new type and names.
  /// @param newType the function's new type
  /// @param names the function's new argument names
  void realize(types::FuncType *newType, const std::vector<std::string> &names);

  /// @return iterator to the first arg
  auto arg_begin() { return args.begin(); }
  /// @return iterator beyond the last arg
  auto arg_end() { return args.end(); }
  /// @return iterator to the first arg
  auto arg_begin() const { return args.begin(); }
  /// @return iterator beyond the last arg
  auto arg_end() const { return args.end(); }

  /// @return a pointer to the last arg
  Var *arg_front() { return args.front(); }
  /// @return a pointer to the last arg
  Var *arg_back() { return args.back(); }
  /// @return a pointer to the last arg
  const Var *arg_back() const { return args.back(); }
  /// @return a pointer to the first arg
  const Var *arg_front() const { return args.front(); }

  /// @return iterator to the first symbol
  auto begin() { return symbols.begin(); }
  /// @return iterator beyond the last symbol
  auto end() { return symbols.end(); }
  /// @return iterator to the first symbol
  auto begin() const { return symbols.begin(); }
  /// @return iterator beyond the last symbol
  auto end() const { return symbols.end(); }

  /// @return a pointer to the first symbol
  Var *front() { return symbols.front(); }
  /// @return a pointer to the last symbol
  Var *back() { return symbols.back(); }
  /// @return a pointer to the first symbol
  const Var *front() const { return symbols.front(); }
  /// @return a pointer to the last symbol
  const Var *back() const { return symbols.back(); }

  /// Inserts an symbol at the given position.
  /// @param pos the position
  /// @param v the symbol
  /// @return an iterator to the newly added symbol
  template <typename It> auto insert(It pos, Var *v) {
    return symbols.insert(pos.internal, v);
  }
  /// Appends an symbol.
  /// @param v the new symbol
  void push_back(Var *v) { symbols.push_back(v); }

  /// Erases the symbol at the given position.
  /// @param pos the position
  /// @return symbol_iterator following the removed symbol.
  template <typename It> auto erase(It pos) { return symbols.erase(pos.internal); }

  /// @return true if the function is a generator
  bool isGenerator() const { return generator; }
  /// Sets the function's generator flag.
  /// @param v the new value
  void setGenerator(bool v = true) { generator = v; }

  Var *getArgVar(const std::string &n);

  /// @return the unmangled function name
  virtual std::string getUnmangledName() const = 0;

  Func *clone() const { return cast<Func>(Var::clone()); }
};

class BodiedFunc : public AcceptorExtend<BodiedFunc, Func> {
private:
  /// the function body
  Flow *body;
  /// whether the function is builtin
  bool builtin = false;

public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  std::string getUnmangledName() const override;

  /// @return the function body
  Flow *getBody() { return body; }
  /// @return the function body
  const Flow *getBody() const { return body; }
  /// Sets the function's body.
  /// @param b the new body
  void setBody(Flow *b) { body = b; }

  /// @return true if the function is builtin
  bool isBuiltin() const { return builtin; }
  /// Changes the function's builtin status.
  /// @param v true if builtin, false otherwise
  void setBuiltin(bool v = true) { builtin = v; }

private:
  Var *doClone() const override;

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
  Var *doClone() const override;

  std::ostream &doFormat(std::ostream &os) const override;
};

class InternalFunc : public AcceptorExtend<InternalFunc, Func> {
private:
  /// parent type of the function if it is magic
  types::Type *parentType = nullptr;

public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  std::string getUnmangledName() const override;

  /// @return the parent type
  types::Type *getParentType() { return parentType; }
  /// @return the parent type
  const types::Type *getParentType() const { return parentType; }
  /// Sets the parent type.
  /// @param p the new parent
  void setParentType(types::Type *p) { parentType = p; }

private:
  Var *doClone() const override;

  std::ostream &doFormat(std::ostream &os) const override;
};

class LLVMFunc : public AcceptorExtend<LLVMFunc, Func> {
public:
  class LLVMLiteral {
  private:
    union {
      int64_t staticVal;
      types::Type *type;
    } val;
    enum { STATIC, TYPE } tag;

  public:
    explicit LLVMLiteral(int64_t v) : val{v}, tag(STATIC) {}
    explicit LLVMLiteral(types::Type *t) : val{}, tag(TYPE) { val.type = t; }

    bool isType() const { return tag == TYPE; }
    bool isStatic() const { return tag == STATIC; }

    types::Type *getType() { return val.type; }
    const types::Type *getType() const { return val.type; }
    void setType(types::Type *t) {
      val.type = t;
      tag = TYPE;
    }

    int64_t getStaticValue() const { return val.staticVal; }
    void setStaticValue(int64_t v) {
      val.staticVal = v;
      tag = STATIC;
    }
  };

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
  auto literal_begin() { return llvmLiterals.begin(); }
  /// @return iterator beyond the last literal
  auto literal_end() { return llvmLiterals.end(); }
  /// @return iterator to the first literal
  auto literal_begin() const { return llvmLiterals.begin(); }
  /// @return iterator beyond the last literal
  auto literal_end() const { return llvmLiterals.end(); }

  /// @return a reference to the first literal
  auto &literal_front() { return llvmLiterals.front(); }
  /// @return a reference to the last literal
  auto &literal_back() { return llvmLiterals.back(); }
  /// @return a reference to the first literal
  auto &literal_front() const { return llvmLiterals.front(); }
  /// @return a reference to the last literal
  auto &literal_back() const { return llvmLiterals.back(); }

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
  Var *doClone() const override;

  std::ostream &doFormat(std::ostream &os) const override;
};

} // namespace ir
} // namespace seq
