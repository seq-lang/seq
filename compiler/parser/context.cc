#include <libgen.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "parser/codegen.h"
#include "parser/common.h"
#include "parser/context.h"
#include "parser/ocaml.h"
#include "parser/transform.h"
#include "seq/seq.h"

using fmt::format;
using std::make_pair;
using std::make_shared;
using std::pair;

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

Context::Context(const string &argv0, seq::SeqModule *module)
    : argv0(argv0), module(module), enclosingType(nullptr),
      tryCatch(nullptr), stdlib(nullptr) {
  filename = getImportFile("core", true);
  if (filename == "") {
    throw seq::exc::SeqException("cannot load standard library");
  }
  module->setFileName(filename);
  stack.push(unordered_set<string>());
  bases.push_back(module);
  blocks.push_back(module->getBlock());
  topBaseIndex = topBlockIndex = 0;

  vector<pair<string, seq::types::Type *>> pods = {
      {"void", seq::types::Void},   {"bool", seq::types::Bool},
      {"byte", seq::types::Byte},   {"int", seq::types::Int},
      {"float", seq::types::Float}, {"str", seq::types::Str},
      {"seq", seq::types::Seq}};
  for (auto &i : pods) {
    add(i.first, i.second);
  }
  add("__argv__", module->getArgVar());

  DBG("loading stdlib from {}...", filename);
  auto stmts = parse_file(filename);
  auto tv = TransformStmtVisitor::apply(move(stmts));
  stdlib = this;
  CodegenStmtVisitor::apply(*this, tv);
}

Context::Context(const string &argv0, seq::SeqModule *module,
                 const string &filename, Context *stdlib)
    : filename(filename), argv0(argv0), module(module), enclosingType(nullptr),
      tryCatch(nullptr), stdlib(stdlib) {
  module->setFileName(filename);
  stack.push(unordered_set<string>());
  bases.push_back(module);
  blocks.push_back(module->getBlock());
  topBaseIndex = topBlockIndex = 0;
}

shared_ptr<ContextItem> Context::find(const string &name) const {
  auto i = VTable<ContextItem>::find(name);
  if (i && (i->isGlobal() || getBase() == i->getBase())) {
    return i;
  } else if (this->stdlib && this != this->stdlib) {
    return stdlib->find(name);
  } else {
    return nullptr;
  }
}

seq::BaseFunc *Context::getBase() const { return bases[topBaseIndex]; }
seq::SeqModule *Context::getModule() const { return module; }
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

seq::types::Type *Context::getEnclosingType() { return enclosingType; }

void Context::setEnclosingType(seq::types::Type *t) { enclosingType = t; }

bool Context::isToplevel() const { return module == getBase(); }

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
  while (!bases[topBaseIndex])
    topBaseIndex--;
  blocks.pop_back();
  topBlockIndex = blocks.size() - 1;
  while (!blocks[topBlockIndex])
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

string Context::getImportFile(const string &what, bool forceStdlib) {
  vector<string> paths;
  char abs[PATH_MAX + 1];
  if (!forceStdlib) {
    realpath(getFilename().c_str(), abs);
    auto parent = dirname(abs);
    paths.push_back(format("{}/{}.seq", parent, what));
    paths.push_back(format("{}/{}/__init__.seq", parent, what));
  }
  strncpy(abs, executable_path(argv0.c_str()).c_str(), PATH_MAX);
  auto parent = format("{}/../stdlib", dirname(abs));
  realpath(parent.c_str(), abs);
  paths.push_back(format("{}/{}.seq", abs, what));
  paths.push_back(format("{}/{}/__init__.seq", abs, what));
  if (auto c = getenv("SEQ_PATH")) {
    char abs[PATH_MAX];
    realpath(c, abs);
    auto parent = dirname(abs);
    paths.push_back(format("{}/{}.seq", parent, what));
    paths.push_back(format("{}/{}/__init__.seq", parent, what));
  }
  for (auto &p : paths) {
    struct stat buffer;
    if (!stat(p.c_str(), &buffer)) {
      return p;
    }
  }
  return "";
}

shared_ptr<Context> Context::importFile(const string &file) {
  DBG("loading {}", file);
  auto i = imports.find(file);
  if (i != imports.end()) {
    return i->second;
  } else {
    auto stmts = parse_file(file);
    auto tv = TransformStmtVisitor::apply(move(stmts));
    auto context = make_shared<Context>(argv0, getModule(), file, stdlib);
    CodegenStmtVisitor::apply(*context, tv);
    return (imports[file] = context);
  }
}
