#include <libgen.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/codegen.h"
#include "parser/ast/format.h"
#include "parser/ast/transform.h"
#include "parser/common.h"
#include "parser/context.h"
#include "parser/ocaml.h"

using fmt::format;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

namespace seq {
namespace ast {

const seq::BaseFunc *ContextItem::getBase() const { return base; }
bool ContextItem::isGlobal() const { return global; }
bool ContextItem::hasAttr(const string &s) const {
  return attributes.find(s) != attributes.end();
};

ContextItem::ContextItem(seq::BaseFunc *base, bool global)
    : base(base), global(global) {}
VarContextItem::VarContextItem(seq::Var *var, seq::BaseFunc *base, bool global)
    : ContextItem(base, global), var(var) {}
seq::Expr *VarContextItem::getExpr() const { return new seq::VarExpr(var); }
seq::Var *VarContextItem::getVar() const { return var; }

FuncContextItem::FuncContextItem(seq::Func *func, vector<string> names,
                                 seq::BaseFunc *base, bool global)
    : ContextItem(base, global), func(func), names(names) {}
seq::Expr *FuncContextItem::getExpr() const { return new seq::FuncExpr(func); }

TypeContextItem::TypeContextItem(seq::types::Type *type, seq::BaseFunc *base,
                                 bool global)
    : ContextItem(base, global), type(type) {}
seq::Expr *TypeContextItem::getExpr() const { return new seq::TypeExpr(type); }
seq::types::Type *TypeContextItem::getType() const { return type; }

ImportContextItem::ImportContextItem(const string &import, seq::BaseFunc *base,
                                     bool global)
    : ContextItem(base, global), import(import) {}
string ImportContextItem::getFile() const { return import; }
seq::Expr *ImportContextItem::getExpr() const {
  error("cannot use import item here");
  return nullptr;
}

Context::Context(shared_ptr<ImportCache> cache, seq::Block *block,
                 seq::BaseFunc *base, seq::SeqJIT *jit,
                 const std::string &filename)
    : cache(cache), filename(filename), jit(jit), enclosingType(nullptr),
      tryCatch(nullptr) {
  stack.push(vector<string>());
  topBaseIndex = topBlockIndex = 0;
  if (block) {
    addBlock(block, base);
  }
}

void Context::loadStdlib(seq::Var *argVar) {
  filename = cache->getImportFile("core", "", true);
  if (filename == "") {
    throw seq::exc::SeqException("cannot load standard library");
  }
  vector<pair<string, seq::types::Type *>> pods = {
      {"void", seq::types::Void},   {"bool", seq::types::Bool},
      {"byte", seq::types::Byte},   {"int", seq::types::Int},
      {"float", seq::types::Float}, {"str", seq::types::Str},
      {"seq", seq::types::Seq}};
  for (auto &i : pods) {
    add(i.first, i.second);
  }
  if (argVar) {
    add("__argv__", argVar);
  }
  cache->stdlib = this;
  auto tv = TransformStmtVisitor().transform(parse_file(filename));
  CodegenStmtVisitor(*this).transform(tv);
}

shared_ptr<ContextItem> Context::find(const string &name,
                                      bool onlyLocal) const {
  auto i = VTable<ContextItem>::find(name);
  if (i && dynamic_cast<VarContextItem *>(i.get())) {
    if (onlyLocal) {
      return (getBase() == i->getBase()) ? i : nullptr;
    } else {
      return i;
    }
  } else if (i) {
    return i;
  } else if (cache->stdlib != nullptr && this != cache->stdlib) {
    return cache->stdlib->find(name);
  } else {
    return nullptr;
  }
}

seq::BaseFunc *Context::getBase() const { return bases[topBaseIndex]; }
seq::types::Type *Context::getType(const string &name) const {
  if (auto i = find(name)) {
    if (auto t = dynamic_cast<TypeContextItem *>(i.get())) {
      return t->getType();
    }
  }
  error("cannot find type '{}'", name);
  return nullptr;
}
seq::Block *Context::getBlock() const { return blocks[topBlockIndex]; }
seq::TryCatch *Context::getTryCatch() const { return tryCatch; }
void Context::setTryCatch(seq::TryCatch *t) { tryCatch = t; }

seq::types::Type *Context::getEnclosingType() { return enclosingType; }

void Context::setEnclosingType(seq::types::Type *t) { enclosingType = t; }

bool Context::isToplevel() const { return bases.size() == 1; }

void Context::addBlock(seq::Block *newBlock, seq::BaseFunc *newBase) {
  VTable<ContextItem>::addBlock();
  if (newBlock) {
    topBlockIndex = blocks.size();
  }
  blocks.push_back(newBlock);
  if (newBase) {
    topBaseIndex = bases.size();
  }
  bases.push_back(newBase);
}

void Context::popBlock() {
  bases.pop_back();
  topBaseIndex = bases.size() - 1;
  while (topBaseIndex && !bases[topBaseIndex])
    topBaseIndex--;
  blocks.pop_back();
  topBlockIndex = blocks.size() - 1;
  while (topBlockIndex && !blocks[topBlockIndex])
    topBlockIndex--;
  VTable<ContextItem>::popBlock();
}

void Context::add(const string &name, shared_ptr<ContextItem> var) {
  VTable<ContextItem>::add(name, var);
}

void Context::add(const string &name, seq::Var *v, bool global) {
  VTable<ContextItem>::add(
      name, make_shared<VarContextItem>(v, getBase(), global || isToplevel()));
}

void Context::add(const string &name, seq::types::Type *t, bool global) {
  VTable<ContextItem>::add(
      name, make_shared<TypeContextItem>(t, getBase(), global || isToplevel()));
}

void Context::add(const string &name, seq::Func *f, vector<string> names,
                  bool global) {
  // fmt::print("adding... {} {} \n", name, isToplevel());
  VTable<ContextItem>::add(
      name, make_shared<FuncContextItem>(f, names, getBase(),
                                         global || isToplevel()));
}

void Context::add(const string &name, const string &import, bool global) {
  VTable<ContextItem>::add(
      name, make_shared<ImportContextItem>(import, getBase(),
                                           global || isToplevel()));
}

string Context::getFilename() const { return filename; }

string ImportCache::getImportFile(const string &what, const string &relativeTo,
                                  bool forceStdlib) {
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
  // for (auto &x: paths) DBG("-- {}", x);
  for (auto &p : paths) {
    struct stat buffer;
    if (!stat(p.c_str(), &buffer)) {
      return p;
    }
  }
  return "";
}

shared_ptr<Context> Context::importFile(const string &file) {
  auto i = cache->imports.find(file);
  if (i != cache->imports.end()) {
    return i->second;
  } else {
    auto stmts = parse_file(file);
    auto tv = TransformStmtVisitor().transform(parse_file(file));

    // Import into the root module
    auto block = blocks[0];
    auto base = bases[0];
    auto context =
        make_shared<Context>(cache, block, base, getJIT(), file);
    CodegenStmtVisitor(*context).transform(tv);
    return (cache->imports[file] = context);
  }
}

shared_ptr<ImportCache> Context::getCache() { return cache; }

void Context::initJIT() {
  jit = new seq::SeqJIT();
  auto fn = new seq::Func();
  fn->setName("$jit_0");

  addBlock(fn->getBlock(), fn);
  assert(topBaseIndex == topBlockIndex && topBlockIndex == 0);

  loadStdlib();
  execJIT();
}

seq::SeqJIT *Context::getJIT() { return jit; }

void Context::execJIT(string varName, seq::Expr *varExpr) {
  static int counter = 0;

  assert(jit != nullptr);
  assert(bases.size() == 1);
  jit->addFunc((seq::Func *)bases[0]);

  vector<pair<string, shared_ptr<seq::ast::ContextItem>>> items;
  for (auto &name : stack.top()) {
    auto i = find(name);
    if (i && i->isGlobal()) {
      items.push_back(make_pair(name, i));
    }
  }
  popBlock();
  for (auto &i : items) {
    add(i.first, i.second);
  }
  if (varExpr) {
    auto var = jit->addVar(varExpr);
    add(varName, var);
  }

  // Set up new block

  auto fn = new seq::Func();
  fn->setName(format("$jit_{}", ++counter));
  addBlock(fn->getBlock(), fn);
  assert(topBaseIndex == topBlockIndex && topBlockIndex == 0);
}

} // namespace ast
} // namespace seq
