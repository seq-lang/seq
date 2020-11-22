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
  string base;
  string canonicalName;
  bool global;
  bool genericType;
  bool staticType;

public:
  TransformItem(Kind k, const string &base, const string &canonical,
                bool global = false, bool generic = false, bool stat = false)
      : kind(k), base(base), canonicalName(canonical), global(global),
        genericType(generic), staticType(stat) {}

  string getBase() const { return base; }
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
  shared_ptr<Cache> cache;
  SrcInfo getGeneratedPos() {
    return {"<generated>", cache->generatedID, cache->generatedID++, 0, 0};
  }

public:
  /// Current module-specific name prefix (stack of enclosing class/function
  /// scopes). Module toplevel has no base.
  struct Base {
    string name;
    ExprPtr ast;
    int parent; // parent index; -1 for toplevel!
    // bool referencesParent;

    Base(const string &name, ExprPtr ast = nullptr, int parent = -1)
        : name(name), ast(move(ast)), parent(parent) {}
    bool isType() const { return ast != nullptr; }
  };
  vector<Base> bases;
  vector<string> loops;
  vector<set<string>> captures;

public:
  TransformContext(const string &filename, shared_ptr<Cache> cache);
  virtual ~TransformContext();

  shared_ptr<TransformItem> find(const string &name) const;
  using Context<TransformItem>::add;
  shared_ptr<TransformItem> add(TransformItem::Kind kind, const string &name,
                                const string &canonicalName = "", bool global = false,
                                bool generic = false, bool stat = false);
  void dump(int pad = 0) override;

public:
  string getBase() const;
  int getLevel() const { return bases.size(); }

  string generateCanonicalName(const string &name);

public:
  shared_ptr<types::LinkType> addUnbound(const SrcInfo &srcInfo, int level,
                                         bool setActive = true, bool isStatic = false);
  /// Calls `type->instantiate`, but populates the instantiation table
  /// with "parent" type.
  /// Example: for list[T].foo, list[int].foo will populate type of foo so that
  /// the generic T gets mapped to int.
  types::TypePtr instantiate(const SrcInfo &srcInfo, types::TypePtr type);
  types::TypePtr instantiate(const SrcInfo &srcInfo, types::TypePtr type,
                             types::ClassTypePtr generics, bool activate = true);
  types::TypePtr instantiateGeneric(const SrcInfo &srcInfo, types::TypePtr root,
                                    const vector<types::TypePtr> &generics);
  string findFile(const string &what, const string &relativeTo,
                  bool forceStdlib = false) const;
};

} // namespace ast
} // namespace seq
