#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include <memory>
#include <ostream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/ocaml/ocaml.h"
#include "parser/parser.h"
#include "parser/visitors/doc/doc.h"
#include "parser/visitors/format/format.h"

using fmt::format;
using std::get;
using std::move;
using std::ostream;
using std::stack;
using std::to_string;

#define CAST(s, T) dynamic_cast<T *>(s.get())

namespace seq {
namespace ast {

json DocVisitor::apply(const string &argv0, const vector<string> &files) {
  auto shared = make_shared<DocShared>();
  shared->argv0 = argv0;

  auto stdlib = getImportFile(argv0, "core", "", true);
  auto ast = ast::parseFile(stdlib);
  shared->modules[""] = make_shared<DocContext>(shared);
  shared->modules[""]->file = stdlib;
  for (auto &s : vector<string>{"void", "byte", "float", "seq", "bool", "int", "str",
                                "ptr", "function", "generator", "tuple", "array",
                                "Kmer", "Int", "UInt", "optional"}) {
    shared->j[to_string(shared->itemID)] = {
        {"kind", "class"}, {"name", s}, {"type", "type"}};
    shared->modules[""]->add(s, make_shared<int>(shared->itemID++));
  }

  DocVisitor(shared->modules[""]).transformModule(move(ast));
  auto ctx = make_shared<DocContext>(shared);

  char abs[PATH_MAX];
  for (auto &f : files) {
    realpath(f.c_str(), abs);
    ctx->file = abs;
    ast = ast::parseFile(abs);
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
  if (auto se = CAST(s, ExprStmt))
    if (auto e = CAST(se->expr, StringExpr))
      return e->value;
  return "";
}

vector<StmtPtr> DocVisitor::flatten(StmtPtr stmt, string *docstr, bool deep) {
  vector<StmtPtr> stmts;
  if (auto s = CAST(stmt, SuiteStmt)) {
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
    if (id == "")
      continue;
    if (i < (flat.size() - 1) && CAST(s, AssignStmt)) {
      auto s = getDocstr(flat[i + 1]);
      if (!s.empty())
        ctx->shared->j[id]["doc"] = s;
    }
    children.push_back(id);
  }

  int id = ctx->shared->itemID++;
  ctx->shared->j[to_string(id)] = {
      {"kind", "module"}, {"path", ctx->file}, {"children", children}};
  if (docstr.size())
    ctx->shared->j[to_string(id)]["doc"] = docstr;
}

void DocVisitor::visit(const IdExpr *expr) {
  auto i = ctx->find(expr->value);
  if (!i)
    error("unknown identifier {}", expr->value);
  if (*i == 0)
    resultExpr = expr->value;
  else
    resultExpr = to_string(*i);
}

void DocVisitor::visit(const IndexExpr *expr) {
  vector<json> v;
  v.push_back(transform(expr->expr));
  if (auto tp = CAST(expr->index, TupleExpr)) {
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

void DocVisitor::visit(const FunctionStmt *stmt) {
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
  j["attrs"] = stmt->attributes;
  if (stmt->ret)
    j["return"] = transform(stmt->ret);
  j["args"] = args;
  string docstr;
  flatten(move(const_cast<FunctionStmt *>(stmt)->suite), &docstr);
  for (auto &g : stmt->generics)
    ctx->remove(g.name);
  if (docstr.size())
    j["doc"] = docstr;
  ctx->shared->j[to_string(id)] = j;
  resultStmt = to_string(id);

  // {"extern", stmt->lang}};
  // j["dylib"] = bool(stmt->from);
}

void DocVisitor::visit(const ClassStmt *stmt) {
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
    json j;
    j["name"] = a.name;
    if (a.type)
      j["type"] = transform(a.type);
    args.push_back(j);
  }
  j["generics"] = generics;
  j["args"] = args;
  j["pos"] = jsonify(stmt->getSrcInfo());

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
  if (docstr.size())
    j["doc"] = docstr;
  ctx->shared->j[to_string(id)] = j;
  resultStmt = to_string(id);

  // {"type", "extension"}, {"parent", to_string(*i)}
}

json DocVisitor::jsonify(const seq::SrcInfo &s) {
  json j{s.line, s.endLine};
  // j["file"] = s.file;
  // j["line"] = s.line;
  // j["col"] = s.col;
  return j;
}

void DocVisitor::visit(const ImportStmt *stmt) {
  // auto file = getImportFile(ctx->shared->argv0, stmt->from.first, ctx->file, false);
  // if (file == "")
  //   error(stmt, "cannot locate import '{}'", stmt->from.first);

  // auto ictx = ctx;
  // auto it = ctx->shared->modules.find(file);
  // if (it == ctx->shared->modules.end()) {
  //   ictx = make_shared<DocContext>(ctx->shared);
  //   ictx->file = file;
  //   auto tmp = tmp::parseFile(file);
  //   DocVisitor(ictx).transformModule(move(tmp));
  // } else {
  //   ictx = it->second;
  // }

  // if (!stmt->what.size()) {
  //   // TODO
  //   // ctx.add(stmt->from.second == "" ? stmt->from.first : stmt->from.second,
  //   // file);
  // } else if (stmt->what.size() == 1 && stmt->what[0].first == "*") {
  //   for (auto &i : *ictx)
  //     ctx->add(i.first, i.second[0].second);
  // } else
  //   for (auto &w : stmt->what) {
  //     if (auto c = ictx->find(w.first)) {
  //       ctx->add(w.second == "" ? w.first : w.second, c);
  //     } else {
  //       error(stmt, "symbol '{}' not found in {}", w.first, file);
  //     }
  //   }
}

void DocVisitor::visit(const AssignStmt *stmt) {
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
