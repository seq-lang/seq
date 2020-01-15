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
#include "parser/stmt->h"
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

#define RETURN(T, ...) (this->result = make_unique<T>(__VA_ARGS__))
template <typename T> T &&setSrcInfo(T &&t, const seq::SrcInfo &i) {
  t->setSrcInfo(i);
  return t;
}

#define E(T, ...) make_unique<T>(__VA_ARGS__)
#define EP(T, ...) setSrcInfo(make_unique<T>(__VA_ARGS__), expr->getSrcInfo())
#define S(T, ...) make_unique<T>(__VA_ARGS__)
#define SP(T, ...) setSrcInfo(make_unique<T>(__VA_ARGS__), stmt->getSrcInfo())
#define ERROR(...) error(stmt->getSrcInfo(), __VA_ARGS__)

unique_ptr<SuiteStmt> TransformStmtVisitor::apply(unique_ptr<SuiteStmt> stmts) {
  auto tv = TransformStmtVisitor();
  for (auto &s : stmts->stmts) {
    fmt::print("> {}\n", s->to_string());
  }
  fmt::print("-----\n");
  stmts->accept(tv);
  return unique_ptr<SuiteStmt>(dynamic_cast<SuiteStmt *>(tv.result.release()));
}

StmtPtr TransformStmtVisitor::transform(const StmtPtr &stmt) {
  if (!stmt) {
    return nullptr;
  }
  TransformStmtVisitor v;
  v.newSrcInfo = newSrcInfo;
  stmt->accept(v);
  if (newSrcInfo.line) {
    v.result->setSrcInfo(newSrcInfo);
  }
  fmt::print("==> {} :pos {} \n => {} : pos {}\n", stmt, stmt->getSrcInfo(),
             *v.result, v.result->getSrcInfo());
  return move(v.result);
}
ExprPtr TransformStmtVisitor::transform(const ExprPtr &expr) {
  if (!stmt) {
    return nullptr;
  }
  TransformExprVisitor v;
  expr->accept(v);
  return move(v.result);
}

