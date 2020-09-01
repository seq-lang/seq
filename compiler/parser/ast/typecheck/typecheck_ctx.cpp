#include <map>
#include <memory>
#include <stack>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/common.h"
#include "parser/ocaml.h"
#include "parser/typecheck/typecheck_ctx.h"

using fmt::format;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::make_unique;
using std::pair;
using std::shared_ptr;
using std::stack;
using std::string;
using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace seq {
namespace ast {

TypeContext::TypeContext(const std::string &filename, shared_ptr<Cache> cache)
    : Context<TypecheckItem>("", cache) {
  stack.push_front(vector<string>());
}

types::TypePtr TypeContext::findInternal(const string &name) const {
  auto t = find(name);
  seqassert(t, "cannot find '{}'", name);
  return t->getType();
}

shared_ptr<TypecheckItem> TransformContext::add(TypecheckItem::Kind kind,
                                                const string &name,
                                                const types::TypePtr type, bool global,
                                                bool generic, bool stat) {
  auto t = make_shared<TypecheckItem>(kind, type, getBase(), global, generic, stat);
  add(name, t);
  return t;
}

string TypeContext::getBase(bool full) const {
  if (!bases.size())
    return "";
  if (!full) {
    if (auto f = bases.back().type->getFunc())
      return f->name;
    assert(bases.back().type->getClass());
    return bases.back().type->getClass()->name;
  } else {
    vector<string> s;
    for (auto &b : bases) {
      if (auto f = b.type->getFunc())
        s.push_back(f->name);
      else
        s.push_back(b.type->getClass()->name);
    }
    return join(s, ":");
  }
}

shared_ptr<types::LinkType> TypeContext::addUnbound(const SrcInfo &srcInfo, int level,
                                                    bool setActive, bool isStatic) {
  auto t = make_shared<types::LinkType>(types::LinkType::Unbound,
                                        realizations->getUnboundCount()++, level,
                                        nullptr, isStatic);
  t->setSrcInfo(srcInfo);
  if (!typecheck)
    assert(1);
  LOG7("[ub] new {}: {} ({}); tc={}", t->toString(0), srcInfo, setActive, typecheck);
  if (setActive)
    activeUnbounds.insert(t);
  return t;
}

types::TypePtr TypeContext::instantiate(const SrcInfo &srcInfo, types::TypePtr type) {
  return instantiate(srcInfo, type, nullptr);
}

types::TypePtr TypeContext::instantiate(const SrcInfo &srcInfo, types::TypePtr type,
                                        types::ClassTypePtr generics, bool activate) {
  unordered_map<int, types::TypePtr> cache;
  if (generics)
    for (auto &g : generics->explicits)
      if (g.type &&
          !(g.type->getLink() && g.type->getLink()->kind == types::LinkType::Generic)) {
        // LOG7("{} inst: {} -> {}", type->toString(), g.id, g.type->toString());
        cache[g.id] = g.type;
      }
  auto t = type->instantiate(getLevel(), realizations->getUnboundCount(), cache);
  for (auto &i : cache) {
    if (auto l = i.second->getLink()) {
      if (l->kind != types::LinkType::Unbound)
        continue;
      i.second->setSrcInfo(srcInfo);
      if (activeUnbounds.find(i.second) == activeUnbounds.end()) {
        LOG7("[ub] #{} -> {} (during inst of {}): {} ({})", i.first,
             i.second->toString(0), type->toString(), srcInfo, activate);
        // if (i.second->toString() == "?262.0")
        // assert(1);
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
  return instantiate(srcInfo, root, g);
}

shared_ptr<TypeContext> TypeContext::getContext(const string &argv0,
                                                const string &file) {
  auto realizations = make_shared<RealizationContext>();
  auto imports = make_shared<ImportContext>(argv0);

  auto stdlibPath = imports->getImportFile("core", "", true);
  if (stdlibPath == "")
    error("cannot load standard library");
  auto stdlib = make_shared<TypeContext>(stdlibPath, realizations, imports);
  imports->addImport("", stdlibPath, stdlib);

  unordered_map<string, seq::types::Type *> podTypes = {{"void", seq::types::Void},
                                                        {"bool", seq::types::Bool},
                                                        {"byte", seq::types::Byte},
                                                        {"int", seq::types::Int},
                                                        {"float", seq::types::Float}};
  for (auto &t : podTypes) {
    auto name = t.first;
    auto typ = make_shared<types::ClassType>(name, true);
    realizations->moduleNames[name] = 1;
    realizations->classRealizations[name][name] = {name, typ, {}, t.second};
    stdlib->addType(name, typ);
    stdlib->addType("." + name, typ);
  }
  vector<string> genericTypes = {"Ptr", "Generator", "Optional"};
  for (auto &t : genericTypes) {
    auto typ = make_shared<types::ClassType>(
        t, true, vector<types::TypePtr>(),
        vector<types::Generic>{
            {"T",
             make_shared<types::LinkType>(types::LinkType::Generic,
                                          realizations->unboundCount),
             realizations->unboundCount}});
    realizations->moduleNames[t] = 1;
    stdlib->addType(t, typ);
    stdlib->addType("." + t, typ);
    realizations->unboundCount++;
  }
  genericTypes = {"Int", "UInt"};
  for (auto &t : genericTypes) {
    auto typ = make_shared<types::ClassType>(
        t, true, vector<types::TypePtr>(),
        vector<types::Generic>{
            {"N",
             make_shared<types::LinkType>(types::LinkType::Generic,
                                          realizations->unboundCount, 0, nullptr, true),
             realizations->unboundCount}});
    realizations->moduleNames[t] = 1;
    stdlib->addType(t, typ);
    stdlib->addType("." + t, typ);
    realizations->unboundCount++;
  }

  stdlib->setFlag("internal");
  assert(stdlibPath.substr(stdlibPath.size() - 12) == "__init__.seq");
  auto internal = stdlibPath.substr(0, stdlibPath.size() - 12) + "__internal__.seq";
  stdlib->filename = internal;
  auto stmts = parseFile(internal);
  stdlib->filename = stdlibPath;

  imports->setBody("", make_unique<SuiteStmt>());
  SuiteStmt *tv = static_cast<SuiteStmt *>(imports->getImport("")->statements.get());
  auto t1 = TransformVisitor(stdlib).realizeBlock(stmts.get(), true);
  tv->stmts.push_back(move(t1));
  stdlib->unsetFlag("internal");
  stdlib->addVar("__argv__", make_shared<types::LinkType>(stdlib->instantiateGeneric(
                                 SrcInfo(), stdlib->find("Array")->getType(),
                                 {stdlib->find("str")->getType()})));

  stmts = parseFile(stdlibPath);

  auto t2 = TransformVisitor(stdlib).realizeBlock(stmts.get(), true);
  tv->stmts.push_back(move(t2));
  auto ctx = make_shared<TypeContext>(file, realizations, imports);
  imports->addImport(file, file, ctx);

  LOG7("----------------------------------------------------------------------");
  return ctx;
}

void TypeContext::dump(int pad) {
  auto ordered = std::map<string, decltype(map)::mapped_type>(map.begin(), map.end());
  LOG("base: {}", getBase());
  for (auto &i : ordered) {
    std::string s;
    auto t = i.second.front();
    LOG("{}{:.<25} {} {}", string(pad * 2, ' '), i.first, t->getType()->toString(true),
        t->getBase());
  }
}

} // namespace ast
} // namespace seq
