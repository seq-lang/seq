#include <libgen.h>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/ast/transform.h"
#include "parser/common.h"
#include "parser/ocaml.h"

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

TypeContext::TypeContext(const std::string &filename,
                         shared_ptr<RealizationContext> realizations,
                         shared_ptr<ImportContext> imports)
    : Context<TypeItem::Item>(filename, realizations, imports), module(""),
      level(0), returnType(nullptr), matchType(nullptr),
      wasReturnTypeSet(false) {
  stack.push_front(vector<string>());
}

TypeContext::~TypeContext() {}

shared_ptr<TypeItem::Item> TypeContext::find(const std::string &name,
                                             bool checkStdlib) const {
  auto t = Context<TypeItem::Item>::find(name);
  if (t)
    return t;
  auto stdlib = imports->getImport("")->tctx;
  if (checkStdlib)
    t = stdlib->find(name, false);
  if (t)
    return t;

  auto it = getRealizations()->realizationLookup.find(name);
  if (it != getRealizations()->realizationLookup.end()) {
    auto fit = getRealizations()->funcRealizations.find(it->second);
    if (fit != getRealizations()->funcRealizations.end())
      return make_shared<TypeItem::Func>(fit->second[name].type,
                                         fit->second[name].base);
    auto cit = getRealizations()->classRealizations.find(it->second);
    if (cit != getRealizations()->classRealizations.end())
      return make_shared<TypeItem::Class>(cit->second[name].type,
                                          cit->second[name].base);
  }
  return nullptr;
}

types::TypePtr TypeContext::findInternal(const string &name) const {
  auto stdlib = imports->getImport("")->tctx;
  auto t = stdlib->find(name, false);
  assert(t);
  return t->getType();
}

void TypeContext::addVar(const string &name, types::TypePtr type, bool global) {
  add(name, make_shared<TypeItem::Var>(type, getBase(), global));
}

void TypeContext::addImport(const string &name, const string &import,
                            bool global) {
  add(name, make_shared<TypeItem::Import>(import, getBase(), global));
}

void TypeContext::addType(const string &name, types::TypePtr type,
                          bool global) {
  add(name, make_shared<TypeItem::Class>(type, getBase(), global));
}

void TypeContext::addFunc(const string &name, types::TypePtr type,
                          bool global) {
  add(name, make_shared<TypeItem::Func>(type, getBase(), global));
}

void TypeContext::addStatic(const string &name, int value, bool global) {
  add(name, make_shared<TypeItem::Static>(value, getBase(), global));
}

string TypeContext::getBase() const {
  auto s = format("{}", fmt::join(bases, "."));
  return (s == "" ? "" : s + ".");
}

string TypeContext::getModule() const { return module; }

void TypeContext::increaseLevel() { level++; }

void TypeContext::decreaseLevel() { level--; }

shared_ptr<types::LinkType> TypeContext::addUnbound(const SrcInfo &srcInfo,
                                                    bool setActive) {
  auto t = make_shared<types::LinkType>(
      types::LinkType::Unbound, realizations->getUnboundCount()++, level);
  t->setSrcInfo(srcInfo);
  if (setActive) {
#ifdef TYPE_DEBUG
    DBG("UNBOUND: {} @ {} ", t->toString(0), srcInfo);
#endif
    activeUnbounds.insert(t);
  }
  return t;
}

types::TypePtr TypeContext::instantiate(const SrcInfo &srcInfo,
                                        types::TypePtr type) {
  return instantiate(srcInfo, type, nullptr);
}

types::TypePtr TypeContext::instantiate(const SrcInfo &srcInfo,
                                        types::TypePtr type,
                                        types::ClassTypePtr generics,
                                        bool activate) {
  unordered_map<int, types::TypePtr> cache;
  if (generics)
    for (auto &g : generics->explicits)
      if (g.type)
        cache[g.id] = g.type;
  auto t = type->instantiate(level, realizations->getUnboundCount(), cache);
  for (auto &i : cache) {
    if (auto l = i.second->getLink()) {
      if (l->kind != types::LinkType::Unbound)
        continue;
      i.second->setSrcInfo(srcInfo);
      if (activate && activeUnbounds.find(i.second) == activeUnbounds.end()) {
#ifdef TYPE_DEBUG
        DBG("UNBOUND: {} @ {} (during inst of {})", i.second->toString(0),
            srcInfo, type->toString());
#endif
        // if (dynamic_pointer_cast<types::LinkType>(i.second)->id == 581) {
        // DBG("woho");
        // }
        activeUnbounds.insert(i.second);
      }
    }
  }
  return t;
}

