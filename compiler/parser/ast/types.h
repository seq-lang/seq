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

// #define TYPE_DEBUG

namespace seq {
namespace ast {

struct Expr;

namespace types {

struct FuncType;
struct ClassType;
struct LinkType;
struct StaticType;
struct ImportType;
struct Type;
typedef std::shared_ptr<Type> TypePtr;
typedef std::shared_ptr<LinkType> LinkTypePtr;

struct Unification {
  std::vector<LinkTypePtr> linked;
  std::vector<std::pair<LinkTypePtr, int>> leveled;
  void undo();
};

std::string v2b(const std::vector<char> &c);

struct Generic {
  std::string name;
  int id;
  TypePtr type;
  // -1 is for tuple "generics"
  Generic() : name(""), id(-1), type(nullptr) {}
  Generic(const std::string name, TypePtr type, int id)
      : name(name), id(id), type(type) {}
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
  virtual std::string realizeString() const = 0;
  /// Allow pretty-printing to C++ streams
  // friend std::ostream &operator<<(std::ostream &out, const Type &c) {
  //   return out << c.toString();
  // }

  virtual std::shared_ptr<FuncType> getFunc() { return nullptr; }
  virtual std::shared_ptr<ClassType> getClass() { return nullptr; }
  virtual std::shared_ptr<LinkType> getLink() { return nullptr; }
  virtual std::shared_ptr<LinkType> getUnbound() { return nullptr; }
  virtual std::shared_ptr<StaticType> getStatic() { return nullptr; }
  virtual std::shared_ptr<ImportType> getImport() { return nullptr; }
  // virtual bool isStatic() { return false; }
};

struct StaticType : public Type {
  std::vector<Generic> explicits;
  std::unique_ptr<Expr> expr;
  StaticType(const std::vector<Generic> &ex, std::unique_ptr<Expr> &&expr);

public:
  virtual int unify(TypePtr typ, Unification &us) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;

public:
  bool hasUnbound() const override;
  bool canRealize() const override;
  std::string toString(bool reduced = false) const override;
  std::string realizeString() const override;
  int getValue() const;
  std::shared_ptr<StaticType> getStatic() override {
    return std::static_pointer_cast<StaticType>(shared_from_this());
  }
};
typedef std::shared_ptr<StaticType> StaticTypePtr;

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
  std::string realizeString() const override { assert(false); }
  std::shared_ptr<ImportType> getImport() override {
    return std::static_pointer_cast<ImportType>(shared_from_this());
  }
};
typedef std::shared_ptr<ImportType> ImportTypePtr;

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
  /// is static variable?
  bool isStatic;

  LinkType(Kind kind, int id, int level = 0, TypePtr type = nullptr,
           bool isStatic = false);
  LinkType(TypePtr type) : kind(Link), id(0), level(0), type(type), isStatic(false) {}
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
  std::string realizeString() const override;

  LinkTypePtr getLink() override {
    return std::static_pointer_cast<LinkType>(follow());
  }
  LinkTypePtr getUnbound() override {
    if (kind == Unbound)
      return std::static_pointer_cast<LinkType>(shared_from_this());
    if (kind == Link)
      return type->getUnbound();
    return nullptr;
  }
  std::shared_ptr<FuncType> getFunc() override {
    return std::dynamic_pointer_cast<FuncType>(follow());
  }
  std::shared_ptr<ClassType> getClass() override {
    return std::dynamic_pointer_cast<ClassType>(follow());
  }
  std::shared_ptr<StaticType> getStatic() override {
    return std::dynamic_pointer_cast<StaticType>(follow());
  }

private:
  bool occurs(TypePtr typ, Unification &us);
};

/**
 * ClassType describes a (generic) class type.
 */
typedef std::shared_ptr<ClassType> ClassTypePtr;
struct ClassType : public Type {
  std::vector<Generic> explicits;
  ClassTypePtr parent;

public:
  /// Global unique name for each type (generated from the getSrcPos())
  std::string name;
  /// Distinguish between records and classes
  bool record;
  /// Record or function members
  std::vector<TypePtr> args;

  ClassType(ClassTypePtr c)
      : explicits(c->explicits), parent(c->parent), name(c->name), record(c->record),
        args(c->args) {}
  ClassType(const std::string &name, bool isRecord = false,
            const std::vector<TypePtr> &args = std::vector<TypePtr>(),
            const std::vector<Generic> &explicits = std::vector<Generic>(),
            ClassTypePtr parent = nullptr);

public:
  virtual int unify(TypePtr typ, Unification &us) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;

public:
  bool hasUnbound() const override;
  bool canRealize() const override;
  std::string toString(bool reduced = false) const override;
  std::string realizeString(const std::string &n, bool deep = true,
                            int firstArg = 0) const;
  std::string realizeString() const override;
  ClassTypePtr getClass() override {
    return std::static_pointer_cast<ClassType>(shared_from_this());
  }
  ClassTypePtr getCallable();
  bool isRecord() const { return record; }
};

/**
 * FuncType describes a (generic) function type that can be realized.
 */
typedef std::shared_ptr<FuncType> FuncTypePtr;
struct FuncType : public ClassType {
  std::string canonicalName;

public:
  FuncType(ClassTypePtr c, const std::string &canonicalName = "");
  FuncType(const std::vector<TypePtr> &args = std::vector<TypePtr>(),
           const std::vector<Generic> &explicits = std::vector<Generic>(),
           ClassTypePtr parent = nullptr, const std::string &canonicalName = "");

public:
  std::string realizeString() const override;
  FuncTypePtr getFunc() override {
    return std::static_pointer_cast<FuncType>(shared_from_this());
  }
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;
};

struct PartialType : public ClassType {
  std::vector<char> knownTypes;
  PartialType(ClassTypePtr c, const std::vector<char> &k);
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;
  virtual int unify(TypePtr typ, Unification &us) override;
};
typedef std::shared_ptr<PartialType> PartialTypePtr;

} // namespace types
} // namespace ast
} // namespace seq
