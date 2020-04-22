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

struct FuncType;
struct ClassType;
struct Type;
typedef std::shared_ptr<Type> TypePtr;
typedef std::shared_ptr<FuncType> FuncTypePtr;
typedef std::shared_ptr<ClassType> ClassTypePtr;

struct Type : public seq::SrcObject, public std::enable_shared_from_this<Type> {
public:
  /// The following procedures implement the quintessential parts of
  /// Hindley-Milner's Algorithm W.
  ///
  /// (a) Unification: merge (unify) t with the current type.
  virtual int unify(TypePtr t) = 0;
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
  enum LinkKind { Unbound, Generic, Link };

  LinkKind kind;
  /// ID of unbound and generic variants. Should not clash!
  int id;
  /// Level of unbound variant
  int level;
  /// Type of link variant. nullptr otherwise.
  TypePtr type;

  LinkType(LinkKind kind, int id, int level = 0, TypePtr type = nullptr);
  LinkType(TypePtr type) : kind(Link), id(0), level(0), type(type) {}
  virtual ~LinkType() {}

public:
  int unify(TypePtr typ) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;

public:
  TypePtr follow() override;
  bool hasUnbound() const override;
  bool canRealize() const override;
  std::string toString(bool reduced) const override;

private:
  bool occurs(Type *typ);
};

/**
 * ClassType describes a (generic) class type.
 */
struct ClassType : public Type {
  /// Global unique name for each type (generated from the getSrcPos()).
  std::string name;
  /// Distinguish between records and classes
  bool isRecord;
  /// Each generic is represented as a pair (generic_id, current_type).
  /// It is necessary to maintain unique generic ID as defined in the
  /// "canonical" class type to be able to properly realize types.
  std::vector<std::pair<int, TypePtr>> generics;
  std::vector<std::pair<std::string, TypePtr>> args;

  ClassType(const std::string &name, bool isRecord,
            const std::vector<std::pair<int, TypePtr>> &generics,
            const std::vector<std::pair<std::string, TypePtr>> &args);
  virtual ~ClassType() {}

public:
  int unify(TypePtr typ) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;

public:
  bool hasUnbound() const override;
  bool canRealize() const override;
  std::string toString(bool reduced = false) const override;
};

/**
 * FuncType describes a (generic) function type.
 */
struct FuncType : public Type {
  /// Each generic is represented as a pair (generic_id, current_type).
  /// It is necessary to maintain unique generic ID as defined in the
  /// "canonical" class type to be able to properly realize types.
  std::vector<std::pair<int, TypePtr>> generics;
  /// We also need to keep "implicit generics" that are inherited from
  /// a generic class (for cases like e.g. class A[T]: def foo(): x = T())
  std::vector<std::pair<int, TypePtr>> implicitGenerics;
  std::vector<std::pair<std::string, TypePtr>> args;
  /// Return type. Usually deduced after the realization.
  TypePtr ret;

  FuncType(const std::vector<std::pair<int, TypePtr>> &generics,
           const std::vector<std::pair<std::string, TypePtr>> &args,
           TypePtr ret);
  virtual ~FuncType() {}

public:
  int unify(TypePtr typ) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;

public:
  bool hasUnbound() const override;
  bool canRealize() const override;
  std::string toString(bool reduced = false) const override;

public:
  void setImplicits(const std::vector<std::pair<int, TypePtr>> &i) {
    implicitGenerics = i;
  }
};

////////

} // namespace ast
} // namespace seq
