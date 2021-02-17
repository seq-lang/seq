/*
 * types.h --- Seq type definitions.
 * Contains a basic implementation of Hindley-Milner's W algorithm.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "parser/common.h"

namespace seq {
namespace ast {

struct Expr;

namespace types {

/// Forward declarations

struct FuncType;
struct ClassType;
struct LinkType;
struct RecordType;
struct PartialType;
struct StaticType;

/**
 * A type instance that contains the basic plumbing for type inference.
 * The virtual methods are designed for Hindley-Milner's Algorithm W inference.
 * For more information, consult https://github.com/tomprimozic/type-systems.
 *
 * Type instances are widely mutated during the type inference and each type is intended
 * to be instantiated and manipulated as a shared_ptr.
 */
struct Type : public seq::SrcObject, public std::enable_shared_from_this<Type> {
  /// A structure that keeps the list of unification steps that can be undone later.
  /// Needed because the unify() is destructive.
  struct Unification {
    /// List of unbound types that have been changed.
    vector<LinkType *> linked;
    /// List of unbound types whose level has been changed.
    vector<pair<LinkType *, int>> leveled;
    /// List of static types that have been evaluated during the unification.
    vector<StaticType *> evaluated;

  public:
    /// Undo the unification step.
    void undo();
  };

public:
  /// Unifies a given type with the current type.
  /// ⚠️ Destructive operation! (both the current and a given type are modified).
  /// @param typ A given type.
  /// @param undo A reference to Unification structure to track the unification steps
  ///             and allow later undoing of the unification procedure.
  /// @return Unification score: -1 for failure, anything >= 0 for success.
  ///         Higher score translates to a "better" unification.
  virtual int unify(Type *typ, Unification *undo) = 0;
  /// Generalize all unbound types whose level is below the provided level.
  /// This method replaces all unbound types with a generic types (e.g. ?1 -> T1).
  /// Note that the generalized type keeps the unbound type's ID.
  virtual shared_ptr<Type> generalize(int atLevel) = 0;
  /// Instantiate all generic types. Inverse of generalize(): it replaces all
  /// generic types with new unbound types (e.g. T1 -> ?1234).
  /// Note that the instantiated type has a distinct and unique ID.
  /// @param level Level of the instantiation.
  /// @param unboundCount A reference of the unbound counter to ensure that no two
  ///                     unbound types share the same ID.
  /// @param cache A reference to a lookup table to ensure that all instances of a
  ///              generic point to the same unbound type (e.g. dict[T, list[T]] should
  ///              be instantiated as dict[?1, list[?1]]).
  virtual shared_ptr<Type> instantiate(int atLevel, int &unboundCount,
                                       unordered_map<int, shared_ptr<Type>> &cache) = 0;

public:
  /// Get the final type (follow through all LinkType links).
  /// For example, for (a->b->c->d) it returns d.
  virtual shared_ptr<Type> follow();
  /// Obtain the list of internal unbound types.
  virtual vector<shared_ptr<Type>> getUnbounds() const { return {}; }
  /// True if a type is realizable.
  virtual bool canRealize() const = 0;
  /// Pretty-print facility.
  /// @param reduced If True, do not print parent types.
  virtual string toString() const = 0;
  /// Print the realization string.
  /// Similar to toString, but does not print the data unnecessary for realization
  /// (e.g. the function return type).
  virtual string realizedName() const = 0;

  /// Convenience virtual functions to avoid unnecessary dynamic_cast calls.
  virtual shared_ptr<FuncType> getFunc() { return nullptr; }
  virtual shared_ptr<PartialType> getPartial() { return nullptr; }
  virtual shared_ptr<ClassType> getClass() { return nullptr; }
  virtual shared_ptr<RecordType> getRecord() { return nullptr; }
  virtual shared_ptr<LinkType> getLink() { return nullptr; }
  virtual shared_ptr<LinkType> getUnbound() { return nullptr; }
  virtual shared_ptr<StaticType> getStatic() { return nullptr; }
  virtual shared_ptr<RecordType> getHeterogenousTuple() { return nullptr; }

