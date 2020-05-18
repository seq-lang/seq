#include <libgen.h>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/transform.h"
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

/**************************************************************************************/

RealizationContext::RealizationContext() : unboundCount(0) {}

string RealizationContext::getCanonicalName(const SrcInfo &info) const {
  auto it = canonicalNames.find(info);
  // DBG("---- LOOK {}.{}.{}.{}", info.file, info.line, info.col, info.id);
  if (it != canonicalNames.end())
    return it->second;
  assert(false);
  return "";
}

string RealizationContext::generateCanonicalName(const SrcInfo &info,
                                                 const string &module,
                                                 const string &name) {
  auto it = canonicalNames.find(info);
  // DBG("---- QUE {}.{}.{}.{} ", info.file, info.line, info.col, info.id);
  if (it != canonicalNames.end())
    return it->second;

  auto &num = moduleNames[name];
  auto newName = "#" + (module == "" ? "" : module + ".");
  newName += format("{}{}", name, num ? format(".{}", num) : "");
  num++;
  canonicalNames[info] = newName;
  // DBG("---- ADD {}.{}.{}.{} -> {}", info.file, info.line, info.col, info.id,
  // newName);
  return newName;
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
    auto t = m->second.members.find(member);
    if (t != m->second.members.end())
      return t->second;
  }
  return nullptr;
}

