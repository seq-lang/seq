#pragma once

#include "parser/common.h"
#include "parser/context.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

namespace seq {
namespace ast {

typedef string Cache;

struct FuncType;
struct ClassType;

struct Type;
typedef shared_ptr<Type> TypePtr;

struct Type : public seq::SrcInfo, public std::enable_shared_from_this<Type> {
  virtual int unify(TypePtr t) = 0;
  virtual TypePtr generalize(int level) = 0;
  virtual TypePtr instantiate(int level, int &unboundCount,
                              std::unordered_map<int, TypePtr> &cache) = 0;
  virtual string str() const = 0;
  virtual bool isUnbound() const = 0;
  virtual bool hasUnbound() const = 0;
  virtual bool canRealize() const = 0;
  friend std::ostream &operator<<(std::ostream &out, const Type &c) {
    return out << c.str();
  }

  virtual FuncType *getFunction() = 0;
  virtual ClassType *getClass() = 0;
};

enum LinkKind { Unbound, Generic, Link };
struct LinkType : public Type {
  LinkKind kind;
  int id, level;
  TypePtr type;
  bool isTypeUnbound;

  LinkType(LinkKind kind, int id, int level = 0, TypePtr type = nullptr);
  LinkType(TypePtr type)
      : kind(Link), id(0), level(0), type(type), isTypeUnbound(false) {}
  virtual ~LinkType() {}
  string str() const override;
  int unify(TypePtr typ) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;
  bool occurs(Type *typ);
  bool isUnbound() const override;
  bool hasUnbound() const override;
  bool canRealize() const override;

  FuncType *getFunction() override;
  ClassType *getClass() override;
};

struct ClassType : public Type {
  string name;
  string canonicalName;
  vector<pair<int, TypePtr>> generics;

  ClassType(const string &name, const string &canonicalName,
            const vector<pair<int, TypePtr>> &generics);
  virtual ~ClassType() {}
  string str() const override;
  int unify(TypePtr typ) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;
  bool isUnbound() const override { return false; }
  bool hasUnbound() const override;
  bool canRealize() const override;

  FuncType *getFunction() override { return nullptr; }
  ClassType *getClass() override { return this; }
  string getCanonicalName() const { return canonicalName; }
};

struct FuncType : public Type {
  string name;
  string canonicalName;

  vector<pair<int, TypePtr>> generics;
  vector<pair<string, TypePtr>> args;
  TypePtr ret;

  FuncType(const string &name, const string &canonicalName,
           const vector<pair<int, TypePtr>> &generics,
           const vector<pair<string, TypePtr>> &args, TypePtr ret);
  virtual ~FuncType() {}
  string str() const override;
  int unify(TypePtr typ) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;
  bool isUnbound() const override { return false; }
  bool hasUnbound() const override;
  bool canRealize() const override;

  FuncType *getFunction() override { return this; }
  ClassType *getClass() override { return nullptr; }
  string getCanonicalName() const { return canonicalName; }
};

struct RecordType : public Type {
  string name;
  string canonicalName;
  vector<pair<string, TypePtr>> args;

  RecordType(const string &name, const string &canonicalName,
             const vector<pair<string, TypePtr>> &args);
  RecordType(const vector<Expr *> args);
  virtual ~RecordType() {}
  string str() const override;
  int unify(TypePtr typ) override;
  TypePtr generalize(int level) override;
  TypePtr instantiate(int level, int &unboundCount,
                      std::unordered_map<int, TypePtr> &cache) override;
  bool isUnbound() const override { return false; }
  bool hasUnbound() const override;
  bool canRealize() const override;

  FuncType *getFunction() override { return nullptr; }
  ClassType *getClass() override { return nullptr; }
  string getCanonicalName() const { return canonicalName; }
};

////////

} // namespace ast
} // namespace seq
