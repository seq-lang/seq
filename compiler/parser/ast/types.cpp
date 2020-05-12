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
namespace types {

int unifyList(const vector<Arg> &a, const vector<Arg> &b, Unification &us) {
  int score = 0, s;
  if (a.size() != b.size())
    return -1;
  for (int i = 0; i < a.size(); i++) {
    if ((s = a[i].type->unify(b[i].type, us)) != -1)
      score += s;
    else
      return -1;
  }
  return score;
}

int unifyList(const vector<Generics::Generic> &a,
              const vector<Generics::Generic> &b, Unification &us) {
  int score = 0, s;
  if (a.size() != b.size())
    return -1;
  for (int i = 0; i < a.size(); i++) {
    if (bool(a[i].type) ^ bool(b[i].type))
      return -1;
    if (!a[i].type) {
      if (a[i].value != b[i].value)
        return -1;
      continue;
    }
    if ((s = a[i].type->unify(b[i].type, us)) != -1)
      score += s;
    else
      return -1;
  }
  return score;
}

TypePtr Type::follow() { return shared_from_this(); }

LinkType::LinkType(Kind kind, int id, int level, TypePtr type)
    : kind(kind), id(id), level(level), type(type) {}

string LinkType::toString(bool reduced) const {
  if (kind == Unbound)
    return fmt::format("?{}", id, level);
  else if (kind == Generic)
    return fmt::format("#{}", id, level);
  else
    return /*"&" +*/ type->toString(reduced);
}

bool LinkType::occurs(TypePtr typ, Unification &us) {
  if (auto t = dynamic_pointer_cast<LinkType>(typ)) {
    if (t->kind == Unbound) {
      if (t->id == id)
        return true;
      if (t->level > level) {
        us.leveled.push_back({t, t->level});
        t->level = level;
      }
      return false;
    } else if (t->kind == Link)
      return occurs(t->type, us);
    else
      return false;
  } else if (auto t = dynamic_pointer_cast<ClassType>(typ)) {
    for (auto &g : t->generics.explicits)
      if (g.type)
        if (occurs(g.type, us))
          return true;
    for (auto &a : t->args)
      if (occurs(a.type, us))
        return true;
    return false;
  } else if (auto t = dynamic_pointer_cast<FuncType>(typ)) {
    if (occurs(t->ret, us))
      return true;
    for (auto &t : t->generics.implicits)
      if (t.type)
        if (occurs(t.type, us))
          return true;
    for (auto &t : t->generics.explicits)
      if (t.type)
        if (occurs(t.type, us))
          return true;
    for (auto &t : t->args)
      if (occurs(t.type, us))
        return true;
    return false;
  } else {
    return false;
  }
}

int LinkType::unify(TypePtr typ, Unification &us) {
  if (kind == Link) {
    return type->unify(typ, us);
  } else if (kind == Generic) {
    if (auto t = dynamic_pointer_cast<LinkType>(typ)) {
      if (t->kind == Generic && id == t->id)
        return 1;
    }
    return -1;
  } else { // if (kind == Unbound)
    if (auto t = dynamic_pointer_cast<LinkType>(typ)) {
      if (t->kind == Link)
        return t->type->unify(shared_from_this(), us);
      else if (t->kind == Generic)
        return -1;
      else if (t->kind == Unbound && id == t->id)
        return 1;
    }
    if (!occurs(typ, us)) {
      us.linked.push_back(
          std::static_pointer_cast<LinkType>(shared_from_this()));
      kind = Link;
      type = typ;
      return 0;
    } else {
      return -1;
    }
  }
  return -1;
}

void Unification::undo() {
  for (int i = linked.size() - 1; i >= 0; i--) {
    linked[i]->kind = LinkType::Unbound;
    linked[i]->type = nullptr;
  }
  for (int i = leveled.size() - 1; i >= 0; i--) {
    assert(leveled[i].first->kind == LinkType::Unbound);
    leveled[i].first->level = leveled[i].second;
  }
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
    if (cache.find(id) != cache.end())
      return cache[id];
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
                     const Generics &generics, const vector<Arg> &args)
    : name(name), isRecord(isRecord), generics(generics), args(args) {}

string ClassType::toString(bool reduced) const {
  vector<string> gs;
  for (auto &a : generics.explicits)
    gs.push_back(a.type ? a.type->toString(reduced)
                        : fmt::format("{}", a.value));
  if (isRecord && name == "tuple")
    for (auto &a : args)
      gs.push_back(a.type->toString(reduced));
  return fmt::format("{}{}", name,
                     gs.size() ? fmt::format("[{}]", fmt::join(gs, ",")) : "");
}

int ClassType::unify(TypePtr typ, Unification &us) {
  if (auto t = dynamic_pointer_cast<ClassType>(typ)) {
    if (isRecord != t->isRecord)
      return -1;
    if (name != t->name) {
      if (!isRecord || (name != "tuple" && t->name != "tuple"))
        return -1;
    }
    int s1 = unifyList(generics.explicits, t->generics.explicits, us);
    if (s1 == -1)
      return -1;
    int s2 = unifyList(generics.implicits, t->generics.implicits, us);
    if (s2 == -1)
      return -1;
    int s3 = unifyList(args, t->args, us);
    if (s3 == -1)
      return -1;
    return s1 + s2 + s3;
  } else if (auto t = dynamic_pointer_cast<LinkType>(typ)) {
    return t->unify(shared_from_this(), us);
  }
  return -1;
}

TypePtr ClassType::generalize(int level) {
  auto g = generics;
  for (auto &t : g.explicits)
    t.type = t.type ? t.type->generalize(level) : nullptr;
  for (auto &t : g.implicits)
    t.type = t.type ? t.type->generalize(level) : nullptr;
  auto a = args;
  for (auto &t : a)
    t.type = t.type->generalize(level);
  return make_shared<ClassType>(name, isRecord, g, a);
}

TypePtr ClassType::instantiate(int level, int &unboundCount,
                               std::unordered_map<int, TypePtr> &cache) {
  auto g = generics;
  for (auto &t : g.explicits)
    t.type = t.type ? t.type->instantiate(level, unboundCount, cache) : nullptr;
  for (auto &t : g.implicits)
    t.type = t.type ? t.type->instantiate(level, unboundCount, cache) : nullptr;
  auto a = args;
  for (auto &t : a)
    t.type = t.type->instantiate(level, unboundCount, cache);
  return make_shared<ClassType>(name, isRecord, g, a);
}

bool ClassType::hasUnbound() const {
  for (auto &t : generics.explicits)
    if (t.type && t.type->hasUnbound())
      return true;
  for (auto &t : generics.implicits)
    if (t.type && t.type->hasUnbound())
      return true;
  for (auto &t : args)
    if (t.type->hasUnbound())
      return true;
  return false;
}

bool ClassType::canRealize() const {
  for (auto &t : generics.explicits)
    if (t.type && !t.type->canRealize())
      return false;
  for (auto &t : generics.implicits)
    if (t.type && !t.type->canRealize())
      return false;
  for (auto &t : args)
    if (!t.type->canRealize())
      return false;
  return true;
}

FuncType::FuncType(const string &name, const Generics &generics,
                   const vector<Arg> &args, TypePtr ret)
    : name(name), generics(generics), args(args), ret(ret) {
  for (int i = 0; i < args.size(); i++)
    partialArgs.push_back(1);
  partialArgs.push_back(0);
}

string FuncType::toString(bool reduced) const {
  vector<string> gs, as;
  for (auto &a : generics.explicits)
    gs.push_back(a.type ? a.type->toString(reduced)
                        : fmt::format("{}", a.value));
  if (!reduced)
    as.push_back(ret->toString(reduced));
  for (int i = 0; i < args.size(); i++)
    if (partialArgs[i])
      as.push_back(args[i].type->toString(reduced));
  if (partialArgs.back())
    as.push_back("...");
  vector<string> is;
  if (reduced)
    for (auto &a : generics.implicits)
      is.push_back(a.type ? a.type->toString(reduced)
                          : fmt::format("{}", a.value));
  return fmt::format("{}[{}{}{}]", "function",
                     is.size() ? fmt::format("{};", fmt::join(is, ",")) : "",
                     gs.size() ? fmt::format("{};", fmt::join(gs, ",")) : "",
                     as.size() ? fmt::format("{}", fmt::join(as, ",")) : "");
}

int FuncType::unify(TypePtr typ, Unification &us) {
  if (auto t = dynamic_pointer_cast<FuncType>(typ)) {
    if (name == "" && t->name != "")
      return t->unify(shared_from_this(), us);
    if (t->name == "") {
      if (countPartials() != t->countPartials())
        return -1;
      int s = 0, i = 0, j = 0;
      while (i < countPartials()) {
        while (!partialArgs[i])
          i++;
        while (!t->partialArgs[j])
          j++;
        if ((i == args.size()) ^ (j == args.size()))
          return -1;
        if (i == args.size() && j == args.size())
          break;
        int u = args[i].type->unify(t->args[j].type, us);
        if (u == -1)
          return -1;
        s += u;
      }
      int u = ret->unify(t->ret, us);
      if (u == -1)
        return -1;
      return s + u;
    } else {
      if (name != t->name) {
        if (name != "" && t->name != "")
          return -1;
      }
      if (partialArgs.size() != t->partialArgs.size())
        return -1;
      for (int i = 0; i < partialArgs.size(); i++)
        if (partialArgs[i] != t->partialArgs[i])
          return -1;
      int s1 = unifyList(generics.explicits, t->generics.explicits, us);
      if (s1 == -1)
        return -1;
      int s2 = unifyList(args, t->args, us);
      if (s2 == -1)
        return -1;
      int s3 = ret->unify(t->ret, us);
      if (s3 == -1)
        return -1;
      int s4 = unifyList(generics.implicits, t->generics.implicits, us);
      if (s4 == -1)
        return -1;
      return s1 + s2 + s3 + s4;
    }
  } else if (auto t = dynamic_pointer_cast<LinkType>(typ)) {
    return t->unify(shared_from_this(), us);
  }
  return -1;
}

TypePtr FuncType::generalize(int level) {
  auto g = generics;
  for (auto &t : g.explicits)
    t.type = t.type ? t.type->generalize(level) : nullptr;
  for (auto &t : g.implicits)
    t.type = t.type ? t.type->generalize(level) : nullptr;
  auto a = args;
  for (auto &t : a)
    t.type = t.type->generalize(level);
  auto t = make_shared<FuncType>(name, g, a, ret->generalize(level));
  return t;
}

TypePtr FuncType::instantiate(int level, int &unboundCount,
                              std::unordered_map<int, TypePtr> &cache) {
  auto g = generics;
  for (auto &t : g.explicits)
    t.type = t.type ? t.type->instantiate(level, unboundCount, cache) : nullptr;
  for (auto &t : g.implicits)
    t.type = t.type ? t.type->instantiate(level, unboundCount, cache) : nullptr;
  auto a = args;
  for (auto &t : a)
    t.type = t.type->instantiate(level, unboundCount, cache);
  auto t = make_shared<FuncType>(name, g, a,
                                 ret->instantiate(level, unboundCount, cache));
  return t;
}

bool FuncType::hasUnbound() const {
  for (auto &t : generics.explicits)
    if (t.type && t.type->hasUnbound())
      return true;
  for (auto &t : args)
    if (t.type->hasUnbound())
      return true;
  for (auto &t : generics.implicits)
    if (t.type && t.type->hasUnbound())
      return true;
  return ret->hasUnbound();
}

bool FuncType::canRealize() const {
  if (name == "")
    return false;
  for (auto &t : generics.explicits)
    if (t.type && !t.type->canRealize())
      return false;
  for (auto &t : args)
    if (!t.type->canRealize())
      return false;
  for (auto &t : generics.implicits)
    if (t.type && !t.type->canRealize())
      return false;
  return true;
}

FuncTypePtr getFunction(TypePtr t) {
  return dynamic_pointer_cast<FuncType>(t->follow());
}

ClassTypePtr getClass(TypePtr t) {
  return dynamic_pointer_cast<ClassType>(t->follow());
}

LinkTypePtr getUnbound(TypePtr t) {
  auto tp = dynamic_pointer_cast<LinkType>(t->follow());
  if (!tp || tp->kind != LinkType::Unbound)
    tp = nullptr;
  return tp;
}

} // namespace types
} // namespace ast
} // namespace seq
