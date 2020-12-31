#include <memory>
#include <string>
#include <vector>

#include "parser/ast.h"
#include "parser/visitors/typecheck/typecheck.h"

using std::dynamic_pointer_cast;
using std::min;
using std::static_pointer_cast;

namespace seq {
namespace ast {
namespace types {

void Type::Unification::undo() {
  for (int i = int(linked.size()) - 1; i >= 0; i--) {
    linked[i]->kind = LinkType::Unbound;
    linked[i]->type = nullptr;
  }
  for (int i = int(leveled.size()) - 1; i >= 0; i--) {
    assert(leveled[i].first->kind == LinkType::Unbound);
    leveled[i].first->level = leveled[i].second;
  }
}
TypePtr Type::follow() { return shared_from_this(); }

LinkType::LinkType(Kind kind, int id, int level, TypePtr type, bool isStatic)
    : kind(kind), id(id), level(level), type(move(type)), isStatic(isStatic) {
  seqassert((this->type && kind == Link) || (!this->type && kind == Generic) ||
                (!this->type && kind == Unbound),
            "inconsistent link state");
}
LinkType::LinkType(TypePtr type)
    : kind(Link), id(0), level(0), type(move(type)), isStatic(false) {
  seqassert(this->type, "link to nullptr");
}
int LinkType::unify(Type *typ, Unification *undo) {
  if (kind == Link) {
    // Case 1: Just follow the link
    return type->unify(typ, undo);
  } else if (kind == Generic) {
    // Case 2: Generic types cannot be unified.
    // HACK: allow unification between two identical generics (TODO: explain why?).
    auto t = typ->getLink();
    if (t && t->kind == Generic && id == t->id && isStatic == t->isStatic)
      return 1;
    return -1;
  } else {
    // Case 3: Unbound unification
    if (auto t = typ->getLink()) {
      if (t->kind == Link)
        return t->type->unify(this, undo);
      else if (t->kind == Generic)
        return -1;
      else {
        if (isStatic != t->isStatic)
          return -1;
        else if (id == t->id)
          // Identical unbound types get a score of 1
          return 1;
        else if (id < t->id)
          // Always merge a newer type into the older type (e.g. keep the types with
          // lower IDs around).
          return t->unify(this, undo);
      }
    }
    // Ensure that we do not have recursive unification! (e.g. unify ?1 with list[?1])
    if (occurs(typ, undo))
      return -1;

    // ⚠️ Unification: destructive part.
    seqassert(!type, "type has been already unified or is in inconsistent state");
    if (undo) {
      LOG_TYPECHECK("[unify] {} := {}", id, typ->toString());
      if (id == 11677 || id == 11722)
        assert(1);
      // Link current type to typ and ensure that this modification is recorded in undo.
      undo->linked.push_back(this);
      kind = Link;
      seqassert(!typ->getLink() || typ->getLink()->kind != Unbound ||
                    typ->getLink()->id <= id,
                "type unification is not consistent");
      type = typ->follow();
    }
    return 0;
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
  } else {
    seqassert(type, "link is null");
    return type->generalize(level);
  }
}
TypePtr LinkType::instantiate(int level, int &unboundCount,
                              unordered_map<int, TypePtr> &cache) {
  if (kind == Generic) {
    if (cache.find(id) != cache.end())
      return cache[id];
    // LOG_TYPECHECK("[inst] #{} -> ?{}", id, unboundCount);
    return cache[id] =
               make_shared<LinkType>(Unbound, unboundCount++, level, nullptr, isStatic);
  } else if (kind == Unbound) {
    return shared_from_this();
  } else {
    seqassert(type, "link is null");
    return type->instantiate(level, unboundCount, cache);
  }
}
TypePtr LinkType::follow() {
  if (kind == Link)
    return type->follow();
  else
    return shared_from_this();
}
vector<TypePtr> LinkType::getUnbounds() const {
  if (kind == Unbound)
    return {std::const_pointer_cast<Type>(shared_from_this())};
  else if (kind == Link)
    return type->getUnbounds();
  return {};
}
bool LinkType::canRealize() const {
  if (kind != Link)
    return false;
  else
    return type->canRealize();
}
string LinkType::toString() const {
  if (kind == Unbound)
    return fmt::format("?{}.{}", id, level, isStatic ? "_s" : "");
  else if (kind == Generic)
    return fmt::format("#{}.{}", id, level, isStatic ? "_s" : "");
  else
    return type->toString();
}
string LinkType::realizeString() const {
  if (kind == Unbound)
    return "?";
  seqassert(kind == Link, "unexpected generic link");
  return type->realizeString();
}
shared_ptr<LinkType> LinkType::getUnbound() {
  if (kind == Unbound)
    return std::static_pointer_cast<LinkType>(shared_from_this());
  if (kind == Link)
    return type->getUnbound();
  return nullptr;
}
bool LinkType::occurs(Type *typ, Type::Unification *undo) {
  if (auto t = typ->getLink()) {
    if (t->kind == Unbound) {
      if (t->id == id)
        return true;
      if (undo && t->level > level) {
        undo->leveled.push_back({t.get(), t->level});
        t->level = level;
      }
      return false;
    } else if (t->kind == Link) {
      return occurs(t->type.get(), undo);
    } else {
      return false;
    }
  } else if (auto t = typ->getStatic()) {
    for (auto &g : t->explicits)
      if (g.type && occurs(g.type.get(), undo))
        return true;
    return false;
  } else if (auto t = typ->getClass()) {
    for (auto &g : t->explicits)
      if (g.type && occurs(g.type.get(), undo))
        return true;
    if (t->parent && occurs(t->parent.get(), undo))
      return true;
    for (auto &t : t->args)
      if (occurs(t.get(), undo))
        return true;
    return false;
  } else {
    return false;
  }
}

/////

StaticType::StaticType(const vector<Generic> &ex, unique_ptr<Expr> &&e)
    : explicits(ex), expr(move(e)) {}

StaticType::StaticType(int i) { expr = std::make_unique<IntExpr>(i); }

string StaticType::toString() const {
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

int StaticType::unify(Type *typ, Unification *us) {
  if (auto t = typ->getStatic()) {
    // A + 5 + 3; 3 + A + 5
    int s1 = 2;
    if (expr->toString() == t->expr->toString()) {
      int s = 0;
      for (int i = 0; i < explicits.size(); i++) {
        if ((s = explicits[i].type->unify(t->explicits[i].type.get(), us)) == -1)
          return -1;
        s1 += s;
      }
      return s1;
    }
    return -1;
  } else if (auto t = typ->getLink()) {
    return t->unify(this, us);
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

vector<TypePtr> StaticType::getUnbounds() const {
  vector<TypePtr> u;
  for (auto &t : explicits)
    if (t.type) {
      auto tu = t.type->getUnbounds();
      u.insert(u.begin(), tu.begin(), tu.end());
    }
  return u;
}

bool StaticType::canRealize() const {
  for (auto &t : explicits)
    if (t.type && !t.type->canRealize())
      return false;
  return true;
}

int StaticType::getValue() const {
  map<string, Generic> m;
  for (auto &e : explicits)
    m[e.name] = e;
  auto t = StaticVisitor(m).transform(expr);
  assert(t.first);
  return t.second;
}

bool isTuple(const string &name) { return startswith(name, "Tuple.N"); }

bool isCallable(const string &name) {
  return startswith(name, "Function.N") || startswith(name, "Partial.N");
}

bool isFunc(const string &name) { return startswith(name, "Function.N"); }

ClassType::ClassType(const string &name, bool isRecord, const vector<TypePtr> &args,
                     const vector<Generic> &explicits, TypePtr parent)
    : explicits(explicits), parent(parent), isTrait(false), name(name),
      record(isRecord), args(args) {}

string ClassType::toString() const {
  vector<string> gs;
  for (auto &a : explicits)
    if (!a.name.empty())
      gs.push_back(a.type->toString());
  auto g = join(gs, ",");
  return fmt::format("{}{}",
                     // reduced && parent ? parent->toString(reduced) + ":" : "",
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

int ClassType::unify(Type *typ, Unification *us) {
  if (auto t = typ->getClass()) {
    if (isRecord() != t->isRecord())
      return -1;

    auto ti64 = make_shared<StaticType>(64);
    if (name == "int" && t->name == "Int")
      return t->explicits[0].type->unify(ti64.get(), us);
    if (t->name == "int" && name == "Int")
      return explicits[0].type->unify(ti64.get(), us);

    if (us && us->isMatch) {
      if ((t->name == "Kmer" && name == "seq") || (name == "Kmer" && t->name == "seq"))
        return 0;
    }

    if (args.size() != t->args.size())
      return -1;
    int s1 = 2, s;
    for (int i = 0; i < args.size(); i++) {
      if ((s = args[i]->unify(t->args[i].get(), us)) != -1)
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
      if ((s = explicits[i].type->unify(t->explicits[i].type.get(), us)) == -1)
        return -1;
      s1 += s;
    }
    if (parent) {
      if ((s = parent->unify(t->parent.get(), us)) == -1) {
        return -1;
      }
      s1 += s;
    }
    return s1;
  } else if (auto t = typ->getLink()) {
    return t->unify(this, us);
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
      // if (cache.find(t.id) == cache.end())
      // cache[t.id] = t.type;
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

vector<TypePtr> ClassType::getUnbounds() const {
  vector<TypePtr> u;
  for (int i = 0; i < args.size(); i++) {
    auto tu = args[i]->getUnbounds();
    u.insert(u.begin(), tu.begin(), tu.end());
  }
  for (auto &t : explicits)
    if (t.type) {
      auto tu = t.type->getUnbounds();
      u.insert(u.begin(), tu.begin(), tu.end());
    }
  if (parent) {
    auto tu = parent->getUnbounds();
    u.insert(u.begin(), tu.begin(), tu.end());
  }
  return u;
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
  if (startswith(name, "Partial.N"))
    return explicits[0].type;
  if (isCallable(name))
    return shared_from_this();
  return nullptr;
}

FuncType::FuncType(const string &name, ClassType *funcClass,
                   const vector<TypePtr> &args, const vector<Generic> &explicits,
                   TypePtr parent)
    : explicits(explicits), parent(parent), name(name), args(args) {
  vector<Generic> exp;
  for (int ai = 0; ai < args.size(); ai++)
    exp.push_back({fmt::format("T{}", ai), args[ai], funcClass->explicits[ai].id});
  this->funcClass = make_shared<ClassType>(funcClass->name, true, args, exp, nullptr);
}

int FuncType::unify(Type *typ, Unification *us) {
  int s1 = 2, s = 0;
  if (auto t = typ->getFunc()) {
    if (explicits.size() != t->explicits.size())
      return -1;
    for (int i = 0; i < explicits.size(); i++) {
      if ((s = explicits[i].type->unify(t->explicits[i].type.get(), us)) == -1)
        return -1;
      s1 += s;
    }
    if (bool(parent) ^ bool(t->parent))
      return -1;
    if (parent) {
      if ((s = parent->unify(t->parent.get(), us)) == -1)
        return -1;
      s1 += s;
    }
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

string FuncType::toString() const {
  vector<string> gs;
  for (auto &a : explicits)
    if (!a.name.empty())
      gs.push_back(a.type->toString());
  string s = join(gs, ",");
  vector<string> as;
  for (int ai = 0; ai < args.size(); ai++)
    as.push_back(args[ai]->toString());
  string a = join(as, ",");
  s = s.size() ? join(vector<string>{s, a}, ";") : a;
  return fmt::format("{}{}", // parent ? parent->toString(reduced) + ":" : "",
                     name, s.empty() ? "" : fmt::format("[{}]", s));
}

TypePtr FuncType::generalize(int level) {
  auto a = args;
  for (auto &t : a)
    t = t->generalize(level);
  auto e = explicits;
  for (auto &t : e)
    t.type = t.type ? t.type->generalize(level) : nullptr;
  auto p = parent ? parent->generalize(level) : nullptr;
  auto c = make_shared<FuncType>(name, funcClass.get(), a, e, p);
  c->setSrcInfo(getSrcInfo());
  return c;
}

TypePtr FuncType::instantiate(int level, int &unboundCount,
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
  auto c = make_shared<FuncType>(name, funcClass.get(), a, e, p);
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

vector<TypePtr> FuncType::getUnbounds() const {
  vector<TypePtr> u;
  for (auto &t : explicits)
    if (t.type) {
      auto tu = t.type->getUnbounds();
      u.insert(u.begin(), tu.begin(), tu.end());
    }
  for (int i = 1; i < args.size(); i++) {
    auto tu = args[i]->getUnbounds();
    u.insert(u.begin(), tu.begin(), tu.end());
  }
  if (parent) {
    auto tu = parent->getUnbounds();
    u.insert(u.begin(), tu.begin(), tu.end());
  }
  return u;
}

} // namespace types
} // namespace ast
} // namespace seq
