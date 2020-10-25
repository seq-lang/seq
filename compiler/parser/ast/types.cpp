#include <memory>
#include <string>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/ast/typecheck/typecheck.h"

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

StaticType::StaticType(const std::vector<Generic> &ex, std::unique_ptr<Expr> &&e)
    : explicits(ex), expr(move(e)) {}

StaticType::StaticType(int i) { expr = std::make_unique<IntExpr>(i); }

string StaticType::toString(bool reduced) const {
  vector<string> s;
  for (auto &e : explicits)
    s.push_back(fmt::format("{}={}", e.name, e.type->toString()));
  return fmt::format("Static[{}; {}]", join(s, ", "), expr->toString());
}

string StaticType::realizeString() const {
  assert(canRealize());
  vector<string> deps;
  for (auto &e : explicits)
    deps.push_back(e.type->realizeString());
  return fmt::format("{}{}", deps.size() ? join(deps, ";") : "", getValue());
}

int StaticType::unify(TypePtr typ, Unification &us) {
  if (auto t = typ->getStatic()) {
    // A + 5 + 3; 3 + A + 5
    int s1 = 0;
    if (expr->toString() == t->expr->toString()) {
      int s = 0;
      for (int i = 0; i < explicits.size(); i++) {
        if ((s = explicits[i].type->unify(t->explicits[i].type, us)) == -1)
          return -1;
        s1 += s;
      }
      return s1;
    }
    return -1;
  } else if (auto t = typ->getLink()) {
    return t->unify(shared_from_this(), us);
  }
  return -1;
}

TypePtr StaticType::generalize(int level) {
  auto e = explicits;
  for (auto &t : e)
    t.type = t.type ? t.type->generalize(level) : nullptr;
  auto c = make_shared<StaticType>(e, expr->clone());
  c->setSrcInfo(getSrcInfo());
  return c;
}

TypePtr StaticType::instantiate(int level, int &unboundCount,
                                unordered_map<int, TypePtr> &cache) {
  auto e = explicits;
  for (auto &t : e)
    t.type = t.type ? t.type->instantiate(level, unboundCount, cache) : nullptr;
  auto c = make_shared<StaticType>(e, expr->clone());
  c->setSrcInfo(getSrcInfo());
  return c;
}

bool StaticType::hasUnbound() const {
  for (auto &t : explicits)
    if (t.type && t.type->hasUnbound())
      return true;
  return false;
}

bool StaticType::canRealize() const {
  for (auto &t : explicits)
    if (t.type && !t.type->canRealize())
      return false;
  return true;
}

int StaticType::getValue() const {
  std::map<string, Generic> m;
  for (auto &e : explicits)
    m[e.name] = e;
  auto t = StaticVisitor(m).transform(expr);
  assert(t.first);
  return t.second;
}

LinkType::LinkType(Kind kind, int id, int level, TypePtr type, bool isStatic)
    : kind(kind), id(id), level(level), type(type), isStatic(isStatic) {}