types::TypePtr
TypeContext::instantiateGeneric(const SrcInfo &srcInfo, types::TypePtr root,
                                const vector<types::TypePtr> &generics) {
  auto c = root->getClass();
  assert(c);
  auto g = make_shared<types::ClassType>(""); // dummy generic type
  if (generics.size() != c->explicits.size())
    error(srcInfo, "generics do not match");
  for (int i = 0; i < c->explicits.size(); i++) {
    assert(c->explicits[i].type);
    g->explicits.push_back(
        types::ClassType::Generic("", generics[i], c->explicits[i].id));
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

  unordered_map<string, seq::types::Type *> podTypes = {
      {"void", seq::types::Void},
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
    stdlib->addType("#" + name, typ);
  }
  vector<string> genericTypes = {"ptr", "generator", "optional"};
  for (auto &t : genericTypes) {
    auto typ = make_shared<types::ClassType>(
        t, true, vector<types::TypePtr>(),
        vector<types::ClassType::Generic>{
            {"T",
             make_shared<types::LinkType>(types::LinkType::Generic,
                                          realizations->unboundCount),
             realizations->unboundCount}});
    realizations->moduleNames[t] = 1;
    stdlib->addType(t, typ);
    stdlib->addType("#" + t, typ);
    realizations->unboundCount++;
  }
  genericTypes = {"Int", "UInt"};
  for (auto &t : genericTypes) {
    auto typ = make_shared<types::ClassType>(
        t, true, vector<types::TypePtr>(),
        vector<types::ClassType::Generic>{
            {"N",
             make_shared<types::LinkType>(types::LinkType::Generic,
                                          realizations->unboundCount),
             realizations->unboundCount, true}});
    realizations->moduleNames[t] = 1;
    stdlib->addType(t, typ);
    stdlib->addType("#" + t, typ);
    realizations->unboundCount++;
  }

  stdlib->setFlag("internal");
  assert(stdlibPath.substr(stdlibPath.size() - 12) == "__init__.seq");
  auto stmts = parseFile(stdlibPath.substr(0, stdlibPath.size() - 12) +
                         "__internal__.seq");

  imports->setBody("", make_unique<SuiteStmt>());
  SuiteStmt *tv =
      static_cast<SuiteStmt *>(imports->getImport("")->statements.get());
  auto t1 = TransformVisitor(stdlib).realizeBlock(stmts.get(), true);
  tv->stmts.push_back(move(t1));
  stdlib->unsetFlag("internal");
  stdlib->addVar("__argv__",
                 make_shared<types::LinkType>(stdlib->instantiateGeneric(
                     SrcInfo(), stdlib->find("array")->getType(),
                     {stdlib->find("str")->getType()})));

  stmts = parseFile(stdlibPath);

  auto t2 = TransformVisitor(stdlib).realizeBlock(stmts.get(), true);
  tv->stmts.push_back(move(t2));
  return make_shared<TypeContext>(file, realizations, imports);
}

void TypeContext::dump(int pad) {
  auto ordered =
      std::map<string, decltype(map)::mapped_type>(map.begin(), map.end());
  DBG("base: {}", getBase());
  for (auto &i : ordered) {
    std::string s;
    auto t = i.second.front();
    if (auto im = t->getImport()) {
      DBG("{}{:.<25} {}", string(pad * 2, ' '), i.first, "<import>");
      getImports()->getImport(im->getFile())->tctx->dump(pad + 1);
    } else
      DBG("{}{:.<25} {} {}", string(pad * 2, ' '), i.first,
          t->getType()->toString(true), t->getBase());
  }
}

} // namespace ast
} // namespace seq
