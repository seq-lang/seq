/**
 * types.h
 * Type definitions and type inference algorithm.
 *
 * Basic implementation of Hindley-Milner's W algorithm.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "parser/common.h"

namespace seq {
namespace ast {
namespace types {

struct FuncType;
struct ClassType;
struct LinkType;
struct GenericType;
struct StaticType;
struct ImportType;
struct Type;
typedef std::shared_ptr<Type> TypePtr;
typedef std::shared_ptr<FuncType> FuncTypePtr;
typedef std::shared_ptr<ClassType> ClassTypePtr;
typedef std::shared_ptr<LinkType> LinkTypePtr;
typedef std::shared_ptr<GenericType> GenericTypePtr;
typedef std::shared_ptr<StaticType> StaticTypePtr;
typedef std::shared_ptr<ImportType> ImportTypePtr;

struct Unification {
  std::vector<LinkTypePtr> linked;
  std::vector<std::pair<LinkTypePtr, int>> leveled;
  void undo();
};

struct Type : public seq::SrcObject, public std::enable_shared_from_this<Type> {
public:
  /// The following procedures implement the quintessential parts of
  /// Hindley-Milner's Algorithm W.
  ///
  /// (a) Unification: merge (unify) t with the current type.
  virtual int unify(TypePtr t, Unification &us) = 0;
  /// (b) Generalization: generalize all unbound types
  ///     whose level is less than [level] to generic types.
  virtual TypePtr generalize(int level) = 0;
  /// (c) Instantiation: instantiate all generic types as unbound types.
  ///     Uses [cache] lookup to ensure that same generics are linked with same
  ///     unbound types (e.g. dict[T, list[T]] should get instantiated to
  ///                         dict[?1, list[?1]]).
  virtual TypePtr instantiate(int level, int &unboundCount,
                              std::unordered_map<int, TypePtr> &cache) = 0;

public:
  /// Get the  type (follow all links)
  virtual TypePtr follow();
  /// Does this type have an unbound type within
  /// (e.g. list[?] has while list[int] does not)
  virtual bool hasUnbound() const = 0;
  /// Can we realize this type?
  virtual bool canRealize() const = 0;

  /// Pretty-printing.
  virtual std::string toString(bool reduced = false) const = 0;
  /// Allow pretty-printing to C++ streams
  friend std::ostream &operator<<(std::ostream &out, const Type &c) {
    return out << c.toString();
  }

  virtual GenericTypePtr getGeneric() { return nullptr; }
  virtual FuncTypePtr getFunc() { return nullptr; }
  virtual ClassTypePtr getClass() { return nullptr; }
  virtual LinkTypePtr getLink() { return nullptr; }
  virtual LinkTypePtr getUnbound() { return nullptr; }
  virtual StaticTypePtr getStatic() { return nullptr; }
  virtual ImportTypePtr getImport() { return nullptr; }
};

struct StaticType : public Type {
  int value;
  StaticType(int value) : value(value) {}

public:
  virtual int unify(TypePtr typ, Unification &us) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;

public:
  bool hasUnbound() const override { return false; }
  bool canRealize() const override { return true; }
  std::string toString(bool reduced = false) const override;
  StaticTypePtr getStatic() override {
    return std::static_pointer_cast<StaticType>(shared_from_this());
  }
};

struct ImportType : public Type {
  std::string name;

public:
  ImportType(const std::string &name) : name(name) {}

  int unify(TypePtr typ, Unification &us) override {
    if (auto t = CAST(typ, ImportType))
      return name == t->name ? 0 : -1;
    return -1;
  }
  TypePtr generalize(int level) override { return shared_from_this(); }
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override {
    return shared_from_this();
  }

public:
  bool hasUnbound() const override { return false; }
  bool canRealize() const override { return false; }
  std::string toString(bool reduced) const override {
    return fmt::format("<{}>", name);
  }
  ImportTypePtr getImport() override {
    return std::static_pointer_cast<ImportType>(shared_from_this());
  }
};

/**
 * LinkType is a fundamental type classifier.
 * It has three states:
 * - Unbound: type is currently unknown and will be (hopefully)
 *            made "known" (e.g. casted to Link) through the
 *            unification algorithm.
 *            Unbounds are casted to Generics during the generalization.
 *            Represented as ?-id (e.g. ?-3).
 * - Generic: type is a generic type.
 *            Generics are casted to Unbounds during the instantiation.
 *            Represented as T-id (e.g. T-3).
 * - Link:    a link to another type.
 *            Represented as &type (e.g. &int).
 *
 * Type for each non-type expression is expressed as a LinkType.
 */
