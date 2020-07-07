#pragma once

#include "lang/stmt.h"
#include "types/func.h"
#include "types/types.h"
#include "util/common.h"
#include <unordered_map>

namespace seq {
class Expr;
class Var;
class Return;
class Yield;

/**
 * Abstract base function class, representing functions in a program
 * or the top-level module (which ultimately also compiles to an LLVM
 * function).
 */
class BaseFunc {
protected:
  /// Type containing this function as a method, or null if none
  types::Type *parentType;

  /// Module containing this function
  llvm::Module *module;

  /// This function's first block (where alloca's are codegen'd)
  llvm::BasicBlock *preambleBlock;

  /// LLVM function representing this function
  llvm::Function *func;

  BaseFunc();

public:
  virtual bool isGen();
  virtual void codegen(llvm::Module *module) = 0;
  llvm::LLVMContext &getContext();
  llvm::BasicBlock *getPreamble() const;
  virtual types::FuncType *getFuncType();
  virtual llvm::Function *getFunc(llvm::Module *module);
  virtual void setEnclosingClass(types::Type *parentType);

  virtual void sawReturn(Return *ret) {}
  virtual void sawYield(Yield *ret) {}
};

/**
 * Functions in a program are represented by this class.
 */
class Func : public BaseFunc, public SrcObject {
private:
  /// Cache of built-in functions
  static std::unordered_map<std::string, Func *> builtins;

  /// Whether this function is externally-defined (e.g. via `cdef`)
  bool external;

  /// Name of this function
  std::string name;

  /// Function argument types
  std::vector<types::Type *> inTypes;

  /// Function return type
  types::Type *outType;

  /// Original function return type, before deduction
  types::Type *outType0;

  /// Default arguments, or null if none (corresponds to `inTypes` vector)
  std::vector<Expr *> defaultArgs;

  /// Block representing this function's body
  Block *scope;

  /// Vector of this function's argument names
  std::vector<std::string> argNames;

  /// Map of argument name to variable representing the
  /// corresponding argument
  std::map<std::string, Var *> argVars;

  /// Vector of attributes given to this function
  std::vector<std::string> attributes;

  /// Function enclosing this function, or null if none
  Func *parentFunc;

  /// First return statement contained in this function,
  /// or null if none
  Return *ret;

  /// First yield statement contained in this function,
  /// or null if none
  Yield *yield;

  /// Whether this function contains a `prefetch` statement
  bool prefetch;

  /// Whether this function performs inter-sequence alignment
  bool interAlign;

  /*
   * Refer to https://llvm.org/docs/Coroutines.html for more
   * details on the following fields
   */

  /// Whether this function is a generator
  bool gen;

  /// Storage for this coroutine's promise, or null if none
  llvm::Value *promise;

  /// Coroutine handle, or null if none
  llvm::Value *handle;

  /// Coroutine cleanup block, or null if none
  llvm::BasicBlock *cleanup;

  /// Coroutine suspend block, or null if none
  llvm::BasicBlock *suspend;

  /// Coroutine exit block, or null if none
  llvm::BasicBlock *exit;

  /// Returns this function's mangled name.
  std::string getMangledFuncName();

public:
  Func();
  Block *getBlock();

  std::string genericName();
  void setEnclosingFunc(BaseFunc *parentFunc);
  void sawReturn(Return *ret) override;
  void sawYield(Yield *yield) override;
  void addAttribute(std::string attr);
  std::vector<std::string> getAttributes();
  bool hasAttribute(const std::string &attr);

  void codegen(llvm::Module *module) override;
  void codegenReturn(llvm::Value *val, types::Type *type,
                     llvm::BasicBlock *&block, bool dryrun = false);
  void codegenYield(llvm::Value *val, types::Type *type,
                    llvm::BasicBlock *&block, bool empty = false,
                    bool dryrun = false);
  llvm::Value *codegenYieldExpr(llvm::BasicBlock *&block, bool suspend = true);

  bool isGen() override;
  Var *getArgVar(std::string name);
  types::FuncType *getFuncType() override;

  void setExternal();
  void setIns(std::vector<types::Type *> inTypes);
  void setOut(types::Type *outType);
  void setDefaults(std::vector<Expr *> defaultArgs);
  void setName(std::string name);
  std::vector<std::string> getArgNames();
  void setArgNames(std::vector<std::string> argNames);
  static Func *getBuiltin(const std::string &name);
};

/**
 * This class is a useful abstraction for LLVM functions that do not
 * necessarily correspond to an actual source function. For example,
 * some methods of built-in types are implemented through this class.
 */
class BaseFuncLite : public BaseFunc {
private:
  /// Function argument types
  std::vector<types::Type *> inTypes;

  /// Function return type
  types::Type *outType;

  /// Lambda function for code generation
  std::function<llvm::Function *(llvm::Module *)> codegenLambda;

public:
  BaseFuncLite(std::vector<types::Type *> inTypes, types::Type *outType,
               std::function<llvm::Function *(llvm::Module *)> codegenLambda);
  void codegen(llvm::Module *module) override;
  types::FuncType *getFuncType() override;
};

} // namespace seq
