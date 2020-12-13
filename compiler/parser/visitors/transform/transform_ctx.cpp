#include <libgen.h>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/transform/transform.h"
#include "parser/visitors/transform/transform_ctx.h"

using fmt::format;
using std::dynamic_pointer_cast;
using std::stack;
using std::static_pointer_cast;

namespace seq {
namespace ast {

TransformContext::TransformContext(const string &filename, shared_ptr<Cache> cache)
    : Context<TransformItem>(filename), cache(cache) {
  stack.push_front(vector<string>());
}

TransformContext::~TransformContext() {}

shared_ptr<TransformItem> TransformContext::find(const string &name) const {
  auto t = Context<TransformItem>::find(name);
  if (t)
    return t;
  auto stdlib = cache->imports[""].ctx;
  if (stdlib.get() != this) {
    t = stdlib->find(name);
    if (t)
      return t;
  }

  if (!name.empty() && name[0] == '.') {
    auto t = cache->asts.find(name);
    if (t == cache->asts.end())
      return nullptr;
    else if (CAST(t->second, ClassStmt))
      return make_shared<TransformItem>(TransformItem::Type, "", name, true);
    else if (CAST(t->second, FunctionStmt))
      return make_shared<TransformItem>(TransformItem::Func, "", name, true);
  }
  return nullptr;
}

shared_ptr<TransformItem> TransformContext::add(TransformItem::Kind kind,
                                                const string &name,
                                                const string &canonicalName,
                                                bool global, bool generic, bool stat) {
  auto t =
      make_shared<TransformItem>(kind, getBase(), canonicalName, global, generic, stat);
  add(name, t);
  return t;
}

string TransformContext::generateCanonicalName(const string &name) {
  if (name.size() && name[0] == '.')
    return name;
  string newName = format("{}.{}", getBase(), name);
  auto num = cache->moduleNames[newName]++;
  newName = num ? format("{}.{}", newName, num) : newName;
  newName = newName[0] == '.' ? newName : "." + newName;
  cache->reverseLookup[newName] = name;
  return newName;
}

string TransformContext::getBase() const {
  if (!bases.size())
    return "";
  return bases.back().name;
}

void TransformContext::dump(int pad) {
  auto ordered = std::map<string, decltype(map)::mapped_type>(map.begin(), map.end());
  LOG("base: {}", getBase());
  for (auto &i : ordered) {
    string s;
    auto t = i.second.front().second;
    LOG("{}{:.<25} {} {}", string(pad * 2, ' '), i.first, t->canonicalName,
        t->getBase());
  }
}

string TransformContext::findFile(const string &what, const string &relativeTo,
                                  bool forceStdlib) const {
  vector<string> paths;
  char abs[PATH_MAX + 1];
  if (!forceStdlib) {
    realpath(relativeTo.c_str(), abs);
    auto parent = dirname(abs);
    paths.push_back(format("{}/{}.seq", parent, what));
    paths.push_back(format("{}/{}/__init__.seq", parent, what));
  }
  if (auto c = getenv("SEQ_PATH")) {
    char abs[PATH_MAX];
    realpath(c, abs);
    paths.push_back(format("{}/{}.seq", abs, what));
    paths.push_back(format("{}/{}/__init__.seq", abs, what));
  }
  if (cache->argv0 != "") {
    for (auto loci : {"../lib/seq/stdlib", "../stdlib", "stdlib"}) {
      strncpy(abs, executable_path(cache->argv0.c_str()).c_str(), PATH_MAX);
      auto parent = format("{}/{}", dirname(abs), loci);
      realpath(parent.c_str(), abs);
      paths.push_back(format("{}/{}.seq", abs, what));
      paths.push_back(format("{}/{}/__init__.seq", abs, what));
    }
  }
  for (auto &p : paths) {
    struct stat buffer;
    if (!stat(p.c_str(), &buffer))
      return p;
  }
  return "";
}

} // namespace ast
} // namespace seq
