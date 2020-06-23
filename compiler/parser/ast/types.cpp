#include <memory>
#include <string>
#include <vector>

#include "parser/ast/ast.h"
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
      if (g.type && occurs(g.type, us))
        return true;
    for (auto p = t->parent; p; p = p->parent)
      for (auto &g : p->explicits)
        if (g.type && occurs(g.type, us))
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
#ifdef TYPE_DEBUG
      DBG("UNIFY: {} <- {}", id, typ->toString());
#endif
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

bool isTuple(const string &name) {
  return chop(name).substr(0, 8) == "__tuple_";
}

bool isCallable(const string &name) {
  return chop(name).substr(0, 11) == "__function_" ||
         chop(name).substr(0, 10) == "__partial_";
}

bool isFunc(const string &name) {
  return chop(name).substr(0, 11) == "__function_";
}

ClassType::ClassType(const string &name, bool isRecord,
                     const vector<TypePtr> &args,
                     const vector<Generic> &explicits, ClassTypePtr parent)
    : explicits(explicits), parent(parent), name(name), record(isRecord),
      args(args) {}

string ClassType::toString(bool reduced) const {
  vector<string> gs;
  for (auto &a : explicits)
    if (!a.name.empty())
      gs.push_back(a.type->toString(reduced));
  auto g = join(gs, ",");
  if (isCallable(name)) { // special case as functions have generics as well
    vector<string> as;
    for (int i = 0; i < args.size(); i++)
      as.push_back(args[i]->toString(reduced));
    g += (g.size() && as.size() ? ";" : "") + join(as, ",");
  }
  return fmt::format("{}{}{}",
                     reduced && parent ? parent->toString(reduced) + ":" : "",
                     chop(name), g.size() ? fmt::format("[{}]", g) : "");
}

string ClassType::realizeString(const string &className) const {
  vector<string> gs;
  for (auto &a : explicits)
    if (!a.name.empty())
      gs.push_back(a.type->realizeString());
  string s = join(gs, ",");
  if (isCallable(name)) { // special case as functions have generics as well
    vector<string> as;
    for (int i = 1; i < args.size(); i++) // return type is elided!
      as.push_back(args[i]->realizeString());
    s += (s.size() && as.size() ? ";" : "") + join(as, ",");
  }
  return fmt::format("{}{}{}", parent ? parent->realizeString() + ":" : "",
                     chop(className), s.empty() ? "" : fmt::format("[{}]", s));
}

string ClassType::realizeString() const { return realizeString(name); }

int ClassType::unify(TypePtr typ, Unification &us) {
  if (auto t = typ->getClass()) {
    if (isRecord() != t->isRecord())
      return -1;

    if (args.size() != t->args.size())
      return -1;
    int s1 = 0, s;
    for (int i = 0; i < args.size(); i++) {
      if ((s = args[i]->unify(t->args[i], us)) != -1)
        s1 += s;
      else
        return -1;
    }

    // When unifying records, only record members matter
    if (isRecord()) {
      if (isTuple(name) || isTuple(t->name))
        return s1;
      if (isFunc(name) && isFunc(t->name))
        return s1;
      // if ((isCallable(t->name) && chop(name).substr(0, 11) == "__callable_")
      // ||
      //     (isCallable(name) && chop(t->name).substr(0, 11) == "__callable_"))
      //     {
      //   // TODO: merge function types!
      //   // just check arguments!
      //   return s1;
      // }
      if (name != t->name)
        return -1;
    }
    if (bool(parent) ^ bool(t->parent))
      return -1;

    s = 0;
    for (int i = 0; i < explicits.size(); i++) {
      if ((s = explicits[i].type->unify(t->explicits[i].type, us)) == -1)
        return -1;
      s1 += s;
    }
    if (parent) {
      if ((s = parent->unify(t->parent, us)) == -1)
        return -1;
      s1 += s;
    }
    return s1;
  } else if (auto t = typ->getLink()) {
    return t->unify(shared_from_this(), us);
  }
  return -1;
}

TypePtr ClassType::generalize(int level) {
  auto a = args;
  for (auto &t : a)
    t = t->generalize(level);

  auto e = explicits;
  for (auto &t : e)
    t.type = t.type ? t.type->generalize(level) : nullptr;
  auto p = parent ? static_pointer_cast<ClassType>(parent->generalize(level))
                  : nullptr;
  auto c = make_shared<ClassType>(name, record, a, e, p);
  c->setSrcInfo(getSrcInfo());
  return c;
}

