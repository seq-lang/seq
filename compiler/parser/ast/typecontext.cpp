#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/typecontext.h"

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

FuncTContextItem::FuncTContextItem(TypePtr t, const string &name, bool isType,
                                   bool global)
    : TContextItem(t, isType, global), name(name) {}
string FuncTContextItem::getName() const { return name; }

TypeContext::TypeContext(const std::string &filename)
    : filename(filename), module(""), prefix(""), level(0), unboundCount(0),
      returnType(nullptr), hasSetReturnType(false) {
  // set up internals
  stack.push(vector<string>());
  vector<string> podTypes = {"void", "bool", "int", "float",
                             "byte", "str",  "seq"};
  for (auto &t : podTypes) {
    internals[t] = make_shared<ClassType>(t, true, vector<pair<int, TypePtr>>(),
                                          vector<pair<string, TypePtr>>());
    moduleNames[t] = 1;
  }

  vector<string> genericTypes = {"array",    "ptr",   "generator",
                                 "optional", "tuple", "function",
                                 "Kmer",     "UInt",  "Int"};
  for (auto &t : genericTypes) {
    /// TODO: handle Int/UInt record/class status
    internals[t] = make_shared<ClassType>(
        t, false,
        vector<pair<int, TypePtr>>{
            {unboundCount,
             make_shared<LinkType>(LinkType::Generic, unboundCount)}},
        vector<pair<string, TypePtr>>());
    unboundCount++;
    moduleNames[t] = 1;
  }
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

void TypeContext::add(const string &name, const string &canonicalName,
                      FuncTypePtr t, bool global) {
  VTable<TContextItem>::add(
      name, make_shared<FuncTContextItem>(t, canonicalName, false, global));
}

void TypeContext::increaseLevel() { level++; }

void TypeContext::decreaseLevel() { level--; }

string TypeContext::getCanonicalName(const seq::SrcInfo &info) {
  auto it = canonicalNames.find(info);
  if (it != canonicalNames.end()) {
    return it->second;
  }
  return "";
}

string TypeContext::generateCanonicalName(const seq::SrcInfo &info,
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

shared_ptr<LinkType> TypeContext::addUnbound(const seq::SrcInfo &srcInfo,
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

TypePtr TypeContext::instantiate(const seq::SrcInfo &srcInfo, TypePtr type) {
  return instantiate(srcInfo, type, vector<pair<int, TypePtr>>());
}

TypePtr TypeContext::instantiate(const seq::SrcInfo &srcInfo, TypePtr type,
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

FuncHandle TypeContext::findMethod(const string &name,
                                   const string &method) const {
  auto m = classes.find(name);
  if (m != classes.end()) {
    auto t = m->second.methods.find(method);
    if (t != m->second.methods.end()) {
      return t->second;
    }
  }
  return {"", nullptr};
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

vector<pair<string, const FunctionStmt *>>
TypeContext::getRealizations(const FunctionStmt *stmt) {
  vector<pair<string, const FunctionStmt *>> result;
  auto it = canonicalNames.find(stmt->getSrcInfo());
  if (it != canonicalNames.end()) {
    for (auto &i : funcRealizations[it->second]) {
      result.push_back({i.first, i.second.second.get()});
    }
  }
  return result;
}

} // namespace ast
} // namespace seq