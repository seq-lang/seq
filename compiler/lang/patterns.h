#pragma once

#include "lang/func.h"
#include "types/types.h"
#include "util/common.h"
#include <vector>

namespace seq {
class TryCatch;

/**
 * Pattern class representing patterns used in `match` statements.
 */
class Pattern : public SrcObject {
private:
  types::Type *type;
  TryCatch *tc;

public:
  explicit Pattern(types::Type *type);
  void setTryCatch(TryCatch *tc);
  TryCatch *getTryCatch();
  virtual void resolveTypes(types::Type *type);
  virtual llvm::Value *codegen(BaseFunc *base, types::Type *type,
                               llvm::Value *val, llvm::BasicBlock *&block) = 0;
  virtual bool isCatchAll();
  virtual Pattern *clone(Generic *ref);
};

class Wildcard : public Pattern {
private:
  Var *var;

public:
  Wildcard();
  void resolveTypes(types::Type *type) override;
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
  bool isCatchAll() override;
  Wildcard *clone(Generic *ref) override;
  Var *getVar();
};

class BoundPattern : public Pattern {
private:
  Var *var;
  Pattern *pattern;

public:
  explicit BoundPattern(Pattern *pattern);
  void resolveTypes(types::Type *type) override;
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
  bool isCatchAll() override;
  BoundPattern *clone(Generic *ref) override;
  Var *getVar();
};

class StarPattern : public Pattern {
public:
  StarPattern();
  void resolveTypes(types::Type *type) override;
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
};

class IntPattern : public Pattern {
private:
  seq_int_t val;

public:
  explicit IntPattern(seq_int_t val);
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
};

class BoolPattern : public Pattern {
private:
  bool val;

public:
  explicit BoolPattern(bool val);
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
};

class StrPattern : public Pattern {
private:
  std::string val;

public:
  explicit StrPattern(std::string val);
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
};

class RecordPattern : public Pattern {
  std::vector<Pattern *> patterns;

public:
  explicit RecordPattern(std::vector<Pattern *> patterns);
  void resolveTypes(types::Type *type) override;
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
  bool isCatchAll() override;
  RecordPattern *clone(Generic *ref) override;
};

class ArrayPattern : public Pattern {
  std::vector<Pattern *> patterns;

public:
  explicit ArrayPattern(std::vector<Pattern *> patterns);
  void resolveTypes(types::Type *type) override;
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
  ArrayPattern *clone(Generic *ref) override;
};

class SeqPattern : public Pattern {
  std::string pattern;

public:
  explicit SeqPattern(std::string pattern);
  void resolveTypes(types::Type *type) override;
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
};

class OptPattern : public Pattern {
private:
  Pattern *pattern;

public:
  explicit OptPattern(Pattern *pattern);
  void resolveTypes(types::Type *type) override;
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
  OptPattern *clone(Generic *ref) override;
};

class RangePattern : public Pattern {
private:
  seq_int_t a;
  seq_int_t b;

public:
  RangePattern(seq_int_t a, seq_int_t b);
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
};

class OrPattern : public Pattern {
  std::vector<Pattern *> patterns;

public:
  explicit OrPattern(std::vector<Pattern *> patterns);
  void resolveTypes(types::Type *type) override;
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
  bool isCatchAll() override;
  OrPattern *clone(Generic *ref) override;
};

class GuardedPattern : public Pattern {
  Pattern *pattern;
  Expr *guard;

public:
  explicit GuardedPattern(Pattern *pattern, Expr *guard);
  void resolveTypes(types::Type *type) override;
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
  GuardedPattern *clone(Generic *ref) override;
};

} // namespace seq