TypePtr ClassType::instantiate(int level, int &unboundCount,
                               unordered_map<int, TypePtr> &cache) {
  auto a = args;
  for (auto &t : a)
    t = t->instantiate(level, unboundCount, cache);
  auto e = explicits;
  for (auto &t : e)
    t.type = t.type ? t.type->instantiate(level, unboundCount, cache) : nullptr;
  auto p = parent ? static_pointer_cast<ClassType>(
                        parent->instantiate(level, unboundCount, cache))
                  : nullptr;
  auto c = make_shared<ClassType>(name, record, a, e, p);
  c->setSrcInfo(getSrcInfo());
  return c;
}

bool ClassType::hasUnbound() const {
  for (int i = isCallable(name); i < args.size(); i++)
    if (args[i]->hasUnbound())
      return true;
  for (auto &t : explicits)
    if (t.type && t.type->hasUnbound())
      return true;
  for (auto p = parent; p; p = p->parent)
    for (auto &g : p->explicits)
      if (g.type && g.type->hasUnbound())
        return true;
  return false;
}

bool ClassType::canRealize() const {
  if (chop(name).substr(0, 11) == "__callable_") // traits cannot be realized
    return false;
  for (int i = isCallable(name); i < args.size(); i++)
    if (!args[i]->canRealize())
      return false;
  for (auto &t : explicits)
    if (t.type && !t.type->canRealize())
      return false;
  for (auto p = parent; p; p = p->parent)
    for (auto &g : p->explicits)
      if (g.type && !g.type->canRealize())
        return false;
  return true;
}

ClassTypePtr ClassType::getCallable() {
  if (isCallable(name))
    return std::static_pointer_cast<ClassType>(shared_from_this());
  return nullptr;
}

FuncType::Arg FuncType::Arg::clone() const {
  return {name, defaultValue ? defaultValue->clone() : nullptr};
}

FuncType::FuncType(const std::vector<TypePtr> &args,
                   const vector<Generic> &explicits, ClassTypePtr parent,
                   const string &canonicalName, const vector<FuncType::Arg> &ad)
    : ClassType(fmt::format("__function_{}", args.size()), true, args,
                explicits, parent),
      canonicalName(canonicalName), argDefs(CL(ad)) {}

FuncType::FuncType(ClassTypePtr c, const string &canonicalName,
                   const vector<FuncType::Arg> &ad)
    : ClassType(fmt::format("__function_{}", c->args.size()), c->record,
                c->args, c->explicits, c->parent),
      canonicalName(canonicalName), argDefs(CL(ad)) {}

string FuncType::realizeString() const {
  return ClassType::realizeString(canonicalName);
}

TypePtr FuncType::generalize(int level) {
  return make_shared<FuncType>(
      static_pointer_cast<ClassType>(ClassType::generalize(level)),
      canonicalName, argDefs);
}

TypePtr FuncType::instantiate(int level, int &unboundCount,
                              std::unordered_map<int, TypePtr> &cache) {
  return make_shared<FuncType>(
      static_pointer_cast<ClassType>(
          ClassType::instantiate(level, unboundCount, cache)),
      canonicalName, argDefs);
}

PartialType::PartialType(ClassTypePtr c, const vector<int> &pending)
    : ClassType(fmt::format("__partial_{}", pending.size()), c->isRecord(),
                c->args, c->explicits, c->parent),
      pending(pending) {}

TypePtr PartialType::generalize(int level) {
  return make_shared<PartialType>(
      static_pointer_cast<ClassType>(ClassType::generalize(level)), pending);
}

TypePtr PartialType::instantiate(int level, int &unboundCount,
                                 std::unordered_map<int, TypePtr> &cache) {
  return make_shared<PartialType>(
      static_pointer_cast<ClassType>(
          ClassType::instantiate(level, unboundCount, cache)),
      pending);
}

// TODO:
// else {
//   return args[0]->canRealize();
// }

// FuncType::RealizationInfo::RealizationInfo(const string &name,
//                                            const vector<int> &pending,
//                                            const vector<Arg> &args,
//                                            TypePtr base)
//     : name(name), pending(pending), baseClass(base) {
//   for (auto &a : args)
//     this->args.push_back(
//         {a.name, a.type, a.defaultValue ? a.defaultValue->clone() :
//         nullptr});
// }

} // namespace types
} // namespace ast
} // namespace seq
