#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/transform.h"
#include "parser/ocaml.h"

using fmt::format;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::stack;
using std::string;
using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace seq {
namespace ast {

TContextItem::TContextItem(TypePtr t, bool isType, bool global)
    : type(t), typeVar(isType), global(global) {}
bool TContextItem::isType() const { return typeVar; }
bool TContextItem::isGlobal() const { return global; }
TypePtr TContextItem::getType() const { return type; }
bool TContextItem::hasAttr(const std::string &s) const {
  return attributes.find(s) != attributes.end();
}

TypeContext::TypeContext(const std::string &filename)
    : filename(filename), module(""), prefix(""), level(0), unboundCount(0),
      returnType(nullptr), hasSetReturnType(false) {
  stack.push(vector<string>());
  unordered_map<string, types::Type *> podTypes = {
      {"void", seq::types::Void},
      {"bool", seq::types::Bool},
      {"byte", seq::types::Byte},
      {"int", seq::types::Int},
      {"float", seq::types::Float}};

  for (auto &t : podTypes) {
    auto name = t.first;
    auto typ = make_shared<ClassType>(name, true, vector<pair<int, TypePtr>>(),
                                      vector<pair<string, TypePtr>>());
    internals[name] = typ;
    moduleNames[name] = 1;
    classRealizations[name][name] = {typ, t.second};
  }

  /// TODO: special handle function tuple
  vector<string> genericTypes = {"ptr", "generator", "optional"};
  for (auto &t : genericTypes) {
    internals[t] = make_shared<ClassType>(
        t, true,
        vector<pair<int, TypePtr>>{
            {unboundCount,
             make_shared<LinkType>(LinkType::Generic, unboundCount)}},
        vector<pair<string, TypePtr>>());
    moduleNames[t] = 1;
    unboundCount++;
  }

  setFlag("internal");
  auto stmts = ast::parse_file("stdlib/__root__.seq");
  auto tv = ast::TransformVisitor(*this).transform(stmts.get());
  unsetFlag("internal");

  internals["str"] = find("str")->getType();
  classRealizations["str"]["str"] = {
      dynamic_pointer_cast<ClassType>(internals["str"]), seq::types::Str};
  internals["seq"] = find("seq")->getType();
  classRealizations["seq"]["seq"] = {
      dynamic_pointer_cast<ClassType>(internals["seq"]), seq::types::Seq};

  // Mark as as internal
  // for (auto &a: funcASTs)
  //   a.second.second.attributes.add("internal");

  // for (auto &t : podTypes) {
  //   DBG("{} :: {}", t.first, t.second->getName());
  //   t.second->initFields();
  //   t.second->initOps();

  //   for (auto &m : t.second->getVTable().magic) {
  //     bool valid = true;
  //     vector<pair<string, TypePtr>> args{
  //         {"self", classRealizations[t.first][t.first].type}};
  //     for (int i = 0; i < m.args.size(); i++) {
  //       auto n = m.args[i]->getName();
  //       if (podTypes.find(n) == podTypes.end()) {
  //         valid = false;
  //         // DBG("canceling {}.{}", t.first, m.name);
  //         break;
  //       } else {
  //         args.push_back({format("a{}", i), classRealizations[n][n].type});
  //       }
  //     }
  //     if (!valid)
  //       continue;

  //     TypePtr ret = nullptr;
  //     auto n = m.out->getName();
  //     if (podTypes.find(n) == podTypes.end())
  //       continue;
  //     else
  //       ret = classRealizations[n][n].type;
  //     auto fval =
  //         make_shared<FuncType>(format("{}.{}", t.first, m.name),
  //                               vector<pair<int, TypePtr>>(), args, ret);
  //     // fval->setImplicits(genericTypes);
  //     add(fval->name, fval);
  //     classes[t.first].methods[m.name] = fval;
  //     DBG("magic {} :- {}", fval->name, fval->toString());
  //     funcRealizations[fval->name][fval->toString(true)] = {
  //         fval, nullptr, nullptr}; // TODO get it
  //     DBG("add {} :: {} :- {}", t.first, m.name, *fval);
  //   }
  // }
}

shared_ptr<TContextItem> TypeContext::find(const std::string &name) const {
  auto t = VTable<TContextItem>::find(name);
  if (t)
    return t;
  auto it = internals.find(name);
  if (it != internals.end())
    return make_shared<TContextItem>(it->second, true, true);
  return nullptr;
}

TypePtr TypeContext::findInternal(const std::string &name) const {
  auto it = internals.find(name);
  if (it != internals.end())
    return it->second;
  return nullptr;
}

void TypeContext::add(const string &name, TypePtr t, bool isType, bool global) {
  VTable<TContextItem>::add(name, make_shared<TContextItem>(t, isType, global));
}

void TypeContext::increaseLevel() { level++; }

void TypeContext::decreaseLevel() { level--; }

string TypeContext::getCanonicalName(const SrcInfo &info) {
  auto it = canonicalNames.find(info);
  if (it != canonicalNames.end()) {
    return it->second;
  }
  return "";
}

string TypeContext::generateCanonicalName(const SrcInfo &info,
                                          const std::string &name) {
  auto it = canonicalNames.find(info);
  if (it != canonicalNames.end())
    return it->second;

  auto &num = moduleNames[name];
  auto newName = (module == "" ? "" : module + ".");
  newName += name;
  newName += (num ? format(".{}", num) : "");
  num++;
  canonicalNames[info] = newName;
  return newName;
}

shared_ptr<LinkType> TypeContext::addUnbound(const SrcInfo &srcInfo,
                                             bool setActive) {
  auto t = make_shared<LinkType>(LinkType::Unbound, unboundCount, level);
  t->setSrcInfo(srcInfo);
  if (setActive) {
    activeUnbounds.insert(t);
    DBG("UNBOUND {} ADDED # {} ", t, srcInfo.line);
  }
  unboundCount++;
  return t;
}

TypePtr TypeContext::instantiate(const SrcInfo &srcInfo, TypePtr type) {
  return instantiate(srcInfo, type, vector<pair<int, TypePtr>>());
}

TypeContext::ClassBody *TypeContext::findClass(const std::string &name) {
  auto m = classes.find(name);
  if (m != classes.end())
    return &m->second;
  return nullptr;
}

shared_ptr<Stmt> TypeContext::getAST(const string &name) const {
  auto m = funcASTs.find(name);
  if (m != funcASTs.end())
    return m->second.second;
  auto mx = classASTs.find(name);
  if (mx != classASTs.end())
    return mx->second.second;
  return nullptr;
}

TypePtr TypeContext::instantiate(const SrcInfo &srcInfo, TypePtr type,
                                 const vector<pair<int, TypePtr>> &generics) {
  std::unordered_map<int, TypePtr> cache;
  for (auto &g : generics) {
    cache[g.first] = g.second;
  }
  auto t = type->instantiate(level, unboundCount, cache);
  for (auto &i : cache) {
    if (auto l = dynamic_pointer_cast<LinkType>(i.second)) {
      if (l->kind != LinkType::Unbound)
        continue;
      i.second->setSrcInfo(srcInfo);
      if (activeUnbounds.find(i.second) == activeUnbounds.end()) {
        DBG("UNBOUND {} ADDED # {} ",
            dynamic_pointer_cast<LinkType>(i.second)->id, srcInfo.line);
        activeUnbounds.insert(i.second);
      }
    }
  }
  return t;
}

FuncTypePtr TypeContext::findMethod(const string &name,
                                    const string &method) const {
  auto m = classes.find(name);
  if (m != classes.end()) {
    auto t = m->second.methods.find(method);
    if (t != m->second.methods.end()) {
      return t->second;
    }
  }
  return nullptr;
}

TypePtr TypeContext::findMember(const string &name,
                                const string &member) const {
  auto m = classes.find(name);
  if (m != classes.end()) {
    auto t = m->second.members.find(member);
    if (t != m->second.members.end()) {
      return t->second;
    }
  }
  return nullptr;
}

vector<TypeContext::ClassRealization>
TypeContext::getClassRealizations(const std::string &name) {
  vector<TypeContext::ClassRealization> result;
  for (auto &i : classRealizations[name])
    result.push_back(i.second);
  return result;
}

vector<TypeContext::FuncRealization>
TypeContext::getFuncRealizations(const std::string &name) {
  vector<TypeContext::FuncRealization> result;
  for (auto &i : funcRealizations[name])
    result.push_back(i.second);
  return result;
}

} // namespace ast
} // namespace seq