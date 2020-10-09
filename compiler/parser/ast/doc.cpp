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

#include "parser/ast/ast.h"
#include "parser/ast/doc.h"
#include "parser/ast/format.h"
#include "parser/common.h"
#include "parser/context.h"
#include "parser/ocaml.h"
#include "parser/parser.h"

using fmt::format;
using std::get;
using std::make_shared;
using std::make_unique;
using std::move;
using std::ostream;
using std::shared_ptr;
using std::stack;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

#define ERROR(s, ...) error(s->getSrcInfo(), __VA_ARGS__)
#define CAST(s, T) dynamic_cast<T *>(s.get())

namespace seq {
namespace ast {

json DocStmtVisitor::apply(const string &argv0, const vector<string> &files) {
  auto shared = make_shared<DocShared>();
  shared->argv0 = argv0;

  auto stdlib = getImportFile(argv0, "core", "", true);
  auto ast = parse_file(stdlib);
  shared->modules[""] = make_shared<DocContext>(shared);
  shared->modules[""]->file = stdlib;
  for (auto &s : vector<string>{"void", "byte", "float", "seq", "bool", "int",
                                "str", "ptr", "function", "generator", "tuple",
                                "array", "Kmer", "Int", "UInt", "optional"}) {
    shared->j[to_string(shared->itemID)] = {
        {"kind", "class"}, {"name", s}, {"type", "type"}};
    shared->modules[""]->add(s, make_shared<int>(shared->itemID++));
  }

  DocStmtVisitor(shared->modules[""]).transformModule(move(ast));
  auto ctx = make_shared<DocContext>(shared);

  char abs[PATH_MAX];
  for (auto &f : files) {
    realpath(f.c_str(), abs);
    ctx->file = abs;
    ast = parse_file(abs);
    DocStmtVisitor(ctx).transformModule(move(ast));
  }

  return shared->j;
}

std::shared_ptr<int> DocContext::find(const std::string &s) {
  auto i = VTable<int>::find(s);
  if (!i && this != shared->modules[""].get()) {
    return shared->modules[""]->find(s);
  }
  return i;
}

string getDocstr(const StmtPtr &s) {
  if (auto se = CAST(s, ExprStmt))
    if (auto e = CAST(se->expr, StringExpr))
      return e->value;
  return "";
}

vector<StmtPtr> DocStmtVisitor::flatten(StmtPtr stmt, string *docstr,
                                        bool deep) {
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

json DocExprVisitor::transform(const ExprPtr &expr) {
  DocExprVisitor v(ctx);
  expr->accept(v);
  return v.result;
}

json DocStmtVisitor::transform(const ExprPtr &expr) {
  DocExprVisitor v(ctx);
  expr->accept(v);
  return v.result;
}

std::string DocStmtVisitor::transform(const StmtPtr &stmt) {
  DocStmtVisitor v(ctx);
  stmt->accept(v);
  return v.result;
}

void DocStmtVisitor::transformModule(StmtPtr stmt) {
  vector<string> children;
  string docstr;

  auto flat = flatten(move(stmt), &docstr);
  for (int i = 0; i < flat.size(); i++) {
    auto &s = flat[i];
    auto id = transform(s);
    if (id == "")
      continue;
    if (i < (flat.size() - 1) &&
        (CAST(s, AssignStmt) || CAST(s, ExternImportStmt) ||
         CAST(s, TypeAliasStmt))) {
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

void DocExprVisitor::visit(const IdExpr *expr) {
  auto i = ctx->find(expr->value);
  if (!i)
    error("unknown identifier {}", expr->value);
  if (*i == 0)
    result = expr->value;
  else
    result = to_string(*i);
}

void DocExprVisitor::visit(const IndexExpr *expr) {
  vector<json> v;
  v.push_back(transform(expr->expr));
  if (auto tp = CAST(expr->index, TupleExpr)) {
    for (auto &e : tp->items)
      v.push_back(transform(e));
  } else {
    v.push_back(transform(expr->index));
  }
  result = json(v);
}

bool isValidName(const string &s) {
  if (s.empty())
    return false;
  if (s.size() > 4 && s.substr(0, 2) == "__" && s.substr(s.size() - 2) == "__")
    return true;
  return s[0] != '_';
}

void DocStmtVisitor::visit(const FunctionStmt *stmt) {
  int id = ctx->shared->itemID++;
  ctx->add(stmt->name, make_shared<int>(id));
  json j{{"kind", "function"},
         {"pos", jsonify(stmt->getSrcInfo())},
         {"name", stmt->name}};

  vector<json> args;
  for (auto &g : stmt->generics)
    ctx->add(g, make_shared<int>(0));
  for (auto &a : stmt->args) {
    json j;
    j["name"] = a.name;
    if (a.type)
      j["type"] = transform(a.type);
    if (a.deflt) {
      FormatExprVisitor v;
      j["default"] = v.transform(a.deflt);
    }
    args.push_back(j);
  }
  j["generics"] = stmt->generics;
  j["attrs"] = stmt->attributes;
  if (stmt->ret)
    j["return"] = transform(stmt->ret);
  j["args"] = args;
  string docstr;
  flatten(move(const_cast<FunctionStmt *>(stmt)->suite), &docstr);
  for (auto &g : stmt->generics)
    ctx->remove(g);
  if (docstr.size())
    j["doc"] = docstr;
  ctx->shared->j[to_string(id)] = j;
  result = to_string(id);
}

void DocStmtVisitor::visit(const ClassStmt *stmt) {
  int id = ctx->shared->itemID++;
  ctx->add(stmt->name, make_shared<int>(id));
  json j{{"name", stmt->name},
         {"kind", "class"},
         {"type", stmt->isType ? "type" : "class"}};

  vector<json> args;
  for (auto &g : stmt->generics)
    ctx->add(g, make_shared<int>(0));
  for (auto &a : stmt->args) {
    json j;
    j["name"] = a.name;
    if (a.type)
      j["type"] = transform(a.type);
    args.push_back(j);
  }
  j["generics"] = stmt->generics;
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
    ctx->remove(g);
  j["members"] = members;
  if (docstr.size())
    j["doc"] = docstr;
  ctx->shared->j[to_string(id)] = j;
  result = to_string(id);
}

void DocStmtVisitor::visit(const TypeAliasStmt *stmt) {
  int id = ctx->shared->itemID++;
  ctx->add(stmt->name, make_shared<int>(id));
  json j{
      {"name", stmt->name}, {"kind", "alias"}, {"type", transform(stmt->expr)}};
  ctx->shared->j[to_string(id)] = j;
  result = to_string(id);
}

json DocStmtVisitor::jsonify(const seq::SrcInfo &s) {
  json j{s.line, s.endLine};
  // j["file"] = s.file;
  // j["line"] = s.line;
  // j["col"] = s.col;
  return j;
}

void DocStmtVisitor::visit(const ImportStmt *stmt) {
  auto file =
      getImportFile(ctx->shared->argv0, stmt->from.first, ctx->file, false);
  if (file == "")
    ERROR(stmt, "cannot locate import '{}'", stmt->from.first);

  auto ictx = ctx;
  auto it = ctx->shared->modules.find(file);
  if (it == ctx->shared->modules.end()) {
    ictx = make_shared<DocContext>(ctx->shared);
    ictx->file = file;
    auto ast = parse_file(file);
    DocStmtVisitor(ictx).transformModule(move(ast));
  } else {
    ictx = it->second;
  }

  if (!stmt->what.size()) {
    // TODO
    // ctx.add(stmt->from.second == "" ? stmt->from.first : stmt->from.second,
    // file);
  } else if (stmt->what.size() == 1 && stmt->what[0].first == "*") {
    for (auto &i : *ictx)
      ctx->add(i.first, i.second.top());
  } else
    for (auto &w : stmt->what) {
      if (auto c = ictx->find(w.first)) {
        ctx->add(w.second == "" ? w.first : w.second, c);
      } else {
        ERROR(stmt, "symbol '{}' not found in {}", w.first, file);
      }
    }
}

void DocStmtVisitor::visit(const AssignStmt *stmt) {
  if (stmt->mustExist)
    return;
  auto e = CAST(stmt->lhs, IdExpr);
  if (!e)
    return;
  int id = ctx->shared->itemID++;
  ctx->add(e->value, make_shared<int>(id));
  json j{{"name", e->value},
         {"kind", "variable"},
         {"pos", jsonify(stmt->getSrcInfo())}};
  ctx->shared->j[to_string(id)] = j;
  result = to_string(id);
}

void DocStmtVisitor::visit(const ExternImportStmt *stmt) {
  int id = ctx->shared->itemID++;
  auto name = stmt->name.second != "" ? stmt->name.second : stmt->name.first;
  ctx->add(name, make_shared<int>(id));

  json j{{"name", name},
         {"kind", "function"},
         {"pos", jsonify(stmt->getSrcInfo())},
         {"extern", stmt->lang}};

  vector<json> args;
  if (stmt->ret)
    j["return"] = transform(stmt->ret);
  for (auto &a : stmt->args) {
    json j;
    j["name"] = a.name;
    j["type"] = transform(a.type);
  }
  j["dylib"] = bool(stmt->from);
  j["args"] = args;
  ctx->shared->j[to_string(id)] = j;
  result = to_string(id);
}

void DocStmtVisitor::visit(const ExtendStmt *stmt) {
  string name;
  vector<string> generics;
  if (auto w = CAST(stmt->what, IdExpr)) {
    name = w->value;
  } else if (auto w = CAST(stmt->what, IndexExpr)) {
    auto e = CAST(w->expr, IdExpr);
    assert(e);
    name = e->value;
    if (auto t = CAST(w->index, TupleExpr)) {
      for (auto &ti : t->items) {
        auto l = CAST(ti, IdExpr);
        assert(l);
        generics.push_back(l->value);
      }
    } else if (auto l = CAST(w->index, IdExpr)) {
      generics.push_back(l->value);
    } else {
      ERROR(stmt, "invalid generic variable");
    }
  }
  auto i = ctx->find(name);
  if (!i)
    ERROR(stmt, "cannot find '{}'", name);

  json j{{"kind", "class"}, {"type", "extension"}, {"parent", to_string(*i)}};
  j["pos"] = jsonify(stmt->getSrcInfo());
  if (generics.size())
    j["generics"] = generics;

  for (auto &g : generics)
    ctx->add(g, make_shared<int>(0));

  string docstr;
  vector<string> members;
  for (auto &f :
       flatten(move(const_cast<ExtendStmt *>(stmt)->suite), &docstr)) {
    if (auto ff = CAST(f, FunctionStmt)) {
      auto i = transform(f);
      if (i != "")
        members.push_back(i);
      if (isValidName(ff->name))
        ctx->remove(ff->name);
    }
  }
  j["members"] = members;
  if (docstr.size())
    j["doc"] = docstr;

  for (auto &g : generics)
    ctx->remove(g);

  int id = ctx->shared->itemID++;
  ctx->shared->j[to_string(id)] = j;
  result = to_string(id);
}

} // namespace ast
} // namespace seq
