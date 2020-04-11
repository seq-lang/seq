#include <memory>
#include <string>
#include <vector>

#include "parser/ast/types.h"

using std::dynamic_pointer_cast;
using std::make_shared;
using std::min;
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

LinkType::LinkType(LinkKind kind, int id, int level, TypePtr type)
    : kind(kind), id(id), level(level), type(type), isTypeUnbound(false) {}

string LinkType::str() const {
  if (kind == Unbound)
    return fmt::format("?-{}-{}", id, level);
  else if (kind == Generic)
    return fmt::format("T-{}", id, level);
  else
    return "&" + type->str();
}

bool LinkType::occurs(Type *typ) {
  if (auto t = dynamic_cast<LinkType *>(typ)) {
    if (t->kind == Unbound && t->id == id) {
      return true;
    } else if (t->kind == Unbound) {
      // Link higher level unbound to lower level
      id = min(id, t->id);
      level = min(level, t->level);
    } else if (t->kind == Link) {
      return occurs(t->type.get());
    }
  } else if (auto t = dynamic_cast<ClassType *>(typ)) {
    for (auto &t : t->generics)
      if (occurs(t.second.get()))
        return true;
  } else if (auto t = dynamic_cast<FuncType *>(typ)) {
    if (occurs(t->ret.get()))
      return true;
    for (auto &t : t->generics)
      if (occurs(t.second.get()))
        return true;
    for (auto &t : t->args)
      if (occurs(t.second.get()))
        return true;
  } else if (auto t = dynamic_cast<RecordType *>(typ)) {
    for (auto &t : t->args)
      if (occurs(t.second.get()))
        return true;
  }
  return false;
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

bool LinkType::isUnbound() const { return kind == Unbound; }

bool LinkType::hasUnbound() const {
  if (isUnbound())
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

FuncType *LinkType::getFunction() {
  if (kind == Link)
    return type->getFunction();
  else
    return nullptr;
}

ClassType *LinkType::getClass() {
  if (kind == Link)
    return type->getClass();
  else
    return nullptr;
}

ClassType::ClassType(const string &name, const string &canonicalName,
                     const vector<pair<int, TypePtr>> &generics)
    : name(name), canonicalName(canonicalName), generics(generics) {}

string ClassType::str() const {
  vector<string> gs;
  for (auto &a : generics)
    gs.push_back(a.second->str());
  return fmt::format("{}{}", name,
                     gs.size() ? fmt::format("[{}]", fmt::join(gs, ",")) : "");
}

int ClassType::unify(TypePtr typ) {
  if (auto t = dynamic_cast<ClassType *>(typ.get())) {
    if (canonicalName != t->canonicalName)
      return -1;
    int s;
    if ((s = unifyList(generics, t->generics)) != -1) {
      return s;
    }
    return -1;
  } else if (auto t = dynamic_cast<LinkType *>(typ.get())) {
    return t->unify(shared_from_this());
  }
  return -1;
}

TypePtr ClassType::generalize(int level) {
  auto g = generics;
  for (auto &t : g)
    t.second = t.second->generalize(level);
  return make_shared<ClassType>(name, canonicalName, g);
}

TypePtr ClassType::instantiate(int level, int &unboundCount,
                               std::unordered_map<int, TypePtr> &cache) {
  auto g = generics;
  for (auto &t : g)
    t.second = t.second->instantiate(level, unboundCount, cache);
  return make_shared<ClassType>(name, canonicalName, g);
}

bool ClassType::hasUnbound() const {
  for (auto &t : generics)
    if (t.second->hasUnbound())
      return true;
  return false;
}

bool ClassType::canRealize() const {
  for (auto &t : generics)
    if (!t.second->canRealize())
      return false;
  return true;
}

FuncType::FuncType(const string &name, const string &canonicalName,
                   const vector<pair<int, TypePtr>> &generics,
                   const vector<pair<string, TypePtr>> &args, TypePtr ret)
    : name(name), canonicalName(canonicalName), generics(generics), args(args),
      ret(ret) {}

string FuncType::str() const {
  vector<string> gs, as;
  for (auto &a : generics)
    gs.push_back(a.second->str());
  for (auto &a : args)
    as.push_back(a.second->str());
  return fmt::format("{}{}({}{})", name,
                     gs.size() ? fmt::format("[{}]", fmt::join(gs, ",")) : "",
                     ret->str(),
                     as.size() ? fmt::format(", {}", fmt::join(as, ",")) : "");
}

int FuncType::unify(TypePtr typ) {
  if (auto t = dynamic_cast<FuncType *>(typ.get())) {
    if (canonicalName != t->canonicalName)
      return -1;
    int s1, s2, s3;
    if ((s1 = unifyList(generics, t->generics)) != -1) {
      if ((s2 = unifyList(args, t->args)) != -1) {
        if ((s3 = ret->unify(t->ret)) != -1)
          return s1 + s2 + s3;
      }
    }
    return -1;
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
  return make_shared<FuncType>(name, canonicalName, g, a,
                               ret->generalize(level));
}

TypePtr FuncType::instantiate(int level, int &unboundCount,
                              std::unordered_map<int, TypePtr> &cache) {
  auto g = generics;
  for (auto &t : g)
    t.second = t.second->instantiate(level, unboundCount, cache);
  auto a = args;
  for (auto &t : a)
    t.second = t.second->instantiate(level, unboundCount, cache);
  return make_shared<FuncType>(name, canonicalName, g, a,
                               ret->instantiate(level, unboundCount, cache));
}

bool FuncType::hasUnbound() const {
  for (auto &t : generics)
    if (t.second->hasUnbound())
      return true;
  for (auto &t : args)
    if (t.second->hasUnbound())
      return true;
  return ret->hasUnbound();
}

bool FuncType::canRealize() const {
  for (auto &t : generics)
    if (!t.second->canRealize())
      return false;
  for (auto &t : args)
    if (!t.second->canRealize())
      return false;
  return true;
}

RecordType::RecordType(const string &name, const string &canonicalName,
                       const vector<pair<string, TypePtr>> &args)
    : name(name), canonicalName(canonicalName), args(args) {}

string RecordType::str() const {
  vector<string> as;
  for (auto &a : args)
    as.push_back(a.second->str());
  return fmt::format("{}[{}]", name == "" ? "tuple" : name, fmt::join(as, ","));
}

int RecordType::unify(TypePtr typ) {
  if (auto t = dynamic_cast<RecordType *>(typ.get())) {
    if (!(canonicalName == "" || t->canonicalName == "") &&
        canonicalName != t->canonicalName)
      return -1;
    return unifyList(args, t->args);
  } else if (auto t = dynamic_cast<LinkType *>(typ.get())) {
    t->unify(shared_from_this());
  }
  return -1;
}

TypePtr RecordType::generalize(int level) {
  auto a = args;
  for (auto &t : a)
    t.second = t.second->generalize(level);
  return make_shared<RecordType>(name, canonicalName, a);
}

TypePtr RecordType::instantiate(int level, int &unboundCount,
                                std::unordered_map<int, TypePtr> &cache) {
  auto a = args;
  for (auto &t : a)
    t.second = t.second->instantiate(level, unboundCount, cache);
  return make_shared<RecordType>(name, canonicalName, a);
}

bool RecordType::hasUnbound() const {
  for (auto &t : args)
    if (t.second->hasUnbound())
      return true;
  return false;
}

bool RecordType::canRealize() const {
  for (auto &t : args)
    if (!t.second->canRealize())
      return false;
  return true;
}

} // namespace ast
} // namespace seq