shared_ptr<Stmt> RealizationContext::getAST(const string &name) const {
  auto m = funcASTs.find(name);
  if (m != funcASTs.end())
    return m->second.second;
  auto mx = classASTs.find(name);
  if (mx != classASTs.end())
    return mx->second.second;
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

/**************************************************************************************/

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

shared_ptr<TypeContext> ImportContext::getImport(const string &path) const {
  auto i = imports.find(path);
  return i == imports.end() ? nullptr : i->second.ctx;
}

void ImportContext::addImport(const string &name, const string &file,
                              shared_ptr<TypeContext> ctx) {
  imports[name] = {file, ctx, nullptr};
}

void ImportContext::setBody(const string &name, StmtPtr body) {
  imports[name].statements = move(body);
}

/**************************************************************************************/

TItem::TItem(const string &base, bool global) : base(base), global(global) {}

bool TItem::isGlobal() const { return global; }

void TItem::setGlobal() { global = true; }

string TItem::getBase() const { return base; }

bool TItem::hasAttr(const std::string &s) const {
  return attributes.find(s) != attributes.end();
}

/**************************************************************************************/

TypeContext::TypeContext(const std::string &filename,
                         shared_ptr<RealizationContext> realizations,
                         shared_ptr<ImportContext> imports)
    : realizations(realizations), imports(imports), filename(filename),
      module(""), level(0), returnType(nullptr), hasSetReturnType(false) {
  stack.push_front(vector<string>());
}

shared_ptr<TItem> TypeContext::find(const std::string &name,
                                    bool checkStdlib) const {
  auto t = VTable<TItem>::find(name);
  if (t)
    return t;
  auto stdlib = imports->getImport("");
  return checkStdlib ? stdlib->find(name, false) : nullptr;
}

TypePtr TypeContext::findInternal(const string &name) const {
  auto stdlib = imports->getImport("");
  auto t = stdlib->find(name, false);
  assert(t);
  return t->getType();
}

void TypeContext::add(const string &name, TypePtr type, bool global) {
  add(name, make_shared<TVarItem>(type, getBase(), global));
}

void TypeContext::addImport(const string &name, const string &import,
                            bool global) {
  add(name, make_shared<TImportItem>(import, getBase(), global));
}

void TypeContext::addType(const string &name, TypePtr type, bool global) {
  add(name, make_shared<TTypeItem>(type, getBase(), global));
}

void TypeContext::addFunc(const string &name, TypePtr type, bool global) {
  add(name, make_shared<TFuncItem>(type, getBase(), global));
}

void TypeContext::addStatic(const string &name, int value, bool global) {
  add(name, make_shared<TStaticItem>(value, getBase(), global));
}

string TypeContext::getBase() const {
  auto s = format("{}", fmt::join(bases, "."));
  return (s == "" ? "" : s + ".");
}

string TypeContext::getModule() const { return module; }

string TypeContext::getFilename() const { return filename; }

shared_ptr<RealizationContext> TypeContext::getRealizations() const {
  return realizations;
}

shared_ptr<ImportContext> TypeContext::getImports() const { return imports; }

void TypeContext::increaseLevel() { level++; }

void TypeContext::decreaseLevel() { level--; }

shared_ptr<LinkType> TypeContext::addUnbound(const SrcInfo &srcInfo,
                                             bool setActive) {
  auto t = make_shared<LinkType>(LinkType::Unbound,
                                 realizations->getUnboundCount()++, level);
  t->setSrcInfo(srcInfo);
  if (setActive) {
    activeUnbounds.insert(t);
    DBG("UNBOUND {} ADDED # {} ", *t, srcInfo.line);
  }
  return t;
}

TypePtr TypeContext::instantiate(const SrcInfo &srcInfo, TypePtr type) {
  return instantiate(srcInfo, type, nullptr);
}

TypePtr TypeContext::instantiate(const SrcInfo &srcInfo, TypePtr type,
                                 GenericTypePtr generics, bool activate) {
  unordered_map<int, TypePtr> cache;
  if (generics)
    for (auto &g : generics->explicits)
      if (g.type)
        cache[g.id] = g.type;
  auto t = type->instantiate(level, realizations->getUnboundCount(), cache);
  for (auto &i : cache) {
    if (auto l = dynamic_pointer_cast<LinkType>(i.second)) {
      if (l->kind != LinkType::Unbound)
        continue;
      i.second->setSrcInfo(srcInfo);
      if (activate && activeUnbounds.find(i.second) == activeUnbounds.end()) {
        DBG("UNBOUND {} ADDED # {} ",
            dynamic_pointer_cast<LinkType>(i.second)->id, srcInfo.line);
        activeUnbounds.insert(i.second);
      }
    }
  }
  return t;
}

TypePtr TypeContext::instantiateGeneric(const SrcInfo &srcInfo, TypePtr root,
                                        const vector<TypePtr> &generics) {
  auto c = root->getClass();
  assert(c);
  auto g = make_shared<ClassType>(""); // dummy generic type
  if (generics.size() != c->explicits.size())
    error(srcInfo, "generics do not match");
  for (int i = 0; i < c->explicits.size(); i++) {
    assert(c->explicits[i].type);
    g->explicits.push_back(
        GenericType::Generic("", c->explicits[i].id, generics[i]));
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
    auto typ = make_shared<ClassType>(name, true);
    realizations->moduleNames[name] = 1;
    realizations->classRealizations[name][name] = {typ, t.second};
    stdlib->addType(name, typ);
    stdlib->addType("#" + name, typ);
  }
  vector<string> genericTypes = {"ptr", "generator", "optional"};
  for (auto &t : genericTypes) {
    auto typ = make_shared<ClassType>(
        t, true, vector<TypePtr>(),
        make_shared<GenericType>(vector<GenericType::Generic>{
            {"T", realizations->unboundCount,
             make_shared<LinkType>(LinkType::Generic,
                                   realizations->unboundCount)}}));
    realizations->moduleNames[t] = 1;
    stdlib->addType(t, typ);
    stdlib->addType("#" + t, typ);
    realizations->unboundCount++;
  }
  auto tt = make_shared<ClassType>("tuple", true);
  stdlib->addType("tuple", tt);
  stdlib->addType("#tuple", tt);
  auto ft = make_shared<FuncType>();
  stdlib->addType("function", ft);
  stdlib->addType("#function", ft);

  stdlib->setFlag("internal");
  auto stmts = ast::parse_file(stdlibPath);
  auto tv = TransformVisitor(stdlib).realizeBlock(stmts.get(), true);
  stdlib->unsetFlag("internal");
  stdlib->add("#str", stdlib->find("str"));
  stdlib->add("#seq", stdlib->find("seq"));
  stdlib->add("#array", stdlib->find("array"));
  realizations->classRealizations["str"]["str"] = {
      dynamic_pointer_cast<ClassType>(stdlib->find("str")->getType()),
      seq::types::Str};
  realizations->classRealizations["seq"]["seq"] = {
      dynamic_pointer_cast<ClassType>(stdlib->find("seq")->getType()),
      seq::types::Seq};
  stdlib->add("__argv__", make_shared<LinkType>(stdlib->instantiateGeneric(
                              SrcInfo(), stdlib->find("array")->getType(),
                              {stdlib->find("str")->getType()})));
  imports->setBody("", move(tv));
  stdlib->dump();

  // auto stmts = ast::parse_file(file);
  // auto tv = ast::TransformVisitor(stdlib).realizeBlock(stmts.get());

  return make_shared<TypeContext>(file, realizations, imports);
}

ImportContext::Import TypeContext::importFile(const string &what) {
  auto file = imports->getImportFile(what, filename);
  if (file == "")
    return {"", nullptr, nullptr};
  if (auto i = imports->getImport(file))
    return {file, i, nullptr};
  auto stmts = ast::parse_file(file);
  auto ctx = make_shared<TypeContext>(file, realizations, imports);
  // TODO: set nice module name ctx->module = ;
  imports->addImport(file, file, ctx);
  auto tv = TransformVisitor(ctx).transform(parse_file(file));
  imports->setBody(file, move(tv));
  return {file, ctx, move(stmts)};
}

void TypeContext::dump() {
  std::map<string, std::stack<shared_ptr<TItem>>> ordered(map.begin(),
                                                          map.end());
  for (auto &i : ordered) {
    string s;
    auto t = i.second.top();
    if (t->isImport())
      s = format("<imp>");
    else
      s = t->getType()->toString(true);
    DBG("{:.<25} {}", i.first, s);
  }
  // exit(0);
}

/**************************************************************************************/

} // namespace ast
} // namespace seq