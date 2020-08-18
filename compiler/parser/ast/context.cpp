#include <libgen.h>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/context.h"
#include "parser/common.h"
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

using namespace types;

RealizationContext::RealizationContext() : unboundCount(0), generatedID(0) {}

string RealizationContext::getCanonicalName(const SrcInfo &info) const {
  auto it = canonicalNames.find(info);
  if (it != canonicalNames.end())
    return it->second;
  assert(false);
  return "";
}

string RealizationContext::generateCanonicalName(const SrcInfo &info,
                                                 const string &name) {
  auto it = canonicalNames.find(info);
  if (it != canonicalNames.end())
    return it->second;

  auto &num = moduleNames[name];
  string newName = format("{}{}", name, num ? format(".{}", num) : "");
  num++;
  canonicalNames[info] = (newName[0] == '.' ? newName : "." + newName);
  return canonicalNames[info];
}

int &RealizationContext::getUnboundCount() { return unboundCount; }

RealizationContext::ClassBody *
RealizationContext::findClass(const std::string &name) {
  auto m = classes.find(name);
  if (m != classes.end())
    return &m->second;
  return nullptr;
}

const std::vector<FuncTypePtr> *
RealizationContext::findMethod(const string &name, const string &method) const {
  auto m = classes.find(name);
  if (m != classes.end()) {
    auto t = m->second.methods.find(method);
    if (t != m->second.methods.end())
      return &t->second;
  }
  return nullptr;
}

TypePtr RealizationContext::findMember(const string &name,
                                       const string &member) const {
  auto m = classes.find(name);
  if (m != classes.end()) {
    for (auto &mm : m->second.members)
      if (mm.first == member)
        return mm.second;
  }
  return nullptr;
}

shared_ptr<Stmt> RealizationContext::getAST(const string &name) const {
  auto m = funcASTs.find(name);
  if (m != funcASTs.end())
    return m->second.second;
  return nullptr;
}

vector<RealizationContext::ClassRealization>
RealizationContext::getClassRealizations(const string &name) {
  vector<RealizationContext::ClassRealization> result;
  for (auto &i : classRealizations[name])
    result.push_back(i.second);
  return result;
}

vector<RealizationContext::FuncRealization>
RealizationContext::getFuncRealizations(const string &name) {
  vector<RealizationContext::FuncRealization> result;
  for (auto &i : funcRealizations[name])
    result.push_back(i.second);
  return result;
}

ImportContext::ImportContext(const string &argv0) : argv0(argv0) {}

string ImportContext::getImportFile(const string &what,
                                    const string &relativeTo,
                                    bool forceStdlib) const {
  vector<string> paths;
  char abs[PATH_MAX + 1];
  if (!forceStdlib) {
    realpath(relativeTo.c_str(), abs);
    auto parent = dirname(abs);
    paths.push_back(format("{}/{}.seq", parent, what));
    paths.push_back(format("{}/{}/__init__.seq", parent, what));
  }
  if (argv0 != "") {
    strncpy(abs, executable_path(argv0.c_str()).c_str(), PATH_MAX);
    auto parent = format("{}/../stdlib", dirname(abs));
    realpath(parent.c_str(), abs);
    paths.push_back(format("{}/{}.seq", abs, what));
    paths.push_back(format("{}/{}/__init__.seq", abs, what));
  }
  if (auto c = getenv("SEQ_PATH")) {
    char abs[PATH_MAX];
    realpath(c, abs);
    paths.push_back(format("{}/{}.seq", abs, what));
    paths.push_back(format("{}/{}/__init__.seq", abs, what));
  }
  for (auto &p : paths) {
    struct stat buffer;
    if (!stat(p.c_str(), &buffer))
      return p;
  }
  return "";
}

const ImportContext::Import *
ImportContext::getImport(const string &path) const {
  auto i = imports.find(path);
  return i == imports.end() ? nullptr : &(i->second);
}

void ImportContext::addImport(const string &name, const string &file,
                              shared_ptr<TypeContext> ctx) {
  imports[name] = {file, ctx, nullptr};
}

void ImportContext::setBody(const string &name, StmtPtr body) {
  imports[name].statements = move(body);
}

} // namespace ast
} // namespace seq
