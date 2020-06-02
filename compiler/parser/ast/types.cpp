#include <memory>
#include <string>
#include <vector>

#include "parser/ast/types.h"

using std::dynamic_pointer_cast;
using std::make_shared;
using std::min;
using std::pair;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::unordered_map;
using std::vector;

namespace seq {
namespace ast {
namespace types {

TypePtr Type::follow() { return shared_from_this(); }

string StaticType::toString(bool reduced) const {
  return fmt::format("{}", value);
}

int StaticType::unify(TypePtr typ, Unification &us) {
  if (auto t = typ->getStatic())
    return (value == t->value) ? 0 : -1;
  else if (auto t = typ->getLink())
    return t->unify(shared_from_this(), us);
  return -1;
}

TypePtr StaticType::generalize(int level) {
  return make_shared<StaticType>(value);
}

TypePtr StaticType::instantiate(int level, int &unboundCount,
                                unordered_map<int, TypePtr> &cache) {
  return make_shared<StaticType>(value);
}

LinkType::LinkType(Kind kind, int id, int level, TypePtr type)
    : kind(kind), id(id), level(level), type(type) {}

string LinkType::toString(bool reduced) const {
  if (kind == Unbound)
    return fmt::format("?{}", id, level);
  else if (kind == Generic)
    return fmt::format("#{}", id, level);
  else
    return type->toString(reduced);
}

string LinkType::realizeString() const {
  if (kind == Unbound)
    return "?";
  assert(kind == Link);
  return type->realizeString();
}

bool LinkType::occurs(TypePtr typ, Unification &us) {
  if (auto t = typ->getLink()) {
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
  } else if (auto t = typ->getClass()) {
    for (auto &g : t->explicits)
      if (g.type)
        if (occurs(g.type, us))
          return true;
    for (auto &g : t->implicits)
      if (g.type)
        if (occurs(g.type, us))
          return true;
    return false;
  } else if (auto t = typ->getFunc()) {
    for (auto &t : t->explicits)
      if (t.type)
        if (occurs(t.type, us))
          return true;
    for (auto &t : t->implicits)
      if (t.type)
        if (occurs(t.type, us))
          return true;
    for (auto &t : t->args)
      if (occurs(t, us))
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
    if (auto t = typ->getLink()) {
      if (t->kind == Generic && id == t->id)
        return 1;
    }
    return -1;
  } else { // if (kind == Unbound)
    if (auto t = typ->getLink()) {
      if (t->kind == Link)
        return t->type->unify(shared_from_this(), us);
      else if (t->kind == Generic)
        return -1;
      else if (t->kind == Unbound && id == t->id)
        return 1;
    }
    if (!occurs(typ, us)) {
      // DBG("UNIFY: {} <- {}", id, *typ);
      us.linked.push_back(static_pointer_cast<LinkType>(shared_from_this()));
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
                              unordered_map<int, TypePtr> &cache) {
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

GenericType::GenericType(const vector<GenericType::Generic> &explicits,
                         const vector<GenericType::Generic> &implicits)
    : explicits(explicits), implicits(implicits) {}

string GenericType::toString(bool reduced) const {
  vector<string> gs, is;
  for (auto &a : explicits)
    gs.push_back(a.type->toString(reduced));
  if (reduced)
    for (auto &a : implicits)
      is.push_back(a.type->toString(reduced));
  return fmt::format("{}{}",
                     is.size() ? fmt::format("{};", fmt::join(is, ",")) : "",
                     gs.size() ? fmt::format("{}", fmt::join(gs, ",")) : "");
}

string GenericType::realizeString() const {
  vector<string> gs;
  for (auto &a : implicits)
    gs.push_back(a.type->realizeString());
  for (auto &a : explicits)
    gs.push_back(a.type->realizeString());
  return join(gs, ",");
}

bool GenericType::hasUnbound() const {
  for (auto &t : explicits)
    if (t.type && t.type->hasUnbound())
      return true;
  for (auto &t : implicits)
    if (t.type && t.type->hasUnbound())
      return true;
  return false;
}

bool GenericType::canRealize() const {
  for (auto &t : explicits)
    if (t.type && !t.type->canRealize())
      return false;
  for (auto &t : implicits)
    if (t.type && !t.type->canRealize())
      return false;
  return true;
}

TypePtr GenericType::generalize(int level) {
  auto e = explicits, i = implicits;
  for (auto &t : e)
    t.type = t.type ? t.type->generalize(level) : nullptr;
  for (auto &t : i)
    t.type = t.type ? t.type->generalize(level) : nullptr;
  auto c = make_shared<GenericType>(e, i);
  c->setSrcInfo(getSrcInfo());
  return c;
}

TypePtr GenericType::instantiate(int level, int &unboundCount,
                                 unordered_map<int, TypePtr> &cache) {
  auto e = explicits, i = implicits;
  for (auto &t : e)
    t.type = t.type ? t.type->instantiate(level, unboundCount, cache) : nullptr;
  for (auto &t : i)
    t.type = t.type ? t.type->instantiate(level, unboundCount, cache) : nullptr;
  auto c = make_shared<GenericType>(e, i);
  c->setSrcInfo(getSrcInfo());
  return c;
}

int GenericType::unify(TypePtr t_, Unification &us) {
  auto t = dynamic_pointer_cast<GenericType>(t_);
  assert(t);

  if (explicits.size() != t->explicits.size() ||
      implicits.size() != t->implicits.size())
    return -1;
  int s = 0, u;
  for (int i = 0; i < explicits.size(); i++) {
    if ((u = explicits[i].type->unify(t->explicits[i].type, us)) == -1)
      return -1;
    s += u;
  }
  for (int i = 0; i < implicits.size(); i++) {
    if ((u = implicits[i].type->unify(t->implicits[i].type, us)) == -1)
      return -1;
    s += u;
  }
  return s;
}

ClassType::ClassType(const string &name, bool isRecord,
                     const vector<TypePtr> &recordMembers,
                     shared_ptr<GenericType> generics)
    : name(name), record(isRecord), recordMembers(recordMembers) {
  if (generics) {
    explicits = generics->explicits;
    implicits = generics->implicits;
  }
}

string ClassType::toString(bool reduced) const {
  auto g = GenericType::toString(reduced);
  if (name == "tuple") {
    vector<string> as;
    for (int i = 0; i < recordMembers.size(); i++)
      as.push_back(recordMembers[i]->toString(reduced));
    g = join(as, ",");
  }
  return fmt::format("{}{}", name[0] == '#' ? name.substr(1) : name,
                     g.size() ? fmt::format("[{}]", g) : "");
}

string ClassType::realizeString() const {
  string s;
  if (name == "tuple") {
    vector<string> as;
    for (auto &r : recordMembers)
      as.push_back(r->realizeString());
    s = join(as, ",");
  } else {
    s = GenericType::realizeString();
  }
  return fmt::format("{}{}", name[0] == '#' ? name.substr(1) : name,
                     s.empty() ? "" : fmt::format("[{}]", s));
}

int ClassType::unify(TypePtr typ, Unification &us) {
  if (auto t = typ->getClass()) {
    if (isRecord() != t->isRecord())
      return -1;
    if (isRecord() && (name == "tuple" || t->name == "tuple")) {
      // When unifying tuples, only record members matter
      if (recordMembers.size() != t->recordMembers.size())
        return -1;
      int s1 = 0, s;
      for (int i = 0; i < recordMembers.size(); i++) {
        if ((s = recordMembers[i]->unify(t->recordMembers[i], us)) != -1)
          s1 += s;
        else
          return -1;
      }
      return s1;
    }
    if (name != t->name)
      return -1;
    return GenericType::unify(t, us);
  } else if (auto t = typ->getLink()) {
    return t->unify(shared_from_this(), us);
  }
  return -1;
}

TypePtr ClassType::generalize(int level) {
  auto a = recordMembers;
  for (auto &t : a)
    t = t->generalize(level);
  auto c = make_shared<ClassType>(
      name, record, a,
      static_pointer_cast<GenericType>(GenericType::generalize(level)));
  c->setSrcInfo(getSrcInfo());
  return c;
}

TypePtr ClassType::instantiate(int level, int &unboundCount,
                               unordered_map<int, TypePtr> &cache) {
  auto a = recordMembers;
  for (auto &t : a)
    t = t->instantiate(level, unboundCount, cache);
  auto c = make_shared<ClassType>(
      name, record, a,
      static_pointer_cast<GenericType>(
          GenericType::instantiate(level, unboundCount, cache)));
  c->setSrcInfo(getSrcInfo());
  return c;
}

bool ClassType::hasUnbound() const {
  for (int i = 1; i < recordMembers.size(); i++)
    if (recordMembers[i]->hasUnbound())
      return true;
  if (GenericType::hasUnbound())
    return true;
  return false;
}

bool ClassType::canRealize() const {
  for (int i = 0; i < recordMembers.size(); i++)
    if (!recordMembers[i]->canRealize())
      return false;
  return GenericType::canRealize();
}

FuncType::FuncType(const vector<TypePtr> &args,
                   shared_ptr<GenericType> generics)
    : realizationInfo(nullptr), args(args) {
  if (generics) {
    explicits = generics->explicits;
    implicits = generics->implicits;
  }
}

string FuncType::toString(bool reduced) const {
  auto g = GenericType::toString(reduced);
  vector<string> as;
  for (int i = reduced; i < args.size(); i++)
    as.push_back(args[i]->toString(reduced));
  return fmt::format("function[{}{}]", g.size() ? g + ";" : "",
                     fmt::join(as, ","));
}

string FuncType::realizeString() const {
  if (realizationInfo) {
    string s = GenericType::realizeString();
    vector<string> as;
    for (int i = 1; i < args.size(); i++)
      as.push_back(args[i]->realizeString());
    s = s.empty() ? join(as, ",") : s + ";" + join(as, ",");
    return fmt::format("{}[{}]", realizationInfo->name.substr(1), s);
  } else {
    vector<string> as;
    for (int i = 1; i < args.size(); i++)
      as.push_back(args[i]->realizeString());
    return fmt::format("function[{}]", join(as, ","));
  }
}

int FuncType::unify(TypePtr typ, Unification &us) {
  if (auto t = typ->getFunc()) {
    /// TODO: can it be unified to a vanilla function[?]?
    int s1 = GenericType::unify(t, us), s;
    if (s1 == -1)
      return -1;
    if (args.size() != t->args.size())
      return -1;
    for (int i = 0; i < args.size(); i++) {
      if ((s = args[i]->unify(t->args[i], us)) != -1)
        s1 += s;
      else
        return -1;
    }
    return s1;
  } else if (auto t = typ->getLink()) {
    return t->unify(shared_from_this(), us);
  }
  return -1;
}

TypePtr FuncType::generalize(int level) {
  auto a = args;
  for (auto &t : a)
    t = t->generalize(level);

  shared_ptr<RealizationInfo> p = nullptr;
  if (realizationInfo) {
    p = make_shared<RealizationInfo>(
        realizationInfo->name, realizationInfo->pending, realizationInfo->args,
        realizationInfo->baseClass);
    for (auto &a : p->args)
      a.type = a.type->generalize(level);
    if (p->baseClass)
      p->baseClass = p->baseClass->generalize(level);
  }
  auto f = make_shared<FuncType>(
      a, static_pointer_cast<GenericType>(GenericType::generalize(level)));
  f->realizationInfo = p;
  f->setSrcInfo(getSrcInfo());
  return f;
}

TypePtr FuncType::instantiate(int level, int &unboundCount,
                              unordered_map<int, TypePtr> &cache) {
  auto a = args;
  for (auto &t : a)
    t = t->instantiate(level, unboundCount, cache);

  shared_ptr<RealizationInfo> p = nullptr;
  if (realizationInfo) {
    p = make_shared<RealizationInfo>(
        realizationInfo->name, realizationInfo->pending, realizationInfo->args,
        realizationInfo->baseClass);
    for (auto &a : p->args)
      a.type = a.type->instantiate(level, unboundCount, cache);
    if (p->baseClass)
      p->baseClass = p->baseClass->instantiate(level, unboundCount, cache);
  }

  auto f = make_shared<FuncType>(
      a, static_pointer_cast<GenericType>(
             GenericType::instantiate(level, unboundCount, cache)));
  f->realizationInfo = p;
  f->setSrcInfo(getSrcInfo());
  return f;
}

bool FuncType::hasUnbound() const {
  if (GenericType::hasUnbound())
    return true;
  for (auto &t : args)
    if (t->hasUnbound())
      return true;
  return false;
}

bool FuncType::canRealize() const {
  if (!GenericType::canRealize())
    return false;
  for (int i = 1; i < args.size(); i++)
    if (!args[i]->canRealize())
      return false;
  if (realizationInfo) {
    for (int i = 0; i < realizationInfo->args.size(); i++)
      if (!realizationInfo->args[i].type->canRealize())
        return false;
    return true;
  } else
    return args[0]->canRealize();
}

} // namespace types
} // namespace ast
} // namespace seq
