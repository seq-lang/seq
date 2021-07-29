/*
 * types.cpp --- Seq type definitions.
 * Contains a basic implementation of Hindley-Milner's W algorithm.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

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
vector<shared_ptr<Type>> Type::getUnbounds() const { return {}; }
string Type::toString() const { return debugString(false); }
bool Type::is(const string &s) { return getClass() && getClass()->name == s; }

LinkType::LinkType(Kind kind, int id, int level, TypePtr type, bool isStatic,
                   string genericName)
    : kind(kind), id(id), level(level), type(move(type)), isStatic(isStatic),
      genericName(move(genericName)) {
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
      LOG_TYPECHECK("[unify] {} := {}", id, typ->debugString(true));
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
      return make_shared<LinkType>(Generic, id, 0, nullptr, isStatic, genericName);
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
                                             isStatic, genericName);
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
bool LinkType::isInstantiated() const {
  return kind == Link ? type->isInstantiated() : false;
}
string LinkType::debugString(bool debug) const {
  if (kind == Unbound || kind == Generic)
    return debug ? fmt::format("{}{}", kind == Unbound ? '?' : '#', id)
                 : (genericName.empty() ? "?" : genericName);
  else
    return type->debugString(debug);
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

ClassType::ClassType(string name, string niceName, vector<Generic> generics)
    : name(move(name)), niceName(move(niceName)), generics(move(generics)),
      isTrait(false) {}
ClassType::ClassType(const ClassTypePtr &base)
    : name(base->name), niceName(base->niceName), generics(base->generics),
      isTrait(base->isTrait) {}
int ClassType::unify(Type *typ, Unification *us) {
  if (auto tc = typ->getClass()) {
    // Check names.
    if (name != tc->name)
      return -1;
    // Check generics.
    int s1 = 3, s;
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
  auto c = make_shared<ClassType>(name, niceName, g);
  c->isTrait = isTrait;
  c->setSrcInfo(getSrcInfo());
  return c;
}
TypePtr ClassType::instantiate(int atLevel, int &unboundCount,
                               unordered_map<int, TypePtr> &cache) {
  auto g = generics;
  for (auto &t : g)
    t.type = t.type ? t.type->instantiate(atLevel, unboundCount, cache) : nullptr;
  auto c = make_shared<ClassType>(name, niceName, g);
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
  if (isTrait)
    return false;
  return std::all_of(generics.begin(), generics.end(),
                     [](auto &t) { return !t.type || t.type->canRealize(); });
}
bool ClassType::isInstantiated() const {
  return std::all_of(generics.begin(), generics.end(),
                     [](auto &t) { return !t.type || t.type->isInstantiated(); });
}
string ClassType::debugString(bool debug) const {
  vector<string> gs;
  for (auto &a : generics)
    if (!a.name.empty())
      gs.push_back(a.type->debugString(debug));
  // Special formatting for Functions and Tuples
  auto n = niceName;
  if (startswith(n, TYPE_TUPLE))
    n = "Tuple";
  if (startswith(n, TYPE_FUNCTION))
    n = "Function";
  if (startswith(n, TYPE_CALLABLE))
    n = "Callable";
  return fmt::format("{}{}", n, gs.empty() ? "" : fmt::format("[{}]", join(gs, ",")));
}
string ClassType::realizedName() const {
  vector<string> gs;
  for (auto &a : generics)
    if (!a.name.empty())
      gs.push_back(a.type->realizedName());
  string s = join(gs, ",");
  return fmt::format("{}{}", name, s.empty() ? "" : fmt::format("[{}]", s));
}
bool ClassType::hasTrait() const {
  for (auto &a : generics)
    if (a.type && a.type->getClass() && a.type->getClass()->hasTrait())
      return true;
  return isTrait;
}
string ClassType::realizedTypeName() const { return this->ClassType::realizedName(); }

RecordType::RecordType(string name, string niceName, vector<Generic> generics,
                       vector<TypePtr> args)
    : ClassType(move(name), move(niceName), move(generics)), args(move(args)) {}
RecordType::RecordType(const ClassTypePtr &base, vector<TypePtr> args)
    : ClassType(base), args(move(args)) {}
int RecordType::unify(Type *typ, Unification *us) {
  if (auto tr = typ->getRecord()) {
    // Handle int <-> Int[64]
    if (name == "int" && tr->name == "Int")
      return tr->unify(this, us);
    if (tr->name == "int" && name == "Int") {
      auto t64 = make_shared<StaticType>(64);
      return generics[0].type->unify(t64.get(), us);
    }
    int s1 = 2, s;
    // Handle Callable[...]<->Function[...].
    if (startswith(tr->name, TYPE_CALLABLE) && startswith(name, TYPE_FUNCTION))
      return tr->unify(this, us);
    if (startswith(name, TYPE_CALLABLE) && startswith(tr->name, TYPE_FUNCTION)) {
      if (tr->generics.size() != generics.size())
        return -1;
      for (int i = 0; i < generics.size(); i++) {
        if ((s = generics[i].type->unify(tr->generics[i].type.get(), us)) == -1)
          return -1;
        s1 += s;
      }
      return s1;
    }
    // Handle Callable[...]<->Partial[...].
    if (startswith(tr->name, TYPE_CALLABLE) && !tr->getFunc() && getPartial())
      return tr->unify(this, us);
    if (startswith(name, TYPE_CALLABLE) && !getFunc() && tr->getPartial()) {
      vector<int> zeros;
      for (int pi = 9; pi < tr->name.size() && tr->name[pi] != '.'; pi++)
        if (tr->name[pi] == '0')
          zeros.emplace_back(pi - 9);
      // Just check if the number of generics matches. The rest will be done in
      // transformCall() procedure.
      if (zeros.size() + 1 != args.size())
        return -1;
      return s1;
    }
    if (args.size() != tr->args.size())
      return -1;
    for (int i = 0; i < args.size(); i++) {
      if ((s = args[i]->unify(tr->args[i].get(), us)) != -1)
        s1 += s;
      else
        return -1;
    }
    // Handle Tuple<->@tuple: when unifying tuples, only record members matter.
    if (startswith(name, TYPE_TUPLE) || startswith(tr->name, TYPE_TUPLE)) {
      return s1 + int(name == tr->name);
    }
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
bool RecordType::isInstantiated() const {
  return std::all_of(args.begin(), args.end(),
                     [](auto &a) { return a->isInstantiated(); }) &&
         this->ClassType::isInstantiated();
}
string RecordType::debugString(bool debug) const {
  return fmt::format("{}", this->ClassType::debugString(debug));
}
shared_ptr<RecordType> RecordType::getHeterogenousTuple() {
  seqassert(canRealize(), "{} not realizable", toString());
  // This is only relevant for Tuples and KwTuples.
  if ((startswith(name, TYPE_TUPLE) || startswith(name, TYPE_KWTUPLE)) &&
      args.size() > 1) {
    string first = args[0]->realizedName();
    for (int i = 1; i < args.size(); i++)
      if (args[i]->realizedName() != first)
        return getRecord();
  }
  return nullptr;
}

FuncType::FuncType(const shared_ptr<RecordType> &baseType, string funcName,
                   vector<Generic> funcGenerics, TypePtr funcParent)
    : RecordType(*baseType), funcName(move(funcName)), funcGenerics(move(funcGenerics)),
      funcParent(move(funcParent)) {}
int FuncType::unify(Type *typ, Unification *us) {
  if (this == typ)
    return 0;
  int s1 = 2, s = 0;
  if (auto t = typ->getFunc()) {
    // Check if names and parents match.
    if (funcName != t->funcName || (bool(funcParent) ^ bool(t->funcParent)))
      return -1;
    if (funcParent && (s = funcParent->unify(t->funcParent.get(), us)) == -1)
      return -1;
    s1 += s;
    // Check if function generics match.
    seqassert(funcGenerics.size() == t->funcGenerics.size(),
              "generic size mismatch for {}", funcName);
    for (int i = 0; i < funcGenerics.size(); i++) {
      if ((s = funcGenerics[i].type->unify(t->funcGenerics[i].type.get(), us)) == -1)
        return -1;
      s1 += s;
    }
  }
  s = this->RecordType::unify(typ, us);
  return s == -1 ? s : s1 + s;
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
  // Important: return type unbounds are not important, so skip them.
  for (int ai = 1; ai < args.size(); ai++) {
    auto tu = args[ai]->getUnbounds();
    u.insert(u.begin(), tu.begin(), tu.end());
  }
  return u;
}
bool FuncType::canRealize() const {
  // Important: return type does not have to be realized.
  for (int ai = 1; ai < args.size(); ai++)
    if (!args[ai]->getFunc() &&
        (!args[ai]->canRealize() ||
         (args[ai]->getClass() && args[ai]->getClass()->hasTrait())))
      return false;
  return std::all_of(funcGenerics.begin(), funcGenerics.end(),
                     [](auto &a) { return !a.type || a.type->canRealize(); }) &&
         (!funcParent || funcParent->canRealize());
}
bool FuncType::isInstantiated() const {
  TypePtr removed = nullptr;
  if (args[0]->getFunc() && args[0]->getFunc()->funcParent.get() == this) {
    removed = args[0]->getFunc()->funcParent;
    args[0]->getFunc()->funcParent = nullptr;
  }
  auto ret = std::all_of(funcGenerics.begin(), funcGenerics.end(),
                         [](auto &a) { return !a.type || a.type->isInstantiated(); }) &&
             (!funcParent || funcParent->isInstantiated()) &&
             this->RecordType::isInstantiated();
  if (removed)
    args[0]->getFunc()->funcParent = removed;
  return ret;
}
string FuncType::debugString(bool debug) const {
  vector<string> gs;
  for (auto &a : funcGenerics)
    if (!a.name.empty())
      gs.push_back(a.type->debugString(debug));
  string s = join(gs, ",");
  vector<string> as;
  // Important: return type does not have to be realized.
  for (int ai = debug ? 0 : 1; ai < args.size(); ai++)
    as.push_back(args[ai]->debugString(debug));
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
  // Important: return type does not have to be realized.
  for (int ai = 1; ai < args.size(); ai++)
    as.push_back(args[ai]->getFunc() ? args[ai]->getFunc()->realizedName()
                                     : args[ai]->realizedName());
  string a = join(as, ",");
  s = s.empty() ? a : join(vector<string>{s, a}, ";");
  return fmt::format("{}{}{}", funcParent ? funcParent->realizedName() + ":" : "",
                     funcName, s.empty() ? "" : fmt::format("[{}]", s));
}

PartialType::PartialType(const shared_ptr<RecordType> &baseType,
                         shared_ptr<FuncType> func, vector<char> known)
    : RecordType(*baseType), func(move(func)), known(move(known)) {}
int PartialType::unify(Type *typ, Unification *us) {
  int s1 = 0, s;
  if (auto tc = typ->getPartial()) {
    // Check names.
    if ((s = func->unify(tc->func.get(), us)) == -1)
      return -1;
    s1 += s;
  }
  s = this->RecordType::unify(typ, us);
  return s == -1 ? s : s1 + s;
}
TypePtr PartialType::generalize(int atLevel) {
  return make_shared<PartialType>(
      static_pointer_cast<RecordType>(this->RecordType::generalize(atLevel)), func,
      known);
}
TypePtr PartialType::instantiate(int atLevel, int &unboundCount,
                                 unordered_map<int, TypePtr> &cache) {
  return make_shared<PartialType>(
      static_pointer_cast<RecordType>(
          this->RecordType::instantiate(atLevel, unboundCount, cache)),
      func, known);
}
string PartialType::debugString(bool debug) const {
  vector<string> gs;
  for (auto &a : generics)
    if (!a.name.empty())
      gs.push_back(a.type->debugString(debug));
  vector<string> as;
  int i, gi;
  for (i = 0, gi = 0; i < known.size(); i++)
    if (!known[i])
      as.emplace_back("...");
    else
      as.emplace_back(gs[gi++]);
  return fmt::format("{}[{}]", func->funcName, join(as, ","));
}
string PartialType::realizedName() const {
  vector<string> gs;
  gs.push_back(func->realizedName());
  for (auto &a : generics)
    if (!a.name.empty())
      gs.push_back(a.type->realizedName());
  string s = join(gs, ",");
  return fmt::format("{}{}", name, s.empty() ? "" : fmt::format("[{}]", s));
}

StaticType::StaticType(vector<ClassType::Generic> generics,
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
    // Check if both types are already evaluated.
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
bool StaticType::isInstantiated() const { return staticEvaluation.first; }
string StaticType::debugString(bool debug) const {
  if (staticEvaluation.first)
    return fmt::format("{}", staticEvaluation.second);
  return fmt::format("Static[{}]",
                     staticExpr.first ? FormatVisitor::apply(staticExpr.first) : "");
}
string StaticType::realizedName() const {
  seqassert(canRealize(), "cannot realize {}", toString());
  vector<string> deps;
  for (auto &e : generics)
    deps.push_back(e.type->realizedName());
  if (!staticEvaluation.first) // If not already evaluated, evaluate!
    const_cast<StaticType *>(this)->staticEvaluation = {true, staticExpr.second(this)};
  return fmt::format("{}", staticEvaluation.second);
}

} // namespace types
} // namespace ast
} // namespace seq
