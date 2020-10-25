#pragma once

#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/cache.h"
#include "parser/ast/context.h"
#include "parser/common.h"

namespace seq {
namespace ast {

struct TypecheckItem {
  enum Kind { Func, Type, Var } kind;
  types::TypePtr type;
  std::string base;
  bool global;
  bool genericType;
  bool staticType;
  std::unordered_set<std::string> attributes;

  TypecheckItem(Kind k, types::TypePtr type, const std::string &base,
                bool global = false, bool generic = false, bool stat = false)
      : kind(k), type(type), base(base), global(global), genericType(generic),
        staticType(stat) {}

  std::string getBase() const { return base; }
  bool isGlobal() const { return global; }
  bool isVar() const { return kind == Var; }
  bool isFunc() const { return kind == Func; }
  bool isType() const { return kind == Type; }
  bool isGeneric() const { return isType() && genericType; }
  bool isStatic() const { return isType() && staticType; }
  types::TypePtr getType() const { return type; }
  bool hasAttr(const std::string &s) const {
    return attributes.find(s) != attributes.end();
  }
};

class TypeContext : public Context<TypecheckItem> {
public:
  std::shared_ptr<Cache> cache;

  struct RealizationBase {
    std::string name;
    types::TypePtr type;
    types::TypePtr returnType;
    std::unordered_map<std::string, std::pair<TypecheckItem::Kind, types::TypePtr>>
        visitedAsts;
  };
  std::vector<RealizationBase> bases;

  int typecheckLevel;
  /// Set of active unbound variables.
  /// If type checking is successful, all of them should be resolved.
  std::set<types::TypePtr> activeUnbounds;
  int iteration;

  std::stack<bool> partializeMethod;

public:
  TypeContext(std::shared_ptr<Cache> cache);

  int findBase(const std::string &b) {
    for (int i = int(bases.size()) - 1; i >= 0; i--)
      if (b == bases[i].name)
        return i; // bases[i].type;
    seqassert(false, "cannot find base '{}'", b);
    return -1;
  }

  std::shared_ptr<TypecheckItem> find(const std::string &name) const;
  types::TypePtr findInternal(const std::string &name) const;

  using Context<TypecheckItem>::add;
  std::shared_ptr<TypecheckItem> add(TypecheckItem::Kind kind, const std::string &name,
                                     types::TypePtr type = nullptr, bool global = false,
                                     bool generic = false, bool stat = false);
  void dump(int pad = 0) override;

public:
  std::string getBase() const;
  int getLevel() const { return bases.size(); }
  std::pair<TypecheckItem::Kind, types::TypePtr>
  findInVisited(const std::string &name) const;

public:
  std::shared_ptr<types::LinkType> addUnbound(const SrcInfo &srcInfo, int level,
                                              bool setActive = true,
                                              bool isStatic = false);
  /// Calls `type->instantiate`, but populates the instantiation table
  /// with "parent" type.
  /// Example: for list[T].foo, list[int].foo will populate type of foo so that
  /// the generic T gets mapped to int.
  types::TypePtr instantiate(const SrcInfo &srcInfo, types::TypePtr type);
  types::TypePtr instantiate(const SrcInfo &srcInfo, types::TypePtr type,
                             types::ClassTypePtr generics, bool activate = true);
  types::TypePtr instantiateGeneric(const SrcInfo &srcInfo, types::TypePtr root,
                                    const std::vector<types::TypePtr> &generics);

  const std::vector<types::FuncTypePtr> *findMethod(const std::string &typeName,
                                                    const std::string &method) const {
    auto m = cache->classMethods.find(typeName);
    if (m != cache->classMethods.end()) {
      auto t = m->second.find(method);
      if (t != m->second.end())
        return &t->second;
    }
    return nullptr;
  }

  types::TypePtr findMember(const std::string &typeName,
                            const std::string &member) const {
    auto m = cache->classMembers.find(typeName);
    if (m != cache->classMembers.end()) {
      for (auto &mm : m->second)
        if (mm.first == member)
          return mm.second;
    }
    return nullptr;
  }
};

} // namespace ast
} // namespace seq
