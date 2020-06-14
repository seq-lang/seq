#pragma once

#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/context.h"
#include "parser/common.h"

namespace seq {
namespace ast {

namespace TypeItem {

class Import;
class Static;
class Var;
class Func;
class Class;

class Item {
protected:
  std::string base;
  bool global;
  std::unordered_set<std::string> attributes;

public:
  Item(const std::string &base, bool global = false)
      : base(base), global(global) {}
  virtual ~Item() {}

  virtual const Class *getClass() const { return nullptr; }
  virtual const Func *getFunc() const { return nullptr; }
  virtual const Var *getVar() const { return nullptr; }
  virtual const Import *getImport() const { return nullptr; }
  virtual const Static *getStatic() const { return nullptr; }
  virtual types::TypePtr getType() const { return nullptr; }

  std::string getBase() const { return base; }
  bool isGlobal() const { return global; }
  void setGlobal() { global = true; }
  bool hasAttr(const std::string &s) const {
    return attributes.find(s) != attributes.end();
  }
};

class Import : public Item {
  std::string file;

public:
  Import(const std::string &file, const std::string &base, bool global = false)
      : Item(base, global), file(file) {}
  const Import *getImport() const override { return this; }
  std::string getFile() const { return file; }
};

class Static : public Item {
  int value;

public:
  Static(int value, const std::string &base, bool global = false)
      : Item(base, global), value(value) {}
  const Static *getStatic() const override { return this; }
  int getValue() const { return value; }
};

class Var : public Item {
  types::TypePtr type;

public:
  Var(types::TypePtr type, const std::string &base, bool global = false)
      : Item(base, global), type(type) {}
  const Var *getVar() const override { return this; }
  types::TypePtr getType() const override { return type; }
};

class Class : public Item {
  types::TypePtr type;

public:
  Class(types::TypePtr type, const std::string &base, bool global = false)
      : Item(base, global), type(type) {}
  const Class *getClass() const override { return this; }
  types::TypePtr getType() const override { return type; }
};

class Func : public Item {
  types::TypePtr type;

public:
  Func(types::TypePtr type, const std::string &base, bool global = false)
      : Item(base, global), type(type) {}
  const Func *getFunc() const override { return this; }
  types::TypePtr getType() const override { return type; }
};
} // namespace TypeItem

class TypeContext : public Context<TypeItem::Item> {
  /** Naming **/
  std::string module;

  /// Current name prefix (for functions within classes)
  std::vector<std::string> bases;

  /** Type-checking **/
  /// Current type-checking level
  int level;
  /// Set of active unbound variables.
  /// If type checking is successful, all of them should be resolved.
  std::set<types::TypePtr> activeUnbounds;

  /** Function utilities **/
  /// Function parsing helpers: maintain current return type
  types::TypePtr returnType, matchType, baseType;
  /// Indicates if a return was seen (to account for procedures)
  bool wasReturnTypeSet;

public:
  TypeContext(const std::string &filename,
              std::shared_ptr<RealizationContext> realizations,
              std::shared_ptr<ImportContext> imports);
  virtual ~TypeContext();

  std::shared_ptr<TypeItem::Item> find(const std::string &name,
                                       bool checkStdlib = true) const;
  types::TypePtr findInternal(const std::string &name) const;

  using Context<TypeItem::Item>::add;
  void addVar(const std::string &name, types::TypePtr type,
              bool global = false);
  void addImport(const std::string &name, const std::string &import,
                 bool global = false);
  void addType(const std::string &name, types::TypePtr type,
               bool global = false);
  void addFunc(const std::string &name, types::TypePtr type,
               bool global = false);
  void addStatic(const std::string &name, int value, bool global = false);
  void addRealization(types::TypePtr type);
  void dump(int pad = 0) override;

public:
  std::string getBase() const;
  std::string getModule() const;
  void increaseLevel();
  void decreaseLevel();
  int getLevel() const { return level; }
  types::TypePtr getReturnType() const { return returnType; }
  void setReturnType(types::TypePtr t = nullptr) {
    wasReturnTypeSet = true;
    if (t)
      returnType = t;
  }
  types::TypePtr getMatchType() const { return matchType; }
  void setMatchType(types::TypePtr t) { matchType = t; }
  types::TypePtr getBaseType() const { return baseType; }
  void setBaseType(types::TypePtr t) { baseType = t; }
  bool wasReturnSet() const { return wasReturnTypeSet; }
  void setWasReturnSet(bool state) { wasReturnTypeSet = state; }
  void pushBase(const std::string &s) { bases.push_back(s); }
  void popBase() { bases.pop_back(); }
  std::set<types::TypePtr> &getActiveUnbounds() { return activeUnbounds; }

public:
  std::shared_ptr<types::LinkType> addUnbound(const SrcInfo &srcInfo,
                                              bool setActive = true);
  /// Calls `type->instantiate`, but populates the instantiation table
  /// with "parent" type.
  /// Example: for list[T].foo, list[int].foo will populate type of foo so that
  /// the generic T gets mapped to int.
  types::TypePtr instantiate(const SrcInfo &srcInfo, types::TypePtr type);
  types::TypePtr instantiate(const SrcInfo &srcInfo, types::TypePtr type,
                             types::GenericTypePtr generics,
                             bool activate = true);
  types::TypePtr
  instantiateGeneric(const SrcInfo &srcInfo, types::TypePtr root,
                     const std::vector<types::TypePtr> &generics);
  ImportContext::Import importFile(const std::string &file);

public:
  static std::shared_ptr<TypeContext> getContext(const std::string &argv0,
                                                 const std::string &file);
};

} // namespace ast
} // namespace seq
