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
#include "parser/ast/transform/transform.h"
#include "parser/ast/transform/transform_ctx.h"
#include "parser/common.h"
#include "parser/ocaml.h"

using fmt::format;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::make_unique;
using std::pair;
using std::shared_ptr;
using std::stack;
using std::static_pointer_cast;
using std::string;
using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace seq {
namespace ast {

TransformContext::TransformContext(const std::string &filename, shared_ptr<Cache> cache)
    : Context<TransformItem>(filename), cache(cache) {
  stack.push_front(vector<string>());
}

TransformContext::~TransformContext() {}

shared_ptr<TransformItem> TransformContext::find(const std::string &name) const {
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
  // LOG7("{} / {} ({:x} | {:x})", name, getFilename(), size_t(stdlib.get()),
  //      size_t(this));
  // ((TransformContext *)this)->dump();
  // stdlib->dump();
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
  auto &num = cache->moduleNames[name];
  string newName = format("{}.{}{}", getBase(), name, num ? format(".{}", num) : "");
  num++;
  newName = newName[0] == '.' ? newName : "." + newName;
  cache->reverseLookup[newName] = name;
  return newName;
}

string TransformContext::getBase() const {
  if (!bases.size())
    return "";
  return bases.back().name;
}

pair<shared_ptr<TransformContext>, StmtPtr>
TransformContext::getContext(const string &argv0) {
  auto cache = make_shared<Cache>(argv0);
  auto stdlib = make_shared<TransformContext>("", cache);
  auto stdlibPath = stdlib->findFile("core", "", true);
  if (stdlibPath == "")
    error("cannot load standard library");
  stdlib->setFilename(stdlibPath);
  cache->imports[""] = {stdlibPath, stdlib};

  for (auto &name : {"void", "bool", "byte", "int", "float"}) {
    auto canonical = stdlib->generateCanonicalName(name);
    stdlib->add(TransformItem::Type, name, canonical, true);
    cache->asts[canonical] =
        make_unique<ClassStmt>(true, canonical, vector<Param>(), vector<Param>(),
                               nullptr, vector<string>{"internal"});
  }
  for (auto &name : {"Ptr", "Generator", "Optional", "Int", "UInt"}) {
    auto canonical = stdlib->generateCanonicalName(name);
    stdlib->add(TransformItem::Type, name, canonical, true);
    vector<Param> generics;
    generics.push_back({"T",
                        string(name) == "Int" || string(name) == "UInt"
                            ? make_unique<IdExpr>("int")
                            : nullptr,
                        nullptr});
    cache->asts[canonical] =
        make_unique<ClassStmt>(true, canonical, move(generics), vector<Param>(),
                               nullptr, vector<string>{"internal"});
  }

  // Add preamble for variardic stubs
  auto suite = make_unique<SuiteStmt>();
  suite->stmts.push_back(make_unique<SuiteStmt>());

  // Load __internal__
  stdlib->setFlag("internal");
  assert(stdlibPath.substr(stdlibPath.size() - 12) == "__init__.seq");
  auto internal = stdlibPath.substr(0, stdlibPath.size() - 12) + "__internal__.seq";
  stdlib->filename = internal;
  StmtPtr stmts = parseFile(internal);
  suite->stmts.push_back(TransformVisitor(stdlib).transform(stmts));
  auto canonical = stdlib->generateCanonicalName("__argv__");
  stdlib->add(TransformItem::Var, "__argv__", canonical, true);
  stdlib->unsetFlag("internal");

  // Load stdlib
  stdlib->filename = stdlibPath;
  stmts = parseFile(stdlibPath);
  suite->stmts.push_back(TransformVisitor(stdlib).transform(stmts));

  return {stdlib, move(suite)};
}

void TransformContext::dump(int pad) {
  auto ordered = std::map<string, decltype(map)::mapped_type>(map.begin(), map.end());
  LOG("base: {}", getBase());
  for (auto &i : ordered) {
    std::string s;
    auto t = i.second.front();
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
    if (!stat(p.c_str(), &buffer)) {
      // LOG("getting {}", p);
      return p;
    }
  }
  return "";
}

} // namespace ast
} // namespace seq
