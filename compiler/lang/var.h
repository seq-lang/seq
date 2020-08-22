#pragma once

#include "lang/func.h"
#include "types/types.h"
#include <stack>

namespace seq {

/**
 * Class representing a variable (global, local, etc.)
 * in a program.
 */
class Var {
private:
  /// Unique name for this variable
  std::string name;

  /// Type of this variable
  types::Type *type;

  /// Pointer to area where this variable is stored,
  /// be it an alloca or global memory
  llvm::Value *ptr;

  /// Module containing this variable
  llvm::Module *module;

  /// Whether this variable is global
  bool global;

  /// Whether this variable is thread-local
  bool tls;

  /// Whether this variable is global in the REPL
  bool repl;

  /// Whether this variable is externally visible or private
  bool external;

  /// Stack of variables to which this variable is "mapped".
  /// Mapped variables delegate all of their actions to the
  /// variable they are mapped to.
  std::stack<Var *> mapped;

  /// Creates an alloca or global variable if needed to store
  /// this variable.
  void allocaIfNeeded(BaseFunc *base);

public:
  /// Constructs a variable of specified type
  explicit Var(types::Type *type = nullptr);

  std::string getName();
  void setName(std::string name);
  bool isGlobal();
  void setGlobal();
  void setThreadLocal();
  void setREPL();
  void setExternal();
  void reset();

  /// Map this variable to another variable. This can be necessary
  /// to implement e.g. generator expressions, which need to be
  /// codegen'd in a separate (anonymous) function, meaning we
  /// cannot just reuse variables from the original function.
  /// This function can be called multiple times, and the mapped-to
  /// variables will be placed in a stack (e.g. if nested generators
  /// are being codegen'd).
  /// @param other variable to delegate to
  void mapTo(Var *other);

  /// Un-maps the last-mapped-to variable.
  void unmap();

  /// Returns a pointer to this variable's storage area.
  llvm::Value *getPtr(BaseFunc *base);

  /// Codegen's a load of this variable.
  /// @param base function containing the load
  /// @param block block to generate load in
  /// @param atomic whether this load is atomic
  llvm::Value *load(BaseFunc *base, llvm::BasicBlock *block, bool atomic = false);

  /// Codegen's a store of this variable.
  /// @param base function containing the store
  /// @param val value to store
  /// @param block block to generate store in
  /// @param atomic whether this store is atomic
  void store(BaseFunc *base, llvm::Value *val, llvm::BasicBlock *block,
             bool atomic = false);

  /// Sets the type of this variable.
  void setType(types::Type *type);

  /// Returns the type of this variable.
  types::Type *getType();
};

} // namespace seq
