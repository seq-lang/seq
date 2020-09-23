#pragma once

#include <deque>
#include <memory>
#include <set>
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

struct TransformItem {
  enum Kind { Func, Type, Var, Import } kind;
  std::string base;
  std::string canonicalName;
  bool global;
  bool genericType;
  bool staticType;

public:
  TransformItem(Kind k, const std::string &base, const std::string &canonical,
                bool global = false, bool generic = false, bool stat = false)
      : kind(k), base(base), canonicalName(canonical), global(global),
        genericType(generic), staticType(stat) {}

  std::string getBase() const { return base; }
  bool isGlobal() const { return global; }
  bool isVar() const { return kind == Var; }
  bool isFunc() const { return kind == Func; }
  bool isType() const { return kind == Type; }
  bool isImport() const { return kind == Import; }
  bool isGeneric() const { return isType() && genericType; }
  bool isStatic() const { return isType() && staticType; }
};

class TransformContext : public Context<TransformItem> {
public:
  std::shared_ptr<Cache> cache;
  SrcInfo getGeneratedPos() {
    return {"<generated>", cache->generatedID, cache->generatedID++, 0, 0};
  }

public:
  /// Current module-specific name prefix (stack of enclosing class/function
  /// scopes). Module toplevel has no base.
  struct Base {
    std::string name;
    ExprPtr ast;
    int parent; // parent index; -1 for toplevel!
    // bool referencesParent;

    Base(const std::string &name, ExprPtr ast = nullptr, int parent = -1)
        : name(name), ast(move(ast)), parent(parent) {}
    bool isType() const { return ast != nullptr; }
  };
  std::vector<Base> bases;

  std::vector<std::set<std::string>> captures;

public:
  TransformContext(const std::string &filename, std::shared_ptr<Cache> cache);
  virtual ~TransformContext();

  std::shared_ptr<TransformItem> find(const std::string &name) const;
  using Context<TransformItem>::add;
  std::shared_ptr<TransformItem> add(TransformItem::Kind kind, const std::string &name,
                                     const std::string &canonicalName = "",
                                     bool global = false, bool generic = false,
                                     bool stat = false);
  void dump(int pad = 0) override;

public:
  std::string getBase() const;
  int getLevel() const { return bases.size(); }

  std::string generateCanonicalName(const std::string &name);

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
  std::string findFile(const std::string &what, const std::string &relativeTo,
                       bool forceStdlib = false) const;
};

} // namespace ast
} // namespace seq
