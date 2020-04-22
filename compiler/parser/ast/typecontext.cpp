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

TypeContext::TypeContext(const std::string &filename)
    : filename(filename), module(""), prefix(""), level(0), unboundCount(0),
      returnType(nullptr), hasSetReturnType(false) {
  // set up internals
  stack.push(vector<string>());
  vector<string> podTypes = {"void", "bool", "int", "float",
                             "byte", "str",  "seq"};
  for (auto &t : podTypes) {
    internals[t] = make_shared<ClassType>(t, t, vector<pair<int, TypePtr>>());
    moduleNames[t] = 1;
  }

  vector<string> genericTypes = {"array",    "ptr",   "generator",
                                 "optional", "tuple", "function",
                                 "Kmer",     "UInt",  "Int"};
  for (auto &t : genericTypes) {
    internals[t] = make_shared<ClassType>(
        t, t,
        vector<pair<int, TypePtr>>{
            {unboundCount,
             make_shared<LinkType>(LinkType::Generic, unboundCount)}});
    unboundCount++;
    moduleNames[t] = 1;
  }

  // handle Kmer / Int / Uiint separately

  /// TODO: array, __array__, ptr, generator, tuple etc
  /// UInt / Kmer
}

TypePtr TypeContext::find(const std::string &name, bool *isType) const {
  auto t = VTable<Type>::find(name);
  if (!t) {
    auto it = internals.find(name);
    if (it != internals.end()) {
      if (isType)
        *isType = true;
      return it->second;
    } else {
      return nullptr;
    }
  } else {
    if (isType) {
      auto it = this->isType.find(name);
      assert(it != this->isType.end());
      *isType = it->second.top();
    }
    return t;
  }
}

TypePtr TypeContext::findInternal(const std::string &name) const {
  auto it = internals.find(name);
  if (it != internals.end()) {
    return it->second;
  }
  return nullptr;
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

string TypeContext::getCanonicalName(const std::string &name,
                                     const seq::SrcInfo &info) {
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
    if (i.second->isUnbound()) {
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

shared_ptr<FuncType> TypeContext::findMethod(shared_ptr<ClassType> type,
                                             const string &method) {
  auto m = classMethods.find(type->getCanonicalName());
  if (m != classMethods.end()) {
    auto t = m->second.find(method);
    if (t != m->second.end()) {
      return t->second;
    }
  }
  return nullptr;
}

TypePtr TypeContext::findMember(shared_ptr<ClassType> type,
                                const string &member) {
  auto m = classMembers.find(type->getCanonicalName());
  if (m != classMembers.end()) {
    auto t = m->second.find(member);
    if (t != m->second.end()) {
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