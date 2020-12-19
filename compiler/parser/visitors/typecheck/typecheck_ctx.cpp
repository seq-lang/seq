#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/typecheck/typecheck_ctx.h"

using fmt::format;
using std::dynamic_pointer_cast;
using std::stack;

namespace seq {
namespace ast {

TypeContext::TypeContext(shared_ptr<Cache> cache)
    : Context<TypecheckItem>(""), cache(move(cache)), typecheckLevel(0), iteration(0),
      extendEtape(0) {
  stack.push_front(vector<string>());
  bases.push_back({"", nullptr, nullptr});
}

pair<TypecheckItem::Kind, types::TypePtr>
TypeContext::findInVisited(const string &name) const {
  for (int bi = int(bases.size()) - 1; bi >= 0; bi--) {
    auto t = bases[bi].visitedAsts.find(name);
    if (t == bases[bi].visitedAsts.end())
      continue;
    return t->second;
  }
  return {TypecheckItem::Var, nullptr};
}

shared_ptr<TypecheckItem> TypeContext::find(const string &name) const {
  if (auto t = Context<TypecheckItem>::find(name))
    return t;
  if (!name.empty() && name[0] == '.') {
    auto tt = findInVisited(name);
    if (tt.second)
      return make_shared<TypecheckItem>(tt.first, tt.second, "");
  }
  // ((SimplifyContext *)this)->dump();
  return nullptr;
}

types::TypePtr TypeContext::findInternal(const string &name) const {
  auto t = find(name);
  seqassert(t, "cannot find '{}'", name);
  return t->getType();
}

shared_ptr<TypecheckItem> TypeContext::add(TypecheckItem::Kind kind, const string &name,
                                           types::TypePtr type, bool global,
                                           bool generic, bool stat) {
  auto t = make_shared<TypecheckItem>(kind, type, getBase(), global, generic, stat);
  if (name[0] == '.')
    addToplevel(name, t);
  else
    add(name, t);
  return t;
}

string TypeContext::getBase() const {
  if (!bases.size())
    return "";
  vector<string> s;
  for (auto &b : bases)
    if (b.type)
      s.push_back(b.type->realizeString());
  return join(s, ":");
}

shared_ptr<types::LinkType> TypeContext::addUnbound(const SrcInfo &srcInfo, int level,
                                                    bool setActive, bool isStatic) {
  auto t = make_shared<types::LinkType>(types::LinkType::Unbound, cache->unboundCount++,
                                        level, nullptr, isStatic);
  t->setSrcInfo(srcInfo);
  LOG_TYPECHECK("[ub] new {}: {} ({})", t->toString(), srcInfo, setActive);
  if (cache->unboundCount - 1 == 10245)
    assert(1);
  if (setActive)
    activeUnbounds.insert(t);
  return t;
}

types::TypePtr TypeContext::instantiate(const SrcInfo &srcInfo, types::TypePtr type) {
  return instantiate(srcInfo, type, nullptr);
}

types::TypePtr TypeContext::instantiate(const SrcInfo &srcInfo, types::TypePtr type,
                                        types::ClassType *generics, bool activate) {
  assert(type);
  unordered_map<int, types::TypePtr> genericCache;
  if (generics)
    for (auto &g : generics->explicits)
      if (g.type &&
          !(g.type->getLink() && g.type->getLink()->kind == types::LinkType::Generic)) {
        genericCache[g.id] = g.type;
      }
  auto t = type->instantiate(getLevel(), cache->unboundCount, genericCache);
  for (auto &i : genericCache) {
    if (auto l = i.second->getLink()) {
      if (l->kind != types::LinkType::Unbound)
        continue;
      i.second->setSrcInfo(srcInfo);
      if (activeUnbounds.find(i.second) == activeUnbounds.end()) {
        LOG_TYPECHECK("[ub] #{} -> {} (during inst of {}): {} ({})", i.first,
                      i.second->toString(), type->toString(), srcInfo, activate);
        if (i.second->toString() == "?10245.0")
          assert(1);
        if (activate)
          activeUnbounds.insert(i.second);
      }
    }
  }
  return t;
}

types::TypePtr TypeContext::instantiateGeneric(const SrcInfo &srcInfo,
                                               types::TypePtr root,
                                               const vector<types::TypePtr> &generics) {
  auto c = root->getClass();
  assert(c);
  auto g = make_shared<types::ClassType>(""); // dummy generic type
  if (generics.size() != c->explicits.size())
    error(srcInfo, "generics do not match");
  for (int i = 0; i < c->explicits.size(); i++) {
    assert(c->explicits[i].type);
    g->explicits.push_back(types::Generic("", generics[i], c->explicits[i].id));
  }
  return instantiate(srcInfo, root, g.get());
}

void TypeContext::dump(int pad) {
  auto ordered = std::map<string, decltype(map)::mapped_type>(map.begin(), map.end());
  LOG("base: {}", getBase());
  for (auto &i : ordered) {
    string s;
    auto t = i.second.front().second;
    LOG("{}{:.<25} {} {}", string(pad * 2, ' '), i.first, t->getType()->toString(),
        t->getBase());
  }
}

} // namespace ast
} // namespace seq
