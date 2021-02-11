#pragma once

#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/cache.h"
#include "parser/common.h"
#include "parser/ctx.h"

namespace seq {
namespace ast {

struct TypecheckItem {
  enum Kind { Func, Type, Var } kind;
  types::TypePtr type;
  bool staticType;

  TypecheckItem(Kind k, types::TypePtr type, bool stat = false)
      : kind(k), type(move(type)), staticType(stat) {}

  bool isType() const { return kind == Type; }
  bool isStatic() const { return isType() && staticType; }
};

struct TypeContext : public Context<TypecheckItem> {
public:
  shared_ptr<Cache> cache;

  struct RealizationBase {
    string name;
    types::TypePtr type;
    types::TypePtr returnType;
    unordered_map<string, std::pair<TypecheckItem::Kind, types::TypePtr>> visitedAsts;
  };
  vector<RealizationBase> bases;

  int typecheckLevel;
  /// Set of active unbound variables.
  /// If type checking is successful, all of them should be resolved.
  set<types::TypePtr> activeUnbounds;
  int iteration;
  bool needsAnotherIteration;
  bool allowActivation;
  int age;

  int realizationDepth;
  set<string> defaultCallDepth;

public:
  explicit TypeContext(shared_ptr<Cache> cache);

  int findBase(const string &b) {
    for (int i = int(bases.size()) - 1; i >= 0; i--)
      if (b == bases[i].name)
        return i; // bases[i].type;
    seqassert(false, "cannot find base '{}'", b);
    return -1;
  }

  shared_ptr<TypecheckItem> find(const string &name) const override;
  types::TypePtr findInternal(const string &name) const;

  using Context<TypecheckItem>::add;
  shared_ptr<TypecheckItem> add(TypecheckItem::Kind kind, const string &name,
                                types::TypePtr type = nullptr, bool stat = false);
  void dump() override { dump(0); }

protected:
  void dump(int pad);

public:
  string getBase() const;
  int getLevel() const { return bases.size(); }
  std::pair<TypecheckItem::Kind, types::TypePtr>
  findInVisited(const string &name) const;

public:
  shared_ptr<types::LinkType> addUnbound(const SrcInfo &srcInfo, int level,
                                         bool setActive = true, bool isStatic = false);
  /// Calls `type->instantiate`, but populates the instantiation table
  /// with "parent" type.
  /// Example: for list[T].foo, list[int].foo will populate type of foo so that
  /// the generic T gets mapped to int.
  types::TypePtr instantiate(const SrcInfo &srcInfo, types::TypePtr type);
  types::TypePtr instantiate(const SrcInfo &srcInfo, types::TypePtr type,
                             types::ClassType *generics, bool activate = true);
  types::TypePtr instantiateGeneric(const SrcInfo &srcInfo, types::TypePtr root,
                                    const vector<types::TypePtr> &generics);

  vector<types::FuncTypePtr> findMethod(const string &typeName,
                                        const string &method) const;
  types::TypePtr findMember(const string &typeName, const string &member) const;

  /// Picks the best method named member in a given type that matches the given argument
  /// types. Prefers methods whose signatures are closer to the given arguments (e.g.
  /// a foo(int) will match (int) better that a generic foo(T)). Also takes care of the
  /// Optional arguments.
  /// If multiple valid methods are found, returns the first one. Returns nullptr if no
  /// methods were found.
  types::FuncTypePtr findBestMethod(types::ClassType *typ, const string &member,
                                    const vector<pair<string, types::TypePtr>> &args);
  /// Reorders a given vector or named arguments (consisting of names and the
  /// corresponding types) according to the signature of a given function.
  /// Returns the reordered vector and an associated reordering score (missing
  /// default arguments' score is half of the present arguments).
  /// Score is -1 if the given arguments cannot be reordered.
  typedef std::function<int(const std::map<int, int> &, int, int,
                            const vector<vector<int>> &)>
      ReorderDoneFn;
  typedef std::function<int(string)> ReorderErrorFn;
  int reorderNamedArgs(types::RecordType *func, const string &knownTypes,
                       const vector<CallExpr::Arg> &args, ReorderDoneFn onDone,
                       ReorderErrorFn onError);
};

} // namespace ast
} // namespace seq
