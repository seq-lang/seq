#pragma once

#include "util/common.h"
#include "util/llvm.h"
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace seq {
class BaseFunc;
class Func;
class TryCatch;

class Stmt;

/**
 * Represents a block in a program (e.g. the body of a
 * loop). Essentially a wrapper around a vector of
 * statements.
 */
struct Block {
  /// Statement containing this block
  Stmt *parent;

  /// Vector of statements contained by this block
  std::vector<Stmt *> stmts;

  /// Constructs a block with specified parent.
  explicit Block(Stmt *parent = nullptr);

  /// Adds the given statement to this block.
  void add(Stmt *stmt);

  /// Sequentially generates code for each statement
  /// in this block.
  void codegen(llvm::BasicBlock *&block);
};

/**
 * Class from which all Seq statements derive.
 *
 * A "statement" can be thought of as an AST node,
 * potentially containing sub-expressions and
 * sub-statements.
 */
class Stmt : public SrcObject {
private:
  /// Human-readable name for this expression,
  /// mainly for debugging
  std::string name;

  /// Function to which this statement belongs
  BaseFunc *base;

  /// Vector of branch instructions representing
  /// breaks (if statement is a loop) to be filled
  /// in during codegen
  std::vector<llvm::BranchInst *> breaks;

  /// Vector of branch instructions representing
  /// continues (if statement is a loop) to be filled
  /// in during codegen
  std::vector<llvm::BranchInst *> continues;

protected:
  /// Block containing this statement
  Block *parent;

  /// Whether this statement represents a loop
  bool loop;

  /// Enclosing try-catch, if explicitly specified
  TryCatch *tc;

public:
  explicit Stmt(std::string name);

  /// Returns the given name of this statement.
  std::string getName() const;

  /// Returns the statement enclosing this statement,
  /// or null if none.
  Stmt *getPrev() const;

  /// Returns the parent (i.e. enclosing) block of this
  /// statement.
  Block *getParent();

  /// Sets the parent (i.e. enclosing) block of this
  /// statement.
  void setParent(Block *parent);

  /// Returns the function containing this statement.
  BaseFunc *getBase() const;

  /// Sets the function containing this statement.
  void setBase(BaseFunc *base);

  /// Returns the innermost enclosing loop statement.
  Stmt *findEnclosingLoop();

  /// Adds the given branch instruction as a "break"
  /// to the loop enclosing this statement.
  void addBreakToEnclosingLoop(llvm::BranchInst *inst);

  /// Adds the given branch instruction as a "continue"
  /// to the loop enclosing this statement.
  void addContinueToEnclosingLoop(llvm::BranchInst *inst);

  /// Sets the enclosing try-catch statement.
  void setTryCatch(TryCatch *tc);

  /// Returns the enclosing try-catch statement.
  TryCatch *getTryCatch();

  /// Returns whether this statement represents a loop.
  bool isLoop();

  /// Throws an exception if this statement is not a loop.
  void ensureLoop();

  /// Adds the given branch instruction as a "break"
  /// to this statement (ensuring that it is indeed a
  /// loop in the process).
  void addBreak(llvm::BranchInst *inst);

  /// Adds the given branch instruction as a "continue"
  /// to this statement (ensuring that it is indeed a
  /// loop in the process).
  void addContinue(llvm::BranchInst *inst);

  /// Sets the destination block for all branch instructions
  /// representing "breaks" contained in this statement
  /// (ensuring that it is indeed a loop in the process).
  void setBreaks(llvm::BasicBlock *block);

  /// Sets the destination block for all branch instructions
  /// representing "continues" contained in this statement
  /// (ensuring that it is indeed a loop in the process).
  void setContinues(llvm::BasicBlock *block);

  /// Delegates to \ref codegen0() "codegen0()"; catches
  /// exceptions and fills in source information.
  void codegen(llvm::BasicBlock *&block);

  /// Performs code generation for this statement.
  /// @param block reference to block where code should be
  ///              generated; possibly modified to point
  ///              to a new block where codegen should resume
  virtual void codegen0(llvm::BasicBlock *&block) = 0;
};

} // namespace seq

std::ostream &operator<<(std::ostream &os, seq::Stmt &stmt);
