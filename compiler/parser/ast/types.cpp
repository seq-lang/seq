#include <memory>
#include <string>
#include <vector>

#include "parser/ast.h"
#include "parser/visitors/format/format.h"
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
bool Type::is(const string &s) { return getClass() && getClass()->name == s; }

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
TypePtr LinkType::generalize(int atLevel) {
  if (kind == Generic) {
    return shared_from_this();
  } else if (kind == Unbound) {
    if (level >= atLevel)
      return make_shared<LinkType>(Generic, id, 0, nullptr, isStatic);
    else
      return shared_from_this();
  } else {
    seqassert(type, "link is null");
    return type->generalize(atLevel);
  }
}
TypePtr LinkType::instantiate(int atLevel, int &unboundCount,
                              unordered_map<int, TypePtr> &cache) {
  if (kind == Generic) {
    if (cache.find(id) != cache.end())
      return cache[id];
    return cache[id] = make_shared<LinkType>(Unbound, unboundCount++, atLevel, nullptr,
                                             isStatic);
  } else if (kind == Unbound) {
    return shared_from_this();
  } else {
    seqassert(type, "link is null");
    return type->instantiate(atLevel, unboundCount, cache);
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
string LinkType::realizedName() const {
  if (kind == Unbound)
    return "?";
  seqassert(kind == Link, "unexpected generic link");
  return type->realizedName();
}
shared_ptr<LinkType> LinkType::getUnbound() {
  if (kind == Unbound)
    return std::static_pointer_cast<LinkType>(shared_from_this());
  if (kind == Link)
    return type->getUnbound();
  return nullptr;
}
bool LinkType::occurs(Type *typ, Type::Unification *undo) {
  if (auto tl = typ->getLink()) {
    if (tl->kind == Unbound) {
      if (tl->id == id)
        return true;
      if (undo && tl->level > level) {
        undo->leveled.emplace_back(make_pair(tl.get(), tl->level));
        tl->level = level;
      }
      return false;
    } else if (tl->kind == Link) {
      return occurs(tl->type.get(), undo);
    } else {
      return false;
    }
  } else if (auto ts = typ->getStatic()) {
    return std::any_of(ts->generics.begin(), ts->generics.end(),
                       [&](auto &g) { return g.type && occurs(g.type.get(), undo); });
  }
  if (auto tc = typ->getClass()) {
    for (auto &g : tc->generics)
      if (g.type && occurs(g.type.get(), undo))
        return true;
    if (auto tr = typ->getRecord())
      for (auto &t : tr->args)
        if (occurs(t.get(), undo))
          return true;
    return false;
  } else {
    return false;
  }
}

/////

ClassType::ClassType(string name, vector<Generic> generics)
    : name(move(name)), generics(move(generics)), isTrait(false) {}
ClassType::ClassType(const ClassTypePtr &base)
    : name(base->name), generics(base->generics), isTrait(base->isTrait) {}
int ClassType::unify(Type *typ, Unification *us) {
  if (auto tc = typ->getClass()) {
    if (name != tc->name)
      return -1;
    int s1 = 2, s;
    if (generics.size() != tc->generics.size())
      return -1;
    for (int i = 0; i < generics.size(); i++) {
      if ((s = generics[i].type->unify(tc->generics[i].type.get(), us)) == -1)
        return -1;
      s1 += s;
    }
    return s1;
  } else if (auto tl = typ->getLink()) {
    return tl->unify(this, us);
  } else {
    return -1;
  }
}
TypePtr ClassType::generalize(int atLevel) {
  auto g = generics;
  for (auto &t : g)
    t.type = t.type ? t.type->generalize(atLevel) : nullptr;
  auto c = make_shared<ClassType>(name, g);
  c->isTrait = isTrait;
  c->setSrcInfo(getSrcInfo());
  return c;
}
TypePtr ClassType::instantiate(int atLevel, int &unboundCount,
                               unordered_map<int, TypePtr> &cache) {
  auto g = generics;
  for (auto &t : g)
    t.type = t.type ? t.type->instantiate(atLevel, unboundCount, cache) : nullptr;
  auto c = make_shared<ClassType>(name, g);
  c->isTrait = isTrait;
  c->setSrcInfo(getSrcInfo());
  return c;
}
vector<TypePtr> ClassType::getUnbounds() const {
  vector<TypePtr> u;
  for (auto &t : generics)
    if (t.type) {
      auto tu = t.type->getUnbounds();
      u.insert(u.begin(), tu.begin(), tu.end());
    }
  return u;
}
bool ClassType::canRealize() const {
  return std::all_of(generics.begin(), generics.end(),
                     [](auto &t) { return !t.type || t.type->canRealize(); });
}
string ClassType::toString() const {
  vector<string> gs;
  for (auto &a : generics)
    if (!a.name.empty())
      gs.push_back(a.type->toString());
  return fmt::format("{}{}", name,
                     gs.empty() ? "" : fmt::format("[{}]", join(gs, ",")));
}
string ClassType::realizedName() const {
  vector<string> gs;
  for (auto &a : generics)
    if (!a.name.empty())
      gs.push_back(a.type->realizedName());
  string s = join(gs, ",");
  return fmt::format("{}{}", name, s.empty() ? "" : fmt::format("[{}]", s));
}
string ClassType::realizedTypeName() const {
  return this->ClassType::realizedName();
}

RecordType::RecordType(string name, vector<Generic> generics, vector<TypePtr> args)
    : ClassType(move(name), move(generics)), args(move(args)) {}
RecordType::RecordType(const ClassTypePtr &base, vector<TypePtr> args)
    : ClassType(base), args(move(args)) {}
int RecordType::unify(Type *typ, Unification *us) {
  if (auto tr = typ->getRecord()) {
    if (name == "int" && tr->name == "Int")
      return tr->unify(this, us);
    if (tr->name == "int" && name == "Int") {
      auto t64 = make_shared<StaticType>(64);
      return generics[0].type->unify(t64.get(), us);
    }
    if (args.size() != tr->args.size())
      return -1;
    int s1 = 2, s;
    for (int i = 0; i < args.size(); i++) {
      if ((s = args[i]->unify(tr->args[i].get(), us)) != -1)
        s1 += s;
      else
        return -1;
    }
    // When unifying records, only record members matter.
    if (startswith(name, "Tuple.N") || startswith(tr->name, "Tuple.N"))
      return s1;
    // TODO : needed at all?
    auto isFunc = [](const string &name) { return startswith(name, "Function.N"); };
    if (isFunc(name) && isFunc(tr->name))
      return s1;
    return this->ClassType::unify(tr.get(), us);
  } else if (auto t = typ->getLink()) {
    return t->unify(this, us);
  } else {
    return -1;
  }
}
TypePtr RecordType::generalize(int atLevel) {
  auto c = static_pointer_cast<ClassType>(this->ClassType::generalize(atLevel));
  auto a = args;
  for (auto &t : a)
    t = t->generalize(atLevel);
  return make_shared<RecordType>(c, a);
}
TypePtr RecordType::instantiate(int atLevel, int &unboundCount,
                                unordered_map<int, TypePtr> &cache) {
  auto c = static_pointer_cast<ClassType>(
      this->ClassType::instantiate(atLevel, unboundCount, cache));
  auto a = args;
  for (auto &t : a)
    t = t->instantiate(atLevel, unboundCount, cache);
  return make_shared<RecordType>(c, a);
}
vector<TypePtr> RecordType::getUnbounds() const {
  vector<TypePtr> u;
  for (auto &a : args) {
    auto tu = a->getUnbounds();
    u.insert(u.begin(), tu.begin(), tu.end());
  }
  auto tu = this->ClassType::getUnbounds();
  u.insert(u.begin(), tu.begin(), tu.end());
  return u;
}
bool RecordType::canRealize() const {
  return std::all_of(args.begin(), args.end(),
                     [](auto &a) { return a->canRealize(); }) &&
         this->ClassType::canRealize();
}
string RecordType::toString() const {
  vector<string> as;
  for (auto &a : args)
    as.push_back(a->toString());
  return fmt::format("{}{}", this->ClassType::toString(),
                     as.empty() ? "" : "<" + join(as, ",") + ">");
}

////

FuncType::FuncType(const shared_ptr<RecordType> &baseType, string funcName,
                   vector<Generic> funcGenerics, TypePtr funcParent)
    : RecordType(*baseType), funcName(move(funcName)), funcGenerics(move(funcGenerics)),
      funcParent(move(funcParent)) {}
int FuncType::unify(Type *typ, Unification *us) {
  int s1 = 2, s = 0;
  if (auto t = typ->getFunc()) {
    if (funcName != t->funcName || (bool(funcParent) ^ bool(t->funcParent)))
      return -1;
    if (funcParent && (s = funcParent->unify(t->funcParent.get(), us)) == -1)
      return -1;
    s1 += s;
    seqassert(funcGenerics.size() == t->funcGenerics.size(),
              "generic size mismatch for {}", funcName);
    for (int i = 0; i < funcGenerics.size(); i++) {
      if ((s = funcGenerics[i].type->unify(t->funcGenerics[i].type.get(), us)) == -1)
        return -1;
      s1 += s;
    }
  }
  return s1 + this->RecordType::unify(typ, us);
}
TypePtr FuncType::generalize(int atLevel) {
  auto g = funcGenerics;
  for (auto &t : g)
    t.type = t.type ? t.type->generalize(atLevel) : nullptr;
  auto p = funcParent ? funcParent->generalize(atLevel) : nullptr;
  return make_shared<FuncType>(
      static_pointer_cast<RecordType>(this->RecordType::generalize(atLevel)), funcName,
      g, p);
}
TypePtr FuncType::instantiate(int atLevel, int &unboundCount,
                              unordered_map<int, TypePtr> &cache) {
  auto g = funcGenerics;
  for (auto &t : g)
    if (t.type) {
      t.type = t.type->instantiate(atLevel, unboundCount, cache);
      if (cache.find(t.id) == cache.end())
        cache[t.id] = t.type;
    }
  auto p = funcParent ? funcParent->instantiate(atLevel, unboundCount, cache) : nullptr;
  return make_shared<FuncType>(
      static_pointer_cast<RecordType>(
          this->RecordType::instantiate(atLevel, unboundCount, cache)),
      funcName, g, p);
}
vector<TypePtr> FuncType::getUnbounds() const {
  vector<TypePtr> u;
  for (auto &t : funcGenerics)
    if (t.type) {
      auto tu = t.type->getUnbounds();
      u.insert(u.begin(), tu.begin(), tu.end());
    }
  if (funcParent) {
    auto tu = funcParent->getUnbounds();
    u.insert(u.begin(), tu.begin(), tu.end());
  }
  for (int ai = 1; ai < args.size(); ai++) {
    auto tu = args[ai]->getUnbounds();
    u.insert(u.begin(), tu.begin(), tu.end());
  }
  return u;
}
bool FuncType::canRealize() const {
  for (int ai = 1; ai < args.size(); ai++)
    if (!args[ai]->canRealize())
      return false;
  return std::all_of(funcGenerics.begin(), funcGenerics.end(),
                     [](auto &a) { return !a.type || a.type->canRealize(); }) &&
         (!funcParent || funcParent->canRealize());
}
string FuncType::toString() const {
  vector<string> gs;
  for (auto &a : funcGenerics)
    if (!a.name.empty())
      gs.push_back(a.type->toString());
  string s = join(gs, ",");
  vector<string> as;
  for (auto &a : args)
    as.push_back(a->toString());
  string a = join(as, ",");
  s = s.empty() ? a : join(vector<string>{s, a}, ";");
  return fmt::format("{}{}", funcName, s.empty() ? "" : fmt::format("[{}]", s));
}
string FuncType::realizedName() const {
  vector<string> gs;
  for (auto &a : funcGenerics)
    if (!a.name.empty())
      gs.push_back(a.type->realizedName());
  string s = join(gs, ",");
  vector<string> as;
  for (int ai = 1; ai < args.size(); ai++)
    as.push_back(args[ai]->realizedName());
  string a = join(as, ",");
  s = s.empty() ? a : join(vector<string>{s, a}, ";");
  return fmt::format("{}{}{}", funcParent ? funcParent->realizedName() + ":" : "",
                     funcName, s.empty() ? "" : fmt::format("[{}]", s));
}

////

StaticType::StaticType(vector<Generic> generics,
                       pair<unique_ptr<Expr>, EvalFn> staticExpr,
                       pair<bool, int> staticEvaluation)
    : generics(move(generics)), staticEvaluation(move(staticEvaluation)),
      staticExpr(move(staticExpr)) {
  seqassert(!staticExpr.first ||
                (!staticExpr.first->getId() && !staticExpr.first->getInt()),
            "invalid complex static expression");
}
StaticType::StaticType(int i) : staticEvaluation(true, i) {
  staticExpr = {nullptr,
                [](const StaticType *t) { return t->staticEvaluation.second; }};
}
int StaticType::unify(Type *typ, Unification *us) {
  if (auto t = typ->getStatic()) {
    if (staticEvaluation.first && t->staticEvaluation.first)
      return staticEvaluation == t->staticEvaluation ? 2 : -1;

    if (generics.size() != t->generics.size())
      return -1;
    // We assume that both expressions are simple or both expressions are complex.
    assert(generics.empty() || staticExpr.first);

    int s1 = 2, s;
    if (!generics.empty()) {
      if (staticExpr.first->toString() != t->staticExpr.first->toString())
        return -1;
      for (int i = 0; i < generics.size(); i++) {
        if ((s = generics[i].type->unify(t->generics[i].type.get(), us)) == -1)
          return -1;
        s1 += s;
      }
    } else {
      seqassert(staticEvaluation.first && t->staticEvaluation.first,
                "unevaluated simple expression");
      return staticEvaluation == t->staticEvaluation ? 2 : -1;
    }
    return s1;
  } else if (auto tl = typ->getLink()) {
    return tl->unify(this, us);
  } else {
    return -1;
  }
}
TypePtr StaticType::generalize(int atLevel) {
  auto e = generics;
  for (auto &t : e)
    t.type = t.type ? t.type->generalize(atLevel) : nullptr;
  auto c = make_shared<StaticType>(
      e, std::make_pair(clone(staticExpr.first), staticExpr.second), staticEvaluation);
  c->setSrcInfo(getSrcInfo());
  return c;
}
TypePtr StaticType::instantiate(int atLevel, int &unboundCount,
                                unordered_map<int, TypePtr> &cache) {
  auto e = generics;
  for (auto &t : e)
    t.type = t.type ? t.type->instantiate(atLevel, unboundCount, cache) : nullptr;
  auto c = make_shared<StaticType>(
      e, std::make_pair(clone(staticExpr.first), staticExpr.second), staticEvaluation);
  c->setSrcInfo(getSrcInfo());
  return c;
}
vector<TypePtr> StaticType::getUnbounds() const {
  vector<TypePtr> u;
  for (auto &t : generics)
    if (t.type) {
      auto tu = t.type->getUnbounds();
      u.insert(u.begin(), tu.begin(), tu.end());
    }
  return u;
}
bool StaticType::canRealize() const {
  if (!staticEvaluation.first)
    for (auto &t : generics)
      if (t.type && !t.type->canRealize())
        return false;
  return true;
}
string StaticType::toString() const {
  if (staticEvaluation.first)
    return fmt::format("{}", staticEvaluation.second);
  vector<string> s;
  for (auto &e : generics)
    s.push_back(fmt::format("{}={}", e.name, e.type->toString()));
  return fmt::format("Static[{}]",
                     staticExpr.first ? FormatVisitor::apply(staticExpr.first) : "");
}
string StaticType::realizedName() const {
  assert(canRealize());
  vector<string> deps;
  for (auto &e : generics)
    deps.push_back(e.type->realizedName());
  if (!staticEvaluation.first)
    const_cast<StaticType *>(this)->staticEvaluation = {true, staticExpr.second(this)};
  return fmt::format("{}{}", deps.empty() ? "" : join(deps, ";"),
                     staticEvaluation.second);
}

} // namespace types
} // namespace ast
} // namespace seq