void TransformStmtVisitor::visit(const SuiteStmt *stmt) {
  vector<StmtPtr> result(stmt->stmts.size());
  for (auto &i : stmt->stmts) {
    result.push_back(transform(i));
  }
  RETURN(SuiteStmt, move(result));
}
void TransformStmtVisitor::visit(const PassStmt *stmt) { RETURN(PassStmt, ); }
void TransformStmtVisitor::visit(const BreakStmt *stmt) { RETURN(BreakStmt, ); }
void TransformStmtVisitor::visit(const ContinueStmt *stmt) {
  RETURN(ContinueStmt, );
}
void TransformStmtVisitor::visit(const ExprStmt *stmt) {
  RETURN(ExprStmt, transform(stmt->expr));
}
void TransformStmtVisitor::visit(const AssignStmt *stmt) {
  RETURN(AssignStmt, transform(stmt->lhs), transform(stmt->rhs), stmt->kind,
         transform(stmt->type));
}
void TransformStmtVisitor::visit(const DelStmt *stmt) {
  if (auto expr = dynamic_cast<IndexExpr *>(stmt->expr)) {
    RETURN(ExprStmt,
           transform(EP(CallExpr, EP(DotExpr, expr->expr, "__delitem__"),
                         expr->index)));
  } else {
    RETURN(DelStmt, transform(stmt->expr));
  }
}
void TransformStmtVisitor::visit(const PrintStmt *stmt) {
  vector<StmtPtr> suite;
  for (int i = 0; i < stmt->items.size(); i++) {
    suite.push_back(SP(PrintStmt, transform(stmt->items[i])));
    auto terminator = i == stmt->items.size() - 1 ? stmt->terminator : " ";
    suite.push_back(SP(PrintStmt, EP(StringExpr, terminator)));
  }
  RETURN(SuiteStmt, move(suite));
}
void TransformStmtVisitor::visit(const ReturnStmt *stmt) {
  RETURN(ReturnStmt, transform(stmt->expr));
}
void TransformStmtVisitor::visit(const YieldStmt *stmt) {
  RETURN(YieldStmt, transform(stmt->expr));
}
void TransformStmtVisitor::visit(const AssertStmt *stmt) {
  RETURN(AssertStmt, transform(stmt->expr));
}
void TransformStmtVisitor::visit(const TypeAliasStmt *stmt) {
  RETURN(TypeAliasStmt, stmt->name, transform(stmt->expr));
}
void TransformStmtVisitor::visit(const WhileStmt *stmt) {
  RETURN(WhileStmt, transform(stmt->cond), transform(stmt->suite));
}
void TransformStmtVisitor::visit(const ForStmt *stmt) {
  RETURN(ForStmt, stmt->vars, transform(stmt->iter), transform(stmt->suite));
}
void TransformStmtVisitor::visit(const IfStmt *stmt) {
  vector<IfExpr::If> ifs;
  for (auto &ifc : stmt->ifs) {
    ifs.push_back({transform(ifc.cond), transform(ifc.suite)});
  }
  RETURN(IfStmt, move(ifs));
}
void TransformStmtVisitor::visit(const MatchStmt *stmt) {
  ERROR("TODO");
  // for (auto &c : stmt->cases) {
  //   // c.what = move(visit(*stmt->what));
  //   c.second = visit(*c.second);
  // }
  // RETURN(MatchStmt, visit(*stmt->what), move(stmt->cases));
}
void TransformStmtVisitor::visit(const ExtendStmt *stmt) {
  for (auto s : stmt->suite->getStatements()) {
    if (!dynamic_cast<FunctionStmt *>(s)) {
      suite->stmts.push_back(visit(*s));
    } else {
      ERROR("types can be extended with functions only");
    }
  }
  RETURN(ExtendStmt, transform(*stmt->what), transform(stmt->suite));
}
void TransformStmtVisitor::visit(const ImportStmt *stmt) {
  RETURN(ImportStmt, stmt->from, stmt->what);
}
void TransformStmtVisitor::visit(const ExternImportStmt *stmt) {
  for (auto &a : stmt->args) {
    a.type = visit(*a.type);
    a.deflt = visit(*a.deflt);
  }
  RETURN(ExternImportStmt, stmt->name, visit(*stmt->from), visit(*stmt->ret),
         move(stmt->args), stmt->lang);
}
void TransformStmtVisitor::visit(const TryStmt *stmt) {
  for (auto &c : stmt->catches) {
    c.exc = visit(*c.exc);
    c.suite = visit(*c.suite);
  }
  RETURN(TryStmt, visit(*stmt->suite), move(stmt->catches),
         visit(*stmt->finally));
}
void TransformStmtVisitor::visit(const GlobalStmt *stmt) {
  RETURN(GlobalStmt, stmt->var);
}
void TransformStmtVisitor::visit(const ThrowStmt *stmt) {
  RETURN(ThrowStmt, visit(*stmt->expr));
}
void TransformStmtVisitor::visit(const PrefetchStmt *stmt) {
  for (auto &i : stmt->what) {
    i = visit(*i);
  }
  RETURN(PrefetchStmt, move(stmt->what));
}
void TransformStmtVisitor::visit(const FunctionStmt *stmt) {
  for (auto &a : stmt->args) {
    if (a.type)
      a.type = visit(*a.type);
    if (a.deflt)
      a.deflt = visit(*a.deflt);
  }
  RETURN(FunctionStmt, stmt->name, stmt->ret ? visit(*stmt->ret) : nullptr,
         stmt->generics, move(stmt->args), visit(*stmt->suite),
         stmt->attributes);
}
void TransformStmtVisitor::visit(const ClassStmt *stmt) {
  for (auto &a : stmt->args) {
    a.type = visit(*a.type);
    a.deflt = visit(*a.deflt);
  }
  auto suite = make_unique<SuiteStmt>();
  suite->setSrcInfo(stmt->getSrcInfo());
  for (auto s : stmt->suite->getStatements()) {
    if (dynamic_cast<FunctionStmt *>(s)) {
      suite->stmts.push_back(visit(*s));
    } else {
      error(s->getSrcInfo(), "types can only contain functions");
    }
  }
  RETURN(ClassStmt, stmt->is_type, stmt->name, stmt->generics, move(stmt->args),
         move(suite));
}
void TransformStmtVisitor::visit(const DeclareStmt *stmt) {
  stmt->param.type = visit(*stmt->param.type);
  stmt->param.deflt = visit(*stmt->param.deflt);
  RETURN(DeclareStmt, move(stmt->param));
}
