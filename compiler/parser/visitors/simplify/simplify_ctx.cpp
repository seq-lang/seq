/*
 * simplify_ctx.cpp --- context for simplification transformation.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "parser/common.h"
#include "parser/visitors/simplify/simplify.h"
#include "parser/visitors/simplify/simplify_ctx.h"

using fmt::format;
using std::dynamic_pointer_cast;
using std::stack;
using std::static_pointer_cast;

namespace seq {
namespace ast {

SimplifyItem::SimplifyItem(Kind k, string base, string canonicalName, bool global,
                           bool stat)
    : kind(k), base(move(base)), canonicalName(move(canonicalName)), global(global),
      staticType(stat) {}

SimplifyContext::SimplifyContext(string filename, shared_ptr<Cache> cache)
    : Context<SimplifyItem>(move(filename)), cache(move(cache)), isStdlibLoading(false),
      extendCount(0), canAssign(true) {}

shared_ptr<SimplifyItem> SimplifyContext::add(SimplifyItem::Kind kind,
                                              const string &name,
                                              const string &canonicalName, bool global,
                                              bool isStatic) {
  seqassert(!canonicalName.empty(), "empty canonical name for '{}'", name);
  auto t = make_shared<SimplifyItem>(kind, getBase(), canonicalName, global, isStatic);
  Context<SimplifyItem>::add(name, t);
  Context<SimplifyItem>::add(canonicalName, t);
  return t;
}

shared_ptr<SimplifyItem> SimplifyContext::find(const string &name) const {
  auto t = Context<SimplifyItem>::find(name);
  if (t)
    return t;
  // Item is not found in the current module. Time to look in the standard library!
  auto stdlib = cache->imports[STDLIB_IMPORT].ctx;
  if (stdlib.get() != this) {
    t = stdlib->find(name);
    if (t)
      return t;
  }
  return nullptr;
}

string SimplifyContext::getBase() const {
  if (bases.empty())
    return "";
  return bases.back().name;
}

string SimplifyContext::generateCanonicalName(const string &name) const {
  if (!name.empty() && name[0] == '.')
    return name;
  string newName = format("{}.{}", getBase(), name);
  auto num = cache->identifierCount[newName]++;
  newName = num ? format("{}.{}", newName, num) : newName;
  newName = newName[0] == '.' ? newName : "." + newName;
  cache->reverseIdentifierLookup[newName] = name;
  return newName;
}

SrcInfo SimplifyContext::generateSrcInfo() const {
  return {FILE_GENERATED, cache->generatedSrcInfoCount, cache->generatedSrcInfoCount++,
          0, 0};
}

void SimplifyContext::dump(int pad) {
  auto ordered = std::map<string, decltype(map)::mapped_type>(map.begin(), map.end());
  LOG("base: {}", getBase());
  for (auto &i : ordered) {
    string s;
    auto t = i.second.front().second;
    LOG("{}{:.<25} {} {}", string(pad * 2, ' '), i.first, t->canonicalName,
        t->getBase());
  }
}

} // namespace ast
} // namespace seq
