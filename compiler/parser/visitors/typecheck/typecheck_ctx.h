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
  string base;
  bool global;
  bool genericType;
  bool staticType;
  unordered_set<string> attributes;

  TypecheckItem(Kind k, types::TypePtr type, const string &base, bool global = false,
                bool generic = false, bool stat = false)
      : kind(k), type(type), base(base), global(global), genericType(generic),
        staticType(stat) {}

  string getBase() const { return base; }
  bool isGlobal() const { return global; }
  bool isVar() const { return kind == Var; }
  bool isFunc() const { return kind == Func; }
  bool isType() const { return kind == Type; }
  bool isGeneric() const { return isType() && genericType; }
  bool isStatic() const { return isType() && staticType; }
  types::TypePtr getType() const { return type; }
  bool hasAttr(const string &s) const { return attributes.find(s) != attributes.end(); }
};

class TypeContext : public Context<TypecheckItem> {
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

  std::stack<bool> partializeMethod;

public:
  TypeContext(shared_ptr<Cache> cache);

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
                                types::TypePtr type = nullptr, bool global = false,
                                bool generic = false, bool stat = false);
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

  const vector<types::FuncTypePtr> *findMethod(const string &typeName,
                                               const string &method) const {
    auto m = cache->classMethods.find(typeName);
    if (m != cache->classMethods.end()) {
      auto t = m->second.find(method);
      if (t != m->second.end())
        return &t->second;
    }
    return nullptr;
  }

  types::TypePtr findMember(const string &typeName, const string &member) const {
    auto m = cache->classFields.find(typeName);
    if (m != cache->classFields.end()) {
      for (auto &mm : m->second)
        if (mm.first == member)
          return mm.second;
    }
    return nullptr;
  }
};

} // namespace ast
} // namespace seq
