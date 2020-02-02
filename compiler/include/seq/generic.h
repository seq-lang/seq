#ifndef SEQ_GENERIC_H
#define SEQ_GENERIC_H

#include "seq/types.h"
#include <cassert>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace seq {
class Expr;

/// Checks if two type vectors are "equal".
/// @param strict if true, do not treat isomorphic tuples with different names
/// as equal
template <typename T = types::Type>
static bool typeMatch(const std::vector<T *> &v1, const std::vector<T *> &v2,
                      bool strict = false) {
  if (v1.size() != v2.size())
    return false;

  for (unsigned i = 0; i < v1.size(); i++) {
    if (strict) {
      auto *r1 = dynamic_cast<types::RecordType *>(v1[i]);
      auto *r2 = dynamic_cast<types::RecordType *>(v2[i]);
      if (r1 && r2) {
        if (!r1->isStrict(r2))
          return false;
        continue;
      }
    }
    if (!types::is(v1[i], v2[i]))
      return false;
  }

  return true;
}

/**
 * Generic realization cache for caching instantiations of generic
 * functions, methods, classes, etc.
 * @tparam T type being realized
 */
template <typename T> class RCache {
private:
  std::vector<std::pair<std::vector<types::Type *>, T *>> cache;
  std::vector<std::vector<types::Type *>> inProgress;

public:
  RCache() : cache(), inProgress() {}

  void add(std::vector<types::Type *> types, T *t) {
    cache.emplace_back(std::move(types), t);
  }

  T *find(const std::vector<types::Type *> &types) {
    for (auto &v : inProgress) {
      if (v == types)
        return nullptr;
    }
    inProgress.push_back(types);
    for (auto &v : cache) {
      if (typeMatch<>(v.first, types, /*strict=*/true)) {
        inProgress.pop_back();
        return v.second;
      }
    }
    inProgress.pop_back();
    return nullptr;
  }
};

namespace types {
struct ExtInfo {
  std::string name;
  BaseFunc *func;
  bool force;
};

/**
 * A generic type simply delegates all of its functionality to some other type,
 * which is determined later during type resolution. However, this class is also
 * reused for two other purposes:
 *
 *   - Realized generic types like `A[int]`: Realization is deferred until
 *     absolutely necessary by this class.
 *   - `typeof` expressions: This class can hold an expression object whose
 *     type will not be checked until (again) absolutely necessary.
 *
 * This deferral allows some type-checking corner-cases to work properly.
 */
class GenericType : public Type {
private:
  /* standard generic type parameter */
  std::string genericName;
  mutable Type *type;

  /* realized type (e.g. A[int]) */
  RefType *pending;
  std::vector<types::Type *> types;
  std::vector<ExtInfo> extensions;

  /* typeof expression */
  Expr *expr;

public:
  GenericType();
  GenericType(RefType *pending, std::vector<Type *> types, Expr *expr);
  void setName(std::string name);
  void realize(Type *type);
  void realize() const;
  bool realized() const;
  void ensure() const;
  Type *getType() const;

  int getID() const override;
  std::string getName() const override;
  Type *getParent() const override;
  bool isAbstract() const override;
  VTable &getVTable() override;
  bool isAtomic() const override;

  llvm::Value *alloc(llvm::Value *count, llvm::BasicBlock *block) override;

  llvm::Value *call(BaseFunc *base, llvm::Value *self,
                    const std::vector<llvm::Value *> &args,
                    llvm::BasicBlock *block, llvm::BasicBlock *normal,
                    llvm::BasicBlock *unwind) override;

  llvm::Value *memb(llvm::Value *self, const std::string &name,
                    llvm::BasicBlock *block) override;

  Type *membType(const std::string &name) override;

  llvm::Value *setMemb(llvm::Value *self, const std::string &name,
                       llvm::Value *val, llvm::BasicBlock *block) override;

  bool hasMethod(const std::string &name) override;

  void addMethod(std::string name, BaseFunc *func, bool force) override;

  BaseFunc *getMethod(const std::string &name) override;

  llvm::Value *staticMemb(const std::string &name,
                          llvm::BasicBlock *block) override;

  Type *staticMembType(const std::string &name) override;

  llvm::Value *defaultValue(llvm::BasicBlock *block) override;

  llvm::Value *boolValue(llvm::Value *self, llvm::BasicBlock *&block,
                         TryCatch *tc) override;

  llvm::Value *strValue(llvm::Value *self, llvm::BasicBlock *&block,
                        TryCatch *tc) override;

  llvm::Value *lenValue(llvm::Value *self, llvm::BasicBlock *&block,
                        TryCatch *tc) override;

  void initOps() override;
  void initFields() override;
  Type *magicOut(const std::string &name, std::vector<Type *> args,
                 bool nullOnMissing = false,
                 bool overloadsOnly = false) override;
  llvm::Value *callMagic(const std::string &name, std::vector<Type *> argTypes,
                         llvm::Value *self, std::vector<llvm::Value *> args,
                         llvm::BasicBlock *&block, TryCatch *tc) override;
  Type *initOut(std::vector<Type *> &args, std::vector<std::string> names,
                bool nullOnMissing = false, Func **initFunc = nullptr) override;
  llvm::Value *callInit(std::vector<Type *> argTypes,
                        std::vector<std::string> names, llvm::Value *self,
                        std::vector<llvm::Value *> args,
                        llvm::BasicBlock *&block, TryCatch *tc) override;

  bool is(Type *type) const override;
  bool isGeneric(Type *type) const override;
  unsigned numBaseTypes() const override;
  Type *getBaseType(unsigned idx) const override;
  Type *getCallType(const std::vector<Type *> &inTypes) override;
  llvm::Type *getLLVMType(llvm::LLVMContext &context) const override;
  size_t size(llvm::Module *module) const override;

  RecordType *asRec() override;
  RefType *asRef() override;
  GenType *asGen() override;
  OptionalType *asOpt() override;
  KMer *asKMer() override;

  static GenericType *get();
  static GenericType *get(RefType *pending, std::vector<Type *> types);
  static GenericType *get(Expr *expr);

  GenericType *clone(Generic *ref) override;
  bool findInType(types::Type *type, std::vector<unsigned> &path,
                  bool unwrapOptionals);
};
} // namespace types

/**
 * Generic object abstraction for representing generic functions, methods,
 * classes, etc.
 */
class Generic {
private:
  std::vector<types::GenericType *> generics;
  std::map<void *, void *> cloneCache;

public:
  Generic();
  virtual std::string genericName() = 0;
  virtual Generic *clone(Generic *ref) = 0;
  virtual void addCachedRealized(std::vector<types::Type *> types, Generic *x);

  bool realized();
  std::vector<types::Type *> getRealizedTypes() const;
  bool is(Generic *other) const;
  void setCloneBase(Generic *x, Generic *ref);
  void addGenerics(int count);
  unsigned numGenerics() const;
  types::GenericType *getGeneric(int idx) const;
  bool seenClone(void *p);
  void *getClone(void *p);
  void addClone(void *p, void *clone);
  Generic *realizeGeneric(std::vector<types::Type *> types);
  std::vector<types::Type *>
  deduceTypesFromArgTypes(const std::vector<types::Type *> &inTypes,
                          const std::vector<types::Type *> &argTypes,
                          bool unwrapOptionals = true);
};
} // namespace seq

#endif /* SEQ_GENERIC_H */
