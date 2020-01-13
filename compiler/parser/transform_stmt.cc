#include <fmt/format.h>
#include <fmt/ostream.h>
#include <memory>
#include <ostream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/common.h"
#include "parser/context.h"
#include "parser/expr.h"
#include "parser/stmt.h"
#include "parser/transform.h"
#include "parser/visitor.h"
#include "seq/seq.h"

using fmt::format;
using std::get;
using std::make_unique;
using std::move;
using std::ostream;
using std::stack;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

#define Return(T, ...) Set(make_unique<T>(__VA_ARGS__))

unique_ptr<SuiteStmt> TransformStmtVisitor::apply(unique_ptr<SuiteStmt> stmts) {
  auto tv = TransformStmtVisitor();
  for (auto &s: stmts->stmts) {
    fmt::print("> {}\n", s->to_string());
  }
  fmt::print("-----\n");
  auto result = tv.Visit(*stmts);
  for (auto &s: result->stmts) {
    fmt::print("> {}\n", s->to_string());
  }
  return result;
}

void TransformStmtVisitor::Set(StmtPtr &&stmt) { result = move(stmt); }
StmtPtr TransformStmtVisitor::Visit(Stmt &stmt) {
  TransformStmtVisitor v;
  if (newSrcInfo)
    v.newSrcInfo = move(newSrcInfo);
  stmt.accept(v);
  if (v.result && v.newSrcInfo) {
    v.result->setSrcInfo(*v.newSrcInfo);
  }
  if (v.newSrcInfo)
    newSrcInfo = move(v.newSrcInfo);
  return move(v.result);
}
SuiteStmtPtr TransformStmtVisitor::Visit(SuiteStmt &stmt) {
  for (auto &i: stmt.stmts) {
    i = Visit(*i);
  }
  return make_unique<SuiteStmt>(move(stmt.stmts));
}
StmtPtr TransformStmtVisitor::Visit(Stmt &stmt, const seq::SrcInfo &newInfo) {
  TransformStmtVisitor v;
  v.newSrcInfo = make_unique<seq::SrcInfo>(newInfo);
  stmt.accept(v);
  if (v.result && v.newSrcInfo) {
    v.result->setSrcInfo(*v.newSrcInfo);
  }
  return move(v.result);
}
ExprPtr TransformStmtVisitor::Visit(Expr &expr) {
  TransformExprVisitor v;
  expr.accept(v);
  return move(v.result);
}
ExprPtr TransformStmtVisitor::Visit(Expr &expr, const seq::SrcInfo &newInfo) {
  TransformExprVisitor v;
  v.newSrcInfo = make_unique<seq::SrcInfo>(newInfo);
  expr.accept(v);
  if (v.result && v.newSrcInfo) {
    v.result->setSrcInfo(*v.newSrcInfo);
  }
  return move(v.result);
}

