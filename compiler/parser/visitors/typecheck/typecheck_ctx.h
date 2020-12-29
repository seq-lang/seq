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
  int extendCount;
  bool needsAnotherIteration;

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
                                        const string &method) const {
    auto m = cache->classes.find(typeName);
    if (m != cache->classes.end()) {
      auto t = m->second.methods.find(method);
      if (t != m->second.methods.end()) {
        unordered_map<string, int> signatureLoci;
        vector<types::FuncTypePtr> vv;
        if (typeName == "AttributeError" && method == "__new__")
          assert(1);
        for (auto &mt : t->second)
          if (mt.age <= extendCount) {
            auto sig = cache->functions[mt.name].ast->signature();
            auto it = signatureLoci.find(sig);
            if (it != signatureLoci.end())
              vv[it->second] = mt.type;
            else {
              signatureLoci[sig] = vv.size();
              vv.emplace_back(mt.type);
            }
          }
        return vv;
      }
    }
    return {};
  }

  types::TypePtr findMember(const string &typeName, const string &member) const {
    auto m = cache->classes.find(typeName);
    if (m != cache->classes.end()) {
      for (auto &mm : m->second.fields)
        if (mm.name == member)
          return mm.type;
    }
    return nullptr;
  }
};

} // namespace ast
} // namespace seq
