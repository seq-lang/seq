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
typedef shared_ptr<Type> TypePtr;
typedef shared_ptr<LinkType> LinkTypePtr;

struct Unification {
  vector<LinkTypePtr> linked;
  vector<std::pair<LinkTypePtr, int>> leveled;
  bool isMatch;
  void undo();
  Unification() : isMatch(false) {}
};

string v2b(const vector<char> &c);

struct Generic {
  string name;
  int id;
  TypePtr type;
  shared_ptr<Expr> deflt;

  // -1 is for tuple "generics"
  Generic() : name(""), id(-1), type(nullptr), deflt(nullptr) {}
  Generic(const string name, TypePtr type, int id, shared_ptr<Expr> deflt = nullptr)
      : name(name), id(id), type(type), deflt(deflt) {}
  Generic(const string name, TypePtr type, int id, unique_ptr<Expr> deflt)
      : name(name), id(id), type(type), deflt(move(deflt)) {}
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
                              unordered_map<int, TypePtr> &cache) = 0;

public:
  /// Get the  type (follow all links)
  virtual TypePtr follow();
  /// Does this type have an unbound type within
  /// (e.g. list[?] has while list[int] does not)
  virtual vector<TypePtr> getUnbounds() const = 0;
  /// Can we realize this type?
  virtual bool canRealize() const = 0;

  /// Pretty-printing.
  virtual string toString(bool reduced = false) const = 0;
  virtual string realizeString() const = 0;
  /// Allow pretty-printing to C++ streams
  // friend std::ostream &operator<<(std::ostream &out, const Type &c) {
  //   return out << c.toString();
  // }

  virtual shared_ptr<FuncType> getFunc() { return nullptr; }
  virtual shared_ptr<ClassType> getClass() { return nullptr; }
  virtual shared_ptr<LinkType> getLink() { return nullptr; }
  virtual shared_ptr<LinkType> getUnbound() { return nullptr; }
  virtual shared_ptr<StaticType> getStatic() { return nullptr; }
  virtual shared_ptr<ImportType> getImport() { return nullptr; }
  // virtual bool isStatic() { return false; }
};

struct StaticType : public Type {
  vector<Generic> explicits;
  unique_ptr<Expr> expr;
  StaticType(const vector<Generic> &ex, unique_ptr<Expr> &&expr);
  StaticType(int i);

public:
  virtual int unify(TypePtr typ, Unification &us) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      unordered_map<int, TypePtr> &cache) override;

public:
  vector<TypePtr> getUnbounds() const override;
  bool canRealize() const override;
  string toString(bool reduced = false) const override;
  string realizeString() const override;
  int getValue() const;
  shared_ptr<StaticType> getStatic() override {
    return std::static_pointer_cast<StaticType>(shared_from_this());
  }
};
typedef shared_ptr<StaticType> StaticTypePtr;

struct ImportType : public Type {
  string name;

public:
  ImportType(const string &name) : name(name) {}

  int unify(TypePtr typ, Unification &us) override {
    if (auto t = CAST(typ, ImportType))
      return name == t->name ? 0 : -1;
    return -1;
  }
  TypePtr generalize(int level) override { return shared_from_this(); }
  TypePtr instantiate(int level, int &unboundCount,
                      unordered_map<int, TypePtr> &cache) override {
    return shared_from_this();
  }

public:
  vector<TypePtr> getUnbounds() const override { return {}; }
  bool canRealize() const override { return false; }
  string toString(bool reduced) const override { return fmt::format("<{}>", name); }
  string realizeString() const override { assert(false); }
  shared_ptr<ImportType> getImport() override {
    return std::static_pointer_cast<ImportType>(shared_from_this());
  }
};
typedef shared_ptr<ImportType> ImportTypePtr;

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
                      unordered_map<int, TypePtr> &cache) override;

public:
  TypePtr follow() override;
  vector<TypePtr> getUnbounds() const override;
  bool canRealize() const override;
  string toString(bool reduced) const override;
  string realizeString() const override;

  LinkTypePtr getLink() override {
    return std::dynamic_pointer_cast<LinkType>(shared_from_this());
  }
  LinkTypePtr getUnbound() override {
    if (kind == Unbound)
      return std::static_pointer_cast<LinkType>(shared_from_this());
    if (kind == Link)
      return type->getUnbound();
    return nullptr;
  }
  shared_ptr<FuncType> getFunc() override {
    return std::dynamic_pointer_cast<FuncType>(follow());
  }
  shared_ptr<ClassType> getClass() override {
    return std::dynamic_pointer_cast<ClassType>(follow());
  }
  shared_ptr<StaticType> getStatic() override {
    return std::dynamic_pointer_cast<StaticType>(follow());
  }

private:
  bool occurs(TypePtr typ, Unification &us);
};

/**
 * ClassType describes a (generic) class type.
 */
typedef shared_ptr<ClassType> ClassTypePtr;
struct ClassType : public Type {
  vector<Generic> explicits;
  TypePtr parent;
  bool isTrait;

public:
  /// Global unique name for each type (generated from the getSrcPos())
  string name;
  /// Distinguish between records and classes
  bool record;
  /// Record or function members
  vector<TypePtr> args;

  ClassType(ClassTypePtr c)
      : explicits(c->explicits), parent(c->parent), name(c->name), record(c->record),
        args(c->args) {}
  ClassType(const string &name, bool isRecord = false,
            const vector<TypePtr> &args = vector<TypePtr>(),
            const vector<Generic> &explicits = vector<Generic>(),
            TypePtr parent = nullptr);

public:
  virtual int unify(TypePtr typ, Unification &us) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      unordered_map<int, TypePtr> &cache) override;

public:
  vector<TypePtr> getUnbounds() const override;
  bool canRealize() const override;
  string toString(bool reduced = false) const override;
  string realizeString() const override;
  ClassTypePtr getClass() override {
    return std::static_pointer_cast<ClassType>(shared_from_this());
  }
  TypePtr getCallable();
  bool isRecord() const { return record; }
};

/**
 * FuncType describes a (generic) function type that can be realized.
 */
typedef shared_ptr<FuncType> FuncTypePtr;
struct FuncType : public Type {
  vector<Generic> explicits;
  TypePtr parent;
  ClassTypePtr funcClass;

  string name;
  vector<TypePtr> args;

public:
  FuncType(const string &name, ClassTypePtr funcClass,
           const vector<TypePtr> &args = vector<TypePtr>(),
           const vector<Generic> &explicits = vector<Generic>(),
           TypePtr parent = nullptr);

public:
  virtual int unify(TypePtr typ, Unification &us) override;
  vector<TypePtr> getUnbounds() const override;
  bool canRealize() const override;
  string realizeString() const override;
  FuncTypePtr getFunc() override {
    return std::static_pointer_cast<FuncType>(shared_from_this());
  }
  ClassTypePtr getClass() override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      unordered_map<int, TypePtr> &cache) override;
  string toString(bool reduced = false) const override;
};

} // namespace types
} // namespace ast
} // namespace seq
