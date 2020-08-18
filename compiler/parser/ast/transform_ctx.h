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
  // std::string id;
  std::string module, base;
  bool global;
  std::unordered_set<std::string> attributes;

public:
  Item(const std::string &module, const std::string &base, bool global = false)
      : module(module), base(base), global(global) {}
  virtual ~Item() {}

  virtual const Class *getClass() const { return nullptr; }
  virtual const Func *getFunc() const { return nullptr; }
  virtual const Var *getVar() const { return nullptr; }
  virtual const Import *getImport() const { return nullptr; }
  virtual const Static *getStatic() const { return nullptr; }
  virtual types::TypePtr getType() const { return nullptr; }

  // std::string getUniqueID() const { return id; }
  std::string getBase() const { return base; }
  std::string getModule() const { return module; }
  bool isGlobal() const { return global; }
  void setGlobal() { global = true; }
  bool hasAttr(const std::string &s) const {
    return attributes.find(s) != attributes.end();
  }
};

class Import : public Item {
  std::string file;

public:
  Import(const std::string &file, const std::string &module,
         const std::string &base, bool global = false)
      : Item(module, base, global), file(file) {}
  const Import *getImport() const override { return this; }
  std::string getFile() const { return file; }
};

class Static : public Item {
  types::TypePtr type;
  int value;
  // bool initialized;

public:
  Static(int value, types::TypePtr type, const std::string &module,
         const std::string &base, bool global = false)
      : Item(module, base, global), type(type), value(value) {}
  const Static *getStatic() const override { return this; }
  int getValue() const { return value; }
  types::TypePtr getType() const override { return type; }
  // bool isInitialized() const { return initialized; }
};

class Var : public Item {
  types::TypePtr type;

public:
  Var(types::TypePtr type, const std::string &module, const std::string &base,
      bool global = false)
      : Item(module, base, global), type(type) {}
  const Var *getVar() const override { return this; }
  types::TypePtr getType() const override { return type; }
};

class Class : public Item {
  types::TypePtr type;
  // bool isStatic;

public:
  Class(types::TypePtr type, const std::string &module,
        const std::string &base, /*bool isStatic = false,*/
        bool global = false)
      : Item(module, base, global), type(type) /*,isStatic(isStatic)*/ {}
  const Class *getClass() const override { return this; }
  types::TypePtr getType() const override { return type; }
  // bool getStatic() const { return isStatic; }
};

class Func : public Item {
  types::TypePtr type;

public:
  Func(types::TypePtr type, const std::string &module, const std::string &base,
       bool global = false)
      : Item(module, base, global), type(type) {}
  const Func *getFunc() const override { return this; }
  types::TypePtr getType() const override { return type; }
};
} // namespace TypeItem

class TypeContext : public Context<TypeItem::Item> {
  /** Naming **/

  /// Current module-specific name prefix (stack of enclosing class/function
  /// scopes). Module toplevel has no base.
  std::vector<std::string> bases;

  /** Type-checking **/
  /// Current type-checking level
  int level;
  /// Set of active unbound variables.
  /// If type checking is successful, all of them should be resolved.
  std::set<types::TypePtr> activeUnbounds;

  /** Function utilities **/
  /// Function parsing helpers: maintain current return type
  types::TypePtr returnType, matchType;
  std::vector<types::ClassTypePtr> baseTypes;
  /// Indicates if a return was seen (to account for procedures)
  bool wasReturnTypeSet;

  /// Used for fixing generic function definitions
  bool typecheck;

public:
  TypeContext(const std::string &filename,
              std::shared_ptr<RealizationContext> realizations,
              std::shared_ptr<ImportContext> imports);
  virtual ~TypeContext();

  std::shared_ptr<TypeItem::Item> find(const std::string &name,
                                       bool checkStdlib = true) const;
  types::TypePtr findInternal(const std::string &name) const;

  using Context<TypeItem::Item>::add;
  std::shared_ptr<TypeItem::Item>
  addVar(const std::string &name, types::TypePtr type, bool global = false);
  std::shared_ptr<TypeItem::Item> addImport(const std::string &name,
                                            const std::string &import,
                                            bool global = false);
  std::shared_ptr<TypeItem::Item>
  addType(const std::string &name, types::TypePtr type, bool global = false);
  std::shared_ptr<TypeItem::Item>
  addFunc(const std::string &name, types::TypePtr type, bool global = false);
  std::shared_ptr<TypeItem::Item> addStatic(const std::string &name, int value,
                                            types::TypePtr type = nullptr,
                                            bool global = false);
  void addGlobal(const std::string &name, types::TypePtr type);
  // void addRealization(types::TypePtr type);
  void dump(int pad = 0) override;

public:
  std::string getBase() const;
  // std::string getModule() const;
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
  types::ClassTypePtr getBaseType() const {
    return baseTypes.size() ? baseTypes.back() : nullptr;
  }
  void addBaseType(types::ClassTypePtr t) { baseTypes.push_back(t); }
  void popBaseType() { baseTypes.pop_back(); }
  bool wasReturnSet() const { return wasReturnTypeSet; }
  void setWasReturnSet(bool state) { wasReturnTypeSet = state; }
  void pushBase(const std::string &s) { bases.push_back(s); }
  void popBase() { bases.pop_back(); }
  bool isTypeChecking() const { return typecheck; }
  void setTypeCheck(bool s) { typecheck = s; }
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
                             types::ClassTypePtr generics,
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
