/*
 * doc.cpp --- Seq documentation generator.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/peg/peg.h"
#include "parser/visitors/doc/doc.h"
#include "parser/visitors/format/format.h"

using fmt::format;
using nlohmann::json;
using std::get;
using std::move;
using std::ostream;
using std::stack;
using std::to_string;

namespace seq {
namespace ast {

json DocVisitor::apply(const string &argv0, const vector<string> &files) {
  auto shared = make_shared<DocShared>();
  shared->argv0 = argv0;
  shared->cache = make_shared<ast::Cache>(argv0);

  auto stdlib = getImportFile(argv0, "internal", "", true, "");
  auto ast = ast::parseFile(shared->cache, stdlib->path);
  shared->modules[""] = make_shared<DocContext>(shared);
  shared->modules[""]->setFilename(stdlib->path);
  for (auto &s : vector<string>{"void", "byte", "float", "bool", "int", "str", "pyobj",
                                "Ptr", "Function", "Generator", "Tuple", "Int", "UInt",
                                TYPE_OPTIONAL, "Callable"}) {
    shared->j[to_string(shared->itemID)] = {
        {"kind", "class"}, {"name", s}, {"type", "type"}};
    shared->modules[""]->add(s, make_shared<int>(shared->itemID++));
  }

  DocVisitor(shared->modules[""]).transformModule(move(ast));
  auto ctx = make_shared<DocContext>(shared);

  char abs[PATH_MAX];
  for (auto &f : files) {
    realpath(f.c_str(), abs);
    ctx->setFilename(abs);
    ast = ast::parseFile(shared->cache, abs);
    DocVisitor(ctx).transformModule(move(ast));
  }

  return shared->j;
}

shared_ptr<int> DocContext::find(const string &s) const {
  auto i = Context<int>::find(s);
  if (!i && this != shared->modules[""].get())
    return shared->modules[""]->find(s);
  return i;
}

string getDocstr(const StmtPtr &s) {
  if (auto se = s->getExpr())
    if (auto e = se->expr->getString())
      return e->getValue();
  return "";
}

vector<StmtPtr> DocVisitor::flatten(StmtPtr stmt, string *docstr, bool deep) {
  vector<StmtPtr> stmts;
  if (auto s = const_cast<SuiteStmt *>(stmt->getSuite())) {
    for (int i = 0; i < (deep ? s->stmts.size() : 1); i++) {
      for (auto &x : flatten(move(s->stmts[i]), i ? nullptr : docstr, deep))
        stmts.push_back(move(x));
    }
  } else {
    if (docstr)
      *docstr = getDocstr(stmt);
    stmts.push_back(move(stmt));
  }
  return stmts;
}

json DocVisitor::transform(const ExprPtr &expr) {
  DocVisitor v(ctx);
  expr->accept(v);
  return v.resultExpr;
}

string DocVisitor::transform(const StmtPtr &stmt) {
  DocVisitor v(ctx);
  stmt->accept(v);
  return v.resultStmt;
}

void DocVisitor::transformModule(StmtPtr stmt) {
  vector<string> children;
  string docstr;

  auto flat = flatten(move(stmt), &docstr);
  for (int i = 0; i < flat.size(); i++) {
    auto &s = flat[i];
    auto id = transform(s);
    if (id.empty())
      continue;
    if (i < (flat.size() - 1) && CAST(s, AssignStmt)) {
      auto ds = getDocstr(flat[i + 1]);
      if (!ds.empty())
        ctx->shared->j[id]["doc"] = ds;
    }
    children.push_back(id);
  }

  int id = ctx->shared->itemID++;
  ctx->shared->j[to_string(id)] = {
      {"kind", "module"}, {"path", ctx->getFilename()}, {"children", children}};
  if (!docstr.empty())
    ctx->shared->j[to_string(id)]["doc"] = docstr;
}

void DocVisitor::visit(IntExpr *expr) { resultExpr = expr->value; }

void DocVisitor::visit(IdExpr *expr) {
  auto i = ctx->find(expr->value);
  if (!i)
    error("unknown identifier {}", expr->value);
  if (*i == 0)
    resultExpr = expr->value;
  else
    resultExpr = to_string(*i);
}

void DocVisitor::visit(IndexExpr *expr) {
  vector<json> v;
  v.push_back(transform(expr->expr));
  if (auto tp = CAST(expr->index, TupleExpr)) {
    if (auto l = tp->items[0]->getList()) {
      for (auto &e : l->items)
        v.push_back(transform(e));
      v.push_back(transform(tp->items[1]));
    } else
      for (auto &e : tp->items)
        v.push_back(transform(e));
  } else {
    v.push_back(transform(expr->index));
  }
  resultExpr = json(v);
}

bool isValidName(const string &s) {
  if (s.empty())
    return false;
  if (s.size() > 4 && s.substr(0, 2) == "__" && s.substr(s.size() - 2) == "__")
    return true;
  return s[0] != '_';
}

void DocVisitor::visit(FunctionStmt *stmt) {
  int id = ctx->shared->itemID++;
  ctx->add(stmt->name, make_shared<int>(id));
  json j{
      {"kind", "function"}, {"pos", jsonify(stmt->getSrcInfo())}, {"name", stmt->name}};

  vector<json> args;
  vector<string> generics;
  for (auto &g : stmt->generics) {
    ctx->add(g.name, make_shared<int>(0));
    generics.push_back(g.name);
  }
  for (auto &a : stmt->args) {
    json j;
    j["name"] = a.name;
    if (a.type)
      j["type"] = transform(a.type);
    if (a.deflt) {
      j["default"] = FormatVisitor::apply(a.deflt);
    }
    args.push_back(j);
  }
  j["generics"] = generics;
  bool isLLVM = false;
  for (auto &d : stmt->decorators)
    if (auto e = d->getId()) {
      j["attrs"][e->value] = "";
      isLLVM |= (e->value == "llvm");
    }
  if (stmt->ret)
    j["return"] = transform(stmt->ret);
  j["args"] = args;
  string docstr;
  flatten(move(const_cast<FunctionStmt *>(stmt)->suite), &docstr);
  for (auto &g : stmt->generics)
    ctx->remove(g.name);
  if (!docstr.empty() && !isLLVM)
    j["doc"] = docstr;
  ctx->shared->j[to_string(id)] = j;
  resultStmt = to_string(id);
}

void DocVisitor::visit(ClassStmt *stmt) {
  int id = ctx->shared->itemID++;
  ctx->add(stmt->name, make_shared<int>(id));
  json j{{"name", stmt->name},
         {"kind", "class"},
         {"type", stmt->isRecord() ? "type" : "class"}};

  vector<json> args;
  vector<string> generics;
  for (auto &g : stmt->generics) {
    ctx->add(g.name, make_shared<int>(0));
    generics.push_back(g.name);
  }
  for (auto &a : stmt->args) {
    json ja;
    ja["name"] = a.name;
    if (a.type)
      ja["type"] = transform(a.type);
    args.push_back(ja);
  }
  j["generics"] = generics;
  j["args"] = args;
  j["pos"] = jsonify(stmt->getSrcInfo());

  bool isExtend = false;
  for (auto &d : stmt->decorators)
    if (auto e = d->getId())
      isExtend |= (e->value == "extend");
  if (isExtend) {
    j["type"] = "extension";
    auto i = ctx->find(stmt->name);
    j["parent"] = to_string(*i);
  }

  string docstr;
  vector<string> members;
  for (auto &f : flatten(move(const_cast<ClassStmt *>(stmt)->suite), &docstr)) {
    if (auto ff = CAST(f, FunctionStmt)) {
      auto i = transform(f);
      if (i != "")
        members.push_back(i);
      if (isValidName(ff->name))
        ctx->remove(ff->name);
    }
  }
  for (auto &g : stmt->generics)
    ctx->remove(g.name);
  j["members"] = members;
  if (!docstr.empty())
    j["doc"] = docstr;
  ctx->shared->j[to_string(id)] = j;
  resultStmt = to_string(id);
}

json DocVisitor::jsonify(const seq::SrcInfo &s) {
  json j{s.line, s.len};
  // j["file"] = s.file;
  // j["line"] = s.line;
  // j["col"] = s.col;
  return j;
}

void DocVisitor::visit(ImportStmt *stmt) {
  if (stmt->from->isId("C") || stmt->from->isId("python")) {
    int id = ctx->shared->itemID++;
    string name, lib;
    if (auto i = stmt->what->getId())
      name = i->value;
    else if (auto d = stmt->what->getDot())
      name = d->member, lib = FormatVisitor::apply(d->expr);
    else
      seqassert(false, "invalid C import statement");
    ctx->add(name, make_shared<int>(id));
    name = stmt->as.empty() ? name : stmt->as;

    json j{{"name", name},
           {"kind", "function"},
           {"pos", jsonify(stmt->getSrcInfo())},
           {"extern", stmt->from->getId()->value}};
    vector<json> args;
    if (stmt->ret)
      j["return"] = transform(stmt->ret);
    for (auto &a : stmt->args) {
      json ja;
      ja["name"] = a.name;
      ja["type"] = transform(a.type);
      args.push_back(ja);
    }
    j["dylib"] = lib;
    j["args"] = args;
    ctx->shared->j[to_string(id)] = j;
    resultStmt = to_string(id);
    return;
  }

  vector<string> dirs; // Path components
  Expr *e = stmt->from.get();
  while (auto d = e->getDot()) {
    dirs.push_back(d->member);
    e = d->expr.get();
  }
  if (!e->getId() || !stmt->args.empty() || stmt->ret ||
      (stmt->what && !stmt->what->getId()))
    error("invalid import statement");
  // We have an empty stmt->from in "from .. import".
  if (!e->getId()->value.empty())
    dirs.push_back(e->getId()->value);
  // Handle dots (e.g. .. in from ..m import x).
  seqassert(stmt->dots >= 0, "negative dots in ImportStmt");
  for (int i = 0; i < stmt->dots - 1; i++)
    dirs.emplace_back("..");
  string path;
  for (int i = int(dirs.size()) - 1; i >= 0; i--)
    path += dirs[i] + (i ? "/" : "");
  // Fetch the import!
  auto file = getImportFile(ctx->shared->argv0, path, ctx->getFilename());
  if (!file)
    error(stmt, "cannot locate import '{}'", path);

  auto ictx = ctx;
  auto it = ctx->shared->modules.find(file->path);
  if (it == ctx->shared->modules.end()) {
    ictx = make_shared<DocContext>(ctx->shared);
    ictx->setFilename(file->path);
    auto tmp = parseFile(ctx->shared->cache, file->path);
    DocVisitor(ictx).transformModule(move(tmp));
  } else {
    ictx = it->second;
  }

  if (!stmt->what) {
    // TODO: implement this corner case
  } else if (stmt->what->isId("*")) {
    for (auto &i : *ictx)
      ctx->add(i.first, i.second[0].second);
  } else {
    auto i = stmt->what->getId();
    if (auto c = ictx->find(i->value))
      ctx->add(stmt->as.empty() ? i->value : stmt->as, c);
    else
      error(stmt, "symbol '{}' not found in {}", i->value, file->path);
  }
}

void DocVisitor::visit(AssignStmt *stmt) {
  auto e = CAST(stmt->lhs, IdExpr);
  if (!e)
    return;
  int id = ctx->shared->itemID++;
  ctx->add(e->value, make_shared<int>(id));
  json j{
      {"name", e->value}, {"kind", "variable"}, {"pos", jsonify(stmt->getSrcInfo())}};
  ctx->shared->j[to_string(id)] = j;
  resultStmt = to_string(id);
}

} // namespace ast
} // namespace seq
