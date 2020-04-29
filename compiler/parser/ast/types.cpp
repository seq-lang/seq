#include <memory>
#include <string>
#include <vector>

#include "parser/ast/types.h"

using std::dynamic_pointer_cast;
using std::make_shared;
using std::min;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

namespace seq {
namespace ast {

template <typename T>
int unifyList(const vector<pair<T, TypePtr>> &a,
              const vector<pair<T, TypePtr>> &b) {
  int score = 0, s;
  if (a.size() != b.size()) {
    return -1;
  }
  for (int i = 0; i < a.size(); i++) {
    if ((s = a[i].second->unify(b[i].second)) != -1) {
      score += s;
    } else {
      return -1;
    }
  }
  return score;
}

TypePtr Type::follow() { return shared_from_this(); }

LinkType::LinkType(LinkKind kind, int id, int level, TypePtr type)
    : kind(kind), id(id), level(level), type(type) {}

string LinkType::toString(bool reduced) const {
  if (kind == Unbound)
    return fmt::format("?{}", id, level);
  else if (kind == Generic)
    return fmt::format("#{}", id, level);
  else
    return /*"&" +*/ type->toString(reduced);
}

bool LinkType::occurs(Type *typ) {
  if (auto t = dynamic_cast<LinkType *>(typ)) {
    if (t->kind == Unbound) {
      if (t->id == id)
        return true;
      if (t->level > level)
        t->level = level;
      return false;
    } else if (t->kind == Link) {
      return occurs(t->type.get());
    } else {
      return false;
    }
  } else if (auto t = dynamic_cast<ClassType *>(typ)) {
    for (auto &t : t->generics)
      if (occurs(t.second.get()))
        return true;
    for (auto &t : t->args)
      if (occurs(t.second.get()))
        return true;
    return false;
  } else if (auto t = dynamic_cast<FuncType *>(typ)) {
    if (occurs(t->ret.get()))
      return true;
    for (auto &t : t->implicitGenerics)
      if (occurs(t.second.get()))
        return true;
    for (auto &t : t->generics)
      if (occurs(t.second.get()))
        return true;
    for (auto &t : t->args)
      if (occurs(t.second.get()))
        return true;
    return false;
  } else {
    return false;
  }
}

int LinkType::unify(TypePtr typ) {
  if (kind == Link) {
    return type->unify(typ);
  } else if (kind == Generic) {
    if (auto t = dynamic_pointer_cast<LinkType>(typ)) {
      if (t->kind == Generic && id == t->id)
        return 1;
    }
    return -1;
  } else { // if (kind == Unbound)
    if (auto t = dynamic_pointer_cast<LinkType>(typ)) {
      if (t->kind == Link)
        return t->type->unify(shared_from_this());
      else if (t->kind == Generic)
        return -1;
      else if (t->kind == Unbound && id == t->id)
        return 1;
    }
    if (!occurs(typ.get())) {
      DBG("UNIFIED:  {} := {}", *this, *typ);
      kind = Link;
      type = typ;
      return 0;
    } else {
      return -1;
    }
  }
  return -1;
}

TypePtr LinkType::generalize(int level) {
  if (kind == Generic) {
    return shared_from_this();
  } else if (kind == Unbound) {
    if (this->level >= level)
      return make_shared<LinkType>(Generic, id);
    else
      return shared_from_this();
  } else { // (kind == Link) {
    assert(type);
    return type->generalize(level);
  }
}

TypePtr LinkType::instantiate(int level, int &unboundCount,
                              std::unordered_map<int, TypePtr> &cache) {
  if (kind == Generic) {
    if (cache.find(id) != cache.end()) {
      return cache[id];
    }
    return cache[id] = make_shared<LinkType>(Unbound, unboundCount++, level);
  } else if (kind == Unbound) {
    return shared_from_this();
  } else { // if (kind == Link) {
    assert(type);
    return type->instantiate(level, unboundCount, cache);
  }
}

TypePtr LinkType::follow() {
  if (kind == Link)
    return type->follow();
  else
    return shared_from_this();
}

bool LinkType::hasUnbound() const {
  if (kind == Unbound)
    return true;
  else if (kind == Link)
    return type->hasUnbound();
  return false;
}

bool LinkType::canRealize() const {
  if (kind != Link)
    return false;
  else
    return type->canRealize();
}

ClassType::ClassType(const string &name, bool isRecord,
                     const vector<pair<int, TypePtr>> &generics,
                     const vector<pair<string, TypePtr>> &args)
    : name(name), isRecord(isRecord), generics(generics), args(args) {}

string ClassType::toString(bool reduced) const {
  vector<string> gs;
  for (auto &a : generics)
    gs.push_back(a.second->toString(reduced));
  if (isRecord && name == "tuple")
    for (auto &a : args)
      gs.push_back(a.second->toString(reduced));
  return fmt::format("{}{}", name,
                     gs.size() ? fmt::format("[{}]", fmt::join(gs, ",")) : "");
}

int ClassType::unify(TypePtr typ) {
  if (auto t = dynamic_cast<ClassType *>(typ.get())) {
    if (isRecord != t->isRecord)
      return -1;
    if (name != t->name) {
      if (!isRecord || (name != "tuple" && t->name != "tuple"))
        return -1;
    }
    int s1 = unifyList(generics, t->generics);
    if (s1 == -1)
      return -1;
    int s2 = unifyList(args, t->args);
    if (s2 == -1)
      return -1;
    return s1 + s2;
  } else if (auto t = dynamic_cast<LinkType *>(typ.get())) {
    return t->unify(shared_from_this());
  }
  return -1;
}

TypePtr ClassType::generalize(int level) {
  auto g = generics;
  for (auto &t : g)
    t.second = t.second->generalize(level);
  auto a = args;
  for (auto &t : a)
    t.second = t.second->generalize(level);
  return make_shared<ClassType>(name, isRecord, g, a);
}

TypePtr ClassType::instantiate(int level, int &unboundCount,
                               std::unordered_map<int, TypePtr> &cache) {
  auto g = generics;
  for (auto &t : g)
    t.second = t.second->instantiate(level, unboundCount, cache);
  auto a = args;
  for (auto &t : a)
    t.second = t.second->instantiate(level, unboundCount, cache);
  return make_shared<ClassType>(name, isRecord, g, a);
}

bool ClassType::hasUnbound() const {
  for (auto &t : generics)
    if (t.second->hasUnbound())
      return true;
  for (auto &t : args)
    if (t.second->hasUnbound())
      return true;
  return false;
}

bool ClassType::canRealize() const {
  for (auto &t : generics)
    if (!t.second->canRealize())
      return false;
  for (auto &t : args)
    if (!t.second->canRealize())
      return false;
  return true;
}

FuncType::FuncType(const string &name,
                   const vector<pair<int, TypePtr>> &generics,
                   const vector<pair<string, TypePtr>> &args, TypePtr ret)
    : name(name), generics(generics), args(args), ret(ret) {}

string FuncType::toString(bool reduced) const {
  vector<string> gs, as;
  for (auto &a : generics)
    gs.push_back(a.second->toString(reduced));
  if (!reduced)
    as.push_back(ret->toString(reduced));
  for (auto &a : args)
    as.push_back(a.second->toString(reduced));
  vector<string> is;
  if (reduced)
    for (auto &a : implicitGenerics)
      is.push_back(a.second->toString(reduced));
  return fmt::format("function[{}{}{}]",
                     is.size() ? fmt::format("{};", fmt::join(is, ",")) : "",
                     gs.size() ? fmt::format("{};", fmt::join(gs, ",")) : "",
                     as.size() ? fmt::format("{}", fmt::join(as, ",")) : "");
}

int FuncType::unify(TypePtr typ) {
  if (auto t = dynamic_cast<FuncType *>(typ.get())) {
    if (name != t->name) {
      if (name != "" && t->name != "")
        return -1;
    }
    int s1 = unifyList(generics, t->generics);
    if (s1 == -1)
      return -1;
    int s2 = unifyList(args, t->args);
    if (s2 == -1)
      return -1;
    int s3 = ret->unify(t->ret);
    if (s3 == -1)
      return -1;
    int s4 = unifyList(implicitGenerics, t->implicitGenerics);
    if (s4 == -1)
      return -1;
    return s1 + s2 + s3 + s4;
  } else if (auto t = dynamic_cast<LinkType *>(typ.get())) {
    return t->unify(shared_from_this());
  }
  return -1;
}

TypePtr FuncType::generalize(int level) {
  auto g = generics;
  for (auto &t : g)
    t.second = t.second->generalize(level);
  auto a = args;
  for (auto &t : a)
    t.second = t.second->generalize(level);
  auto i = implicitGenerics;
  for (auto &t : i)
    t.second = t.second->generalize(level);
  auto t = make_shared<FuncType>(name, g, a, ret->generalize(level));
  t->setImplicits(i);
  return t;
}

TypePtr FuncType::instantiate(int level, int &unboundCount,
                              std::unordered_map<int, TypePtr> &cache) {
  auto g = generics;
  for (auto &t : g)
    t.second = t.second->instantiate(level, unboundCount, cache);
  auto a = args;
  for (auto &t : a)
    t.second = t.second->instantiate(level, unboundCount, cache);
  auto i = implicitGenerics;
  for (auto &t : i)
    t.second = t.second->instantiate(level, unboundCount, cache);
  auto t = make_shared<FuncType>(name, g, a,
                                 ret->instantiate(level, unboundCount, cache));
  t->setImplicits(i);
  return t;
}

bool FuncType::hasUnbound() const {
  for (auto &t : generics)
    if (t.second->hasUnbound())
      return true;
  for (auto &t : args)
    if (t.second->hasUnbound())
      return true;
  for (auto &t : implicitGenerics)
    if (t.second->hasUnbound())
      return true;
  return ret->hasUnbound();
}

bool FuncType::canRealize() const {
  if (name == "")
    return false;
  for (auto &t : generics)
    if (!t.second->canRealize())
      return false;
  for (auto &t : args)
    if (!t.second->canRealize())
      return false;
  for (auto &t : implicitGenerics)
    if (!t.second->canRealize())
      return false;
  return true;
}

} // namespace ast
} // namespace seq