string LinkType::toString(bool reduced) const {
  if (kind == Unbound)
    return fmt::format("?{}.{}", id, level, isStatic ? "_s" : "");
  else if (kind == Generic)
    return fmt::format("#{}.{}", id, level, isStatic ? "_s" : "");
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
    } else if (t->kind == Link) {
      return occurs(t->type, us);
    } else {
      return false;
    }
  } else if (auto t = typ->getStatic()) {
    for (auto &g : t->explicits)
      if (g.type && occurs(g.type, us))
        return true;
    return false;
  } else if (auto t = typ->getClass()) {
    for (auto &g : t->explicits)
      if (g.type && occurs(g.type, us))
        return true;
    if (t->parent && occurs(t->parent, us))
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
    auto t = typ->getLink();
    if (t && t->kind == Generic && id == t->id && isStatic == t->isStatic)
      return 1;
    return -1;
  } else { // if (kind == Unbound)
    if (auto t = typ->getLink()) {
      if (t->kind == Link)
        return t->type->unify(shared_from_this(), us);
      else if (t->kind == Generic)
        return -1;
      else if (isStatic != t->isStatic) // t->kind == Unbound
        return -1;
      else if (id == t->id)
        return 1;
    }
    if (!occurs(typ, us)) {
      assert(!type);
      LOG9("[unify] {} <- {}", id, typ->toString());
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
      return make_shared<LinkType>(Generic, id, 0, nullptr, isStatic);
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
    LOG9("[inst] #{} -> ?{}", id, unboundCount);
    return cache[id] =
               make_shared<LinkType>(Unbound, unboundCount++, level, nullptr, isStatic);
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

bool isTuple(const string &name) { return startswith(name, ".Tuple."); }

bool isCallable(const string &name) {
  return startswith(name, ".Function.") || startswith(name, ".Partial.");
}

bool isFunc(const string &name) { return startswith(name, ".Function."); }

ClassType::ClassType(const string &name, bool isRecord, const vector<TypePtr> &args,
                     const vector<Generic> &explicits, TypePtr parent)
    : explicits(explicits), parent(parent), isTrait(false), name(name),
      record(isRecord), args(args) {}

string ClassType::toString(bool reduced) const {
  vector<string> gs;
  for (auto &a : explicits)
    if (!a.name.empty())
      gs.push_back(a.type->toString(reduced));
  auto g = join(gs, ",");
  return fmt::format("{}{}{}", reduced && parent ? parent->toString(reduced) + ":" : "",
                     name, g.size() ? fmt::format("[{}]", g) : "");
}

string ClassType::realizeString() const {
  vector<string> gs;
  for (auto &a : explicits)
    if (!a.name.empty())
      gs.push_back(a.type->realizeString());
  string s = join(gs, ",");
  return fmt::format("{}{}{}", parent ? parent->realizeString() + ":" : "", name,
                     s.empty() ? "" : fmt::format("[{}]", s));
}

int ClassType::unify(TypePtr typ, Unification &us) {
  if (auto t = typ->getClass()) {
    if (isRecord() != t->isRecord())
      return -1;

    if (name == ".int" && t->name == ".Int")
      return t->explicits[0].type->unify(make_shared<StaticType>(64), us);
    if (t->name == ".int" && name == ".Int")
      return explicits[0].type->unify(make_shared<StaticType>(64), us);

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
      if (name != t->name)
        return -1;
    }
    if (bool(parent) ^ bool(t->parent))
      return -1;

    s = 0;
    if (explicits.size() != t->explicits.size())
      return -1;
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
  auto p = parent ? parent->generalize(level) : nullptr;
  auto c = make_shared<ClassType>(name, record, a, e, p);
  c->isTrait = isTrait;
  c->setSrcInfo(getSrcInfo());
  return c;
}

TypePtr ClassType::instantiate(int level, int &unboundCount,
                               unordered_map<int, TypePtr> &cache) {
  auto e = explicits;
  for (auto &t : e)
    if (t.type) {
      t.type = t.type->instantiate(level, unboundCount, cache);
      if (cache.find(t.id) == cache.end())
        cache[t.id] = t.type;
    }
  auto a = args;
  for (auto &t : a)
    t = t->instantiate(level, unboundCount, cache);
  auto p = parent ? parent->instantiate(level, unboundCount, cache) : nullptr;
  auto c = make_shared<ClassType>(name, record, a, e, p);
  c->isTrait = isTrait;
  c->setSrcInfo(getSrcInfo());
  return c;
}

bool ClassType::hasUnbound() const {
  for (int i = 0; i < args.size(); i++)
    if (args[i]->hasUnbound())
      return true;
  for (auto &t : explicits)
    if (t.type && t.type->hasUnbound())
      return true;
  if (parent && parent->hasUnbound())
    return true;
  return false;
}

bool ClassType::canRealize() const {
  for (int i = 0; i < args.size(); i++)
    if (!args[i]->canRealize())
      return false;
  for (auto &t : explicits)
    if (t.type && !t.type->canRealize())
      return false;
  if (parent && !parent->canRealize())
    return false;
  return true;
}

TypePtr ClassType::getCallable() {
  if (startswith(name, ".Partial."))
    return explicits[0].type;
  if (isCallable(name))
    return shared_from_this();
  return nullptr;
}

FuncType::FuncType(const string &name, ClassTypePtr funcClass,
                   const vector<TypePtr> &args, const vector<Generic> &explicits,
                   TypePtr parent)
    : explicits(explicits), parent(parent), funcClass(funcClass), name(name),
      args(args) {}

int FuncType::unify(TypePtr typ, Unification &us) {
  int s1 = 0, s = 0;
  if (auto t = typ->getFunc()) {
    if (explicits.size() != t->explicits.size())
      return -1;
    for (int i = 0; i < explicits.size(); i++) {
      if ((s = explicits[i].type->unify(t->explicits[i].type, us)) == -1)
        return -1;
      s1 += s;
    }
    // TODO : parent?
  }
  return s1 + getClass()->unify(typ, us);
}

string FuncType::realizeString() const {
  vector<string> gs;
  for (auto &a : explicits)
    if (!a.name.empty())
      gs.push_back(a.type->realizeString());
  string s = join(gs, ",");
  vector<string> as;
  for (int ai = 1; ai < args.size(); ai++)
    as.push_back(args[ai]->realizeString());
  string a = join(as, ",");
  s = s.size() ? join(vector<string>{s, a}, ";") : a;
  return fmt::format("{}{}{}", parent ? parent->realizeString() + ":" : "", name,
                     s.empty() ? "" : fmt::format("[{}]", s));
}

string FuncType::toString(bool reduced) const {
  vector<string> gs;
  for (auto &a : explicits)
    if (!a.name.empty())
      gs.push_back(a.type->toString(reduced));
  string s = join(gs, ",");
  vector<string> as;
  for (int ai = 0; ai < args.size(); ai++)
    as.push_back(args[ai]->toString(reduced));
  string a = join(as, ",");
  s = s.size() ? join(vector<string>{s, a}, ";") : a;
  return fmt::format("{}{}{}", parent ? parent->toString(reduced) + ":" : "", name,
                     s.empty() ? "" : fmt::format("[{}]", s));
}

TypePtr FuncType::generalize(int level) {
  auto a = args;
  for (auto &t : a)
    t = t->generalize(level);
  auto e = explicits;
  for (auto &t : e)
    t.type = t.type ? t.type->generalize(level) : nullptr;
  auto p = parent ? parent->generalize(level) : nullptr;
  auto c = make_shared<FuncType>(name, funcClass, a, e, p);
  c->setSrcInfo(getSrcInfo());
  return c;
}

TypePtr FuncType::instantiate(int level, int &unboundCount,
                              std::unordered_map<int, TypePtr> &cache) {
  auto e = explicits;
  for (auto &t : e)
    if (t.type) {
      t.type = t.type->instantiate(level, unboundCount, cache);
      if (cache.find(t.id) == cache.end())
        cache[t.id] = t.type;
    }
  auto a = args;
  for (auto &t : a)
    t = t->instantiate(level, unboundCount, cache);
  auto p = parent ? parent->instantiate(level, unboundCount, cache) : nullptr;
  auto c = make_shared<FuncType>(name, funcClass, a, e, p);
  c->setSrcInfo(getSrcInfo());
  return c;
}

bool FuncType::canRealize() const {
  for (auto &t : explicits)
    if (t.type && !t.type->canRealize())
      return false;
  for (int i = 1; i < args.size(); i++)
    if (!args[i]->canRealize())
      return false;
  if (parent && !parent->canRealize())
    return false;
  return true;
}

bool FuncType::hasUnbound() const {
  for (auto &t : explicits)
    if (t.type && t.type->hasUnbound())
      return true;
  for (int i = 1; i < args.size(); i++)
    if (args[i]->hasUnbound())
      return true;
  if (parent && parent->hasUnbound())
    return true;
  return false;
}

ClassTypePtr FuncType::getClass() {
  vector<Generic> exp;
  for (int ai = 0; ai < args.size(); ai++)
    exp.push_back({fmt::format("T{}", ai), args[ai], funcClass->explicits[ai].id});
  return make_shared<ClassType>(funcClass->name, true, args, exp, nullptr);
}

string v2b(const vector<char> &c) {
  string s(c.size(), '0');
  for (int i = 0; i < c.size(); i++)
    if (c[i])
      s[i] = '1';
  return s;
}

} // namespace types
} // namespace ast
} // namespace seq