  virtual bool is(const string &s);
};
typedef shared_ptr<Type> TypePtr;

/**
 * A basic type-inference building block.
 * LinkType is a metatype (or a type state) that describes the current information about
 * the expression type:
 *   - Link: underlying expression type is known and can be accessed through a pointer
 *           type (link).
 *   - Unbound: underlying expression type is currently unknown and is being inferred.
 *              An unbound is represented as ?id (e.g. ?3).
 *   - Generic: underlying expression type is unknown.
 *              Unlike unbound types, generic types cannot be inferred as-is and must be
 *              instantiated prior to the inference.
 *              Used only in function and class definitions.
 *              A generic is represented as Tid (e.g. T3).
 */
struct LinkType : public Type {
  /// Enumeration describing the current state.
  enum Kind { Unbound, Generic, Link } kind;
  /// The unique identifier of an unbound or generic type.
  int id;
  /// The type-checking level of an unbound type.
  int level;
  /// The type to which LinkType points to. nullptr if unknown (unbound or generic).
  TypePtr type;
  /// True if a type is a static type (e.g. N in Int[N: int]).
  bool isStatic;

public:
  LinkType(Kind kind, int id, int level = 0, TypePtr type = nullptr,
           bool isStatic = false);
  /// Convenience constructor for linked types.
  explicit LinkType(TypePtr type);

public:
  int unify(Type *typ, Unification *undodo) override;
  TypePtr generalize(int atLevel) override;
  TypePtr instantiate(int atLevel, int &unboundCount,
                      unordered_map<int, TypePtr> &cache) override;

public:
  TypePtr follow() override;
  vector<TypePtr> getUnbounds() const override;
  bool canRealize() const override;
  string toString() const override;
  string realizedName() const override;

  shared_ptr<LinkType> getLink() override {
    return std::static_pointer_cast<LinkType>(shared_from_this());
  }
  shared_ptr<LinkType> getUnbound() override;
  shared_ptr<FuncType> getFunc() override {
    return kind == Link ? type->getFunc() : nullptr;
  }
  shared_ptr<PartialType> getPartial() override {
    return kind == Link ? type->getPartial() : nullptr;
  }
  shared_ptr<ClassType> getClass() override {
    return kind == Link ? type->getClass() : nullptr;
  }
  shared_ptr<RecordType> getRecord() override {
    return kind == Link ? type->getRecord() : nullptr;
  }
  shared_ptr<StaticType> getStatic() override {
    return kind == Link ? type->getStatic() : nullptr;
  }

private:
  /// Checks if a current (unbound) type occurs within a given type.
  /// Needed to prevent a recursive unification (e.g. ?1 with list[?1]).
  bool occurs(Type *typ, Type::Unification *undo);
};

/// Done till here

/**
 * A generic type declaration.
 * Each generic is defined by its name and unique ID.
 */
struct Generic {
  string name;
  int id;
  TypePtr type;
  shared_ptr<Expr> deflt;

  // -1 is for tuple "generics"
  explicit Generic() : name(), id(-1), type(nullptr), deflt(nullptr) {}
  Generic(string name, TypePtr type, int id, shared_ptr<Expr> deflt = nullptr)
      : name(move(name)), id(id), type(move(type)), deflt(move(deflt)) {}
  Generic(string name, TypePtr type, int id, unique_ptr<Expr> deflt)
      : name(move(name)), id(id), type(move(type)), deflt(move(deflt)) {}
};

/**
 * ClassType describes a (generic) class type.
 */
struct ClassType : public Type {
  /// Global unique name for each type (generated from the getSrcPos())
  string name;
  vector<Generic> generics;
  bool isTrait;

  explicit ClassType(string name, vector<Generic> generics = vector<Generic>());
  explicit ClassType(const shared_ptr<ClassType> &base);

public:
  int unify(Type *typ, Unification *undo) override;
  TypePtr generalize(int atLevel) override;
  TypePtr instantiate(int atLevel, int &unboundCount,
                      unordered_map<int, TypePtr> &cache) override;

public:
  vector<TypePtr> getUnbounds() const override;
  bool canRealize() const override;
  string toString() const override;
  string realizedName() const override;
  virtual bool hasTrait() const;
  virtual string realizedTypeName() const;
  shared_ptr<ClassType> getClass() override {
    return std::static_pointer_cast<ClassType>(shared_from_this());
  }
};
typedef shared_ptr<ClassType> ClassTypePtr;