struct LinkType : public Type {
  enum Kind { Unbound, Generic, Link } kind;
  /// ID of unbound and generic variants. Should not clash!
  int id;
  /// Level of unbound variant
  int level;
  /// Type of link variant. nullptr otherwise.
  TypePtr type;

  LinkType(Kind kind, int id, int level = 0, TypePtr type = nullptr);
  LinkType(TypePtr type) : kind(Link), id(0), level(0), type(type) {}
  virtual ~LinkType() {}

public:
  int unify(TypePtr typ, Unification &us) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;

public:
  TypePtr follow() override;
  bool hasUnbound() const override;
  bool canRealize() const override;
  std::string toString(bool reduced) const override;

  LinkTypePtr getLink() override {
    return std::static_pointer_cast<LinkType>(follow());
  }
  LinkTypePtr getUnbound() override {
    return kind == Unbound
               ? std::static_pointer_cast<LinkType>(shared_from_this())
               : nullptr;
  }
  GenericTypePtr getGeneric() override {
    return std::dynamic_pointer_cast<GenericType>(follow());
  }
  FuncTypePtr getFunc() override {
    return std::dynamic_pointer_cast<FuncType>(follow());
  }
  ClassTypePtr getClass() override {
    return std::dynamic_pointer_cast<ClassType>(follow());
  }
  StaticTypePtr getStatic() override {
    return std::dynamic_pointer_cast<StaticType>(follow());
  }

private:
  bool occurs(TypePtr typ, Unification &us);
};

struct GenericType : public Type {
  struct Generic {
    std::string name;
    int id;
    TypePtr type;
    bool isStatic;
    // -1 is for tuple "generics"
    Generic(const std::string name, TypePtr type, int id, bool st = false)
        : name(name), id(id), type(type), isStatic(st) {}
  };
  std::vector<Generic> explicits, implicits;

public:
  GenericType(const std::vector<Generic> &explicits = std::vector<Generic>(),
              const std::vector<Generic> &implicits = std::vector<Generic>());

  virtual std::string toString(bool reduced = false) const override;
  virtual bool hasUnbound() const override;
  virtual bool canRealize() const override;
  virtual TypePtr generalize(int level) override;
  virtual TypePtr instantiate(int level, int &unboundCount,
                              std::unordered_map<int, TypePtr> &cache) override;
  virtual int unify(TypePtr t, Unification &us) override;

  GenericTypePtr getGeneric() override {
    return std::static_pointer_cast<GenericType>(shared_from_this());
  }
};

/**
 * ClassType describes a (generic) class type.
 */
struct ClassType : public GenericType {
  /// Global unique name for each type (generated from the getSrcPos()).
  std::string name;
  /// Distinguish between records and classes
  bool record;
  std::vector<TypePtr> recordMembers;

  ClassType(const std::string &name, bool isRecord = false,
            const std::vector<TypePtr> &recordMembers = std::vector<TypePtr>(),
            std::shared_ptr<GenericType> generics = nullptr);

public:
  virtual int unify(TypePtr typ, Unification &us) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;

public:
  bool hasUnbound() const override;
  bool canRealize() const override;
  std::string toString(bool reduced = false) const override;
  ClassTypePtr getClass() override {
    return std::static_pointer_cast<ClassType>(shared_from_this());
  }
  bool isRecord() const { return record; }
};

/**
 * FuncType describes a (generic) function type.
 */
struct FuncType : public GenericType {
  struct RealizationInfo {
    struct Arg {
      std::string name;
      TypePtr type;
      std::string defaultVar;
    };
    std::string name;
    std::vector<int> pending; // loci in resolvedArgs
    std::vector<Arg> args;    // name, value
    RealizationInfo(const std::string &name, const std::vector<int> &pending,
                    const std::vector<Arg> &args)
        : name(name), pending(pending), args(args) {}
  };
  std::shared_ptr<RealizationInfo> realizationInfo;

  /// Empty name indicates "free" function type that can unify to any other
  /// function type
  std::vector<TypePtr> args;

  FuncType(const std::vector<TypePtr> &args = std::vector<TypePtr>(),
           std::shared_ptr<GenericType> generics = nullptr);

public:
  virtual int unify(TypePtr typ, Unification &us) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;

public:
  bool hasUnbound() const override;
  bool canRealize() const override;
  std::string toString(bool reduced = false) const override;
  FuncTypePtr getFunc() override {
    return std::static_pointer_cast<FuncType>(shared_from_this());
  }

  // FuncTypePtr getFullType() const {
  //   assert(realizationInfo);
  //   std::vector<TypePtr> types { args[0] };
  //   for (auto &a: realizationInfo->args) {
  //     types.push_back(a.type);
  //     assert(types.back());
  //   }
  //   return std::make_shared<FuncType>(types, getGeneric());
  // }
};

} // namespace types
} // namespace ast
} // namespace seq
