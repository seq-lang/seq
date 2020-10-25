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
  void setType(types::Type *type);
  types::Type *getType() const;
  void setTryCatch(TryCatch *tc);
  TryCatch *getTryCatch() const;
  virtual llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                               llvm::BasicBlock *&block) = 0;
  virtual bool isCatchAll();
};

class Wildcard : public Pattern {
private:
  Var *var;

public:
  Wildcard();
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
  bool isCatchAll() override;
  Var *getVar();
};

class BoundPattern : public Pattern {
private:
  Var *var;
  Pattern *pattern;

public:
  explicit BoundPattern(Pattern *pattern);
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
  bool isCatchAll() override;
  Var *getVar();
};

class StarPattern : public Pattern {
public:
  StarPattern();
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
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
  bool isCatchAll() override;
};

class ArrayPattern : public Pattern {
  std::vector<Pattern *> patterns;

public:
  explicit ArrayPattern(std::vector<Pattern *> patterns);
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
};

class SeqPattern : public Pattern {
  std::string pattern;
  unsigned k;  // k-mer length if matching k-mers, else 0

public:
  explicit SeqPattern(std::string pattern, unsigned k = 0);
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
};

class OptPattern : public Pattern {
private:
  Pattern *pattern;

public:
  explicit OptPattern(Pattern *pattern);
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
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
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
  bool isCatchAll() override;
};

class GuardedPattern : public Pattern {
  Pattern *pattern;
  Expr *guard;

public:
  explicit GuardedPattern(Pattern *pattern, Expr *guard);
  llvm::Value *codegen(BaseFunc *base, types::Type *type, llvm::Value *val,
                       llvm::BasicBlock *&block) override;
};

} // namespace seq