struct RecordType : public ClassType {
  vector<TypePtr> args;

  explicit RecordType(string name, vector<Generic> generics = vector<Generic>(),
                      vector<TypePtr> args = vector<TypePtr>());
  RecordType(const ClassTypePtr &base, vector<TypePtr> args);

public:
  int unify(Type *typ, Unification *undo) override;
  TypePtr generalize(int atLevel) override;
  TypePtr instantiate(int atLevel, int &unboundCount,
                      unordered_map<int, TypePtr> &cache) override;

public:
  vector<TypePtr> getUnbounds() const override;
  bool canRealize() const override;
  string toString() const override;

  shared_ptr<RecordType> getRecord() override {
    return std::static_pointer_cast<RecordType>(shared_from_this());
  }
  shared_ptr<RecordType> getHeterogenousTuple() override;
};

/**
 * FuncType describes a (generic) function type that can be realized.
 */
struct FuncType : public RecordType {
  string funcName;
  vector<Generic> funcGenerics;
  TypePtr funcParent;

public:
  FuncType(const shared_ptr<RecordType> &baseType, string funcName,
           vector<Generic> funcGenerics = vector<Generic>(),
           TypePtr funcParent = nullptr);

public:
  int unify(Type *typ, Unification *undo) override;
  TypePtr generalize(int atLevel) override;
  TypePtr instantiate(int atLevel, int &unboundCount,
                      unordered_map<int, TypePtr> &cache) override;

public:
  vector<TypePtr> getUnbounds() const override;
  bool canRealize() const override;
  string toString() const override;
  string realizedName() const override;

  shared_ptr<FuncType> getFunc() override {
    return std::static_pointer_cast<FuncType>(shared_from_this());
  }
};
typedef shared_ptr<FuncType> FuncTypePtr;

/**
 * FuncType describes a (generic) function type that can be realized.
 */
struct PartialType : public RecordType {
  FuncTypePtr func;
  vector<char> used;

  // public:
  //  PartialType(const shared_ptr<FuncType> &baseType, const vector<char> &used);
  //
  // public:
  //  int unify(Type *typ, Unification *undo) override;
  //  TypePtr generalize(int atLevel) override;
  //  TypePtr instantiate(int atLevel, int &unboundCount,
  //                      unordered_map<int, TypePtr> &cache) override;
  //
  // public:
  //  vector<TypePtr> getUnbounds() const override;
  //  bool canRealize() const override;
  //  string toString() const override;
  //  string realizedName() const override;

  shared_ptr<PartialType> getPartial() override {
    return std::static_pointer_cast<PartialType>(shared_from_this());
  }
};
typedef shared_ptr<FuncType> FuncTypePtr;

struct StaticType : public Type {
  typedef std::function<int(const StaticType *)> EvalFn;
  vector<Generic> generics;

  pair<bool, int> staticEvaluation;
  pair<unique_ptr<Expr>, EvalFn> staticExpr;

  StaticType(vector<Generic> generics, pair<unique_ptr<Expr>, EvalFn> staticExpr,
             pair<bool, int> staticEvaluation);
  explicit StaticType(int i);

public:
  int unify(Type *typ, Unification *undo) override;
  TypePtr generalize(int atLevel) override;
  TypePtr instantiate(int atLevel, int &unboundCount,
                      unordered_map<int, TypePtr> &cache) override;

public:
  vector<TypePtr> getUnbounds() const override;
  bool canRealize() const override;
  string toString() const override;
  string realizedName() const override;

  shared_ptr<StaticType> getStatic() override {
    return std::static_pointer_cast<StaticType>(shared_from_this());
  }
};

} // namespace types
} // namespace ast
} // namespace seq