void TransformStmtVisitor::visit(SuiteStmt &stmt) {
  for (auto &i : stmt.stmts) {
    i = Visit(*i);
  }
  Return(SuiteStmt, move(stmt.stmts));
}
void TransformStmtVisitor::visit(PassStmt &stmt) { Return(PassStmt, ); }
void TransformStmtVisitor::visit(BreakStmt &stmt) { Return(BreakStmt, ); }
void TransformStmtVisitor::visit(ContinueStmt &stmt) { Return(ContinueStmt, ); }
void TransformStmtVisitor::visit(ExprStmt &stmt) {
  Return(ExprStmt, Visit(*stmt.expr));
}
void TransformStmtVisitor::visit(AssignStmt &stmt) {
  Return(AssignStmt, Visit(*stmt.lhs), stmt.rhs ? Visit(*stmt.rhs) : nullptr,
         stmt.kind, stmt.type ? Visit(*stmt.type) : nullptr);
}
void TransformStmtVisitor::visit(DelStmt &stmt) {
  if (auto expr = dynamic_cast<IndexExpr *>(stmt.expr.get())) {
    ExprPtr p = make_unique<CallExpr>(
        make_unique<DotExpr>(move(expr->expr), "__delitem__"),
        move(expr->index));
    Return(ExprStmt, Visit(*p, expr->getSrcInfo()));
  } else {
    Return(DelStmt, Visit(*stmt.expr));
  }
}
void TransformStmtVisitor::visit(PrintStmt &stmt) {
  vector<StmtPtr> suite;
  for (int i = 0; i < stmt.items.size(); i++) {
    StmtPtr p = make_unique<PrintStmt>(Visit(*stmt.items[i]));
    p->setSrcInfo(stmt.items[i]->getSrcInfo());
    suite.push_back(move(p));
    p = make_unique<PrintStmt>(make_unique<StringExpr>(i == stmt.items.size() - 1 ? stmt.terminator : " "));
    p->setSrcInfo(stmt.items[i]->getSrcInfo());
    suite.push_back(move(p));
  }
  Return(SuiteStmt, move(suite));
}
void TransformStmtVisitor::visit(ReturnStmt &stmt) {
  Return(ReturnStmt, stmt.expr ? Visit(*stmt.expr) : nullptr);
}
void TransformStmtVisitor::visit(YieldStmt &stmt) {
  Return(YieldStmt, stmt.expr ? Visit(*stmt.expr) : nullptr);
}
void TransformStmtVisitor::visit(AssertStmt &stmt) {
  Return(AssertStmt, Visit(*stmt.expr));
}
void TransformStmtVisitor::visit(TypeAliasStmt &stmt) {
  Return(TypeAliasStmt, stmt.name, Visit(*stmt.expr));
}
void TransformStmtVisitor::visit(WhileStmt &stmt) {
  Return(WhileStmt, Visit(*stmt.cond), Visit(*stmt.suite));
}
void TransformStmtVisitor::visit(ForStmt &stmt) {
  Return(ForStmt, stmt.vars, Visit(*stmt.iter), Visit(*stmt.suite));
}
void TransformStmtVisitor::visit(IfStmt &stmt) {
  for (auto &ifc : stmt.ifs) {
    ifc.cond = Visit(*ifc.cond);
    ifc.suite = Visit(*ifc.suite);
  }
  Return(IfStmt, move(stmt.ifs));
}
void TransformStmtVisitor::visit(MatchStmt &stmt) {
  for (auto &c : stmt.cases) {
    error(stmt.getSrcInfo(), "TODO");
    // c.what = move(Visit(*stmt.what));
    c.second = Visit(*c.second);
  }
  Return(MatchStmt, Visit(*stmt.what), move(stmt.cases));
}
void TransformStmtVisitor::visit(ExtendStmt &stmt) {
  for (auto &i : stmt.suite->stmts) {
    if (dynamic_cast<FunctionStmt *>(i.get())) {
      i = Visit(*i);
    } else {
      error(i->getSrcInfo(), "types can be extended with functions only");
    }
  }
  Return(ExtendStmt, Visit(*stmt.what), move(stmt.suite));
}
void TransformStmtVisitor::visit(ImportStmt &stmt) {
  Return(ImportStmt, stmt.from, stmt.what);
}
void TransformStmtVisitor::visit(ExternImportStmt &stmt) {
  for (auto &a : stmt.args) {
    a.type = Visit(*a.type);
    a.deflt = Visit(*a.deflt);
  }
  Return(ExternImportStmt, stmt.name, Visit(*stmt.from), Visit(*stmt.ret), move(stmt.args),
         stmt.lang);
}
void TransformStmtVisitor::visit(TryStmt &stmt) {
  for (auto &c : stmt.catches) {
    c.exc = Visit(*c.exc);
    c.suite = Visit(*c.suite);
  }
  Return(TryStmt, Visit(*stmt.suite), move(stmt.catches), Visit(*stmt.finally));
}
void TransformStmtVisitor::visit(GlobalStmt &stmt) { Return(GlobalStmt, stmt.var); }
void TransformStmtVisitor::visit(ThrowStmt &stmt) {
  Return(ThrowStmt, Visit(*stmt.expr));
}
void TransformStmtVisitor::visit(PrefetchStmt &stmt) {
  for (auto &i : stmt.what) {
    i = Visit(*i);
  }
  Return(PrefetchStmt, move(stmt.what));
}
void TransformStmtVisitor::visit(FunctionStmt &stmt) {
  for (auto &a : stmt.args) {
    if (a.type) a.type = Visit(*a.type);
    if (a.deflt) a.deflt = Visit(*a.deflt);
  }
  Return(FunctionStmt, stmt.name, stmt.ret ? Visit(*stmt.ret) : nullptr, stmt.generics, move(stmt.args),
    Visit(*stmt.suite),
    stmt.attributes
  );
}
void TransformStmtVisitor::visit(ClassStmt &stmt) {
  for (auto &a : stmt.args) {
    a.type = Visit(*a.type);
    a.deflt = Visit(*a.deflt);
  }
  for (auto &i : stmt.suite->stmts) {
    if (dynamic_cast<FunctionStmt *>(i.get())) {
      i = Visit(*i);
    } else {
      error(i->getSrcInfo(), "types can only contain functions");
    }
  }
  Return(ClassStmt, stmt.is_type, stmt.name, stmt.generics, move(stmt.args), move(stmt.suite));
}
void TransformStmtVisitor::visit(DeclareStmt &stmt) {
  stmt.param.type = Visit(*stmt.param.type);
  stmt.param.deflt = Visit(*stmt.param.deflt);
  Return(DeclareStmt, move(stmt.param));
}
