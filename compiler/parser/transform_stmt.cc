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

#define RETURN(T, ...) (this->result = setSrcInfo(make_unique<T>(__VA_ARGS__), stmt->getSrcInfo()))
#define E(T, ...) make_unique<T>(__VA_ARGS__)
#define EP(T, ...) setSrcInfo(make_unique<T>(__VA_ARGS__), expr->getSrcInfo())
#define EPX(e, T, ...) setSrcInfo(make_unique<T>(__VA_ARGS__), e->getSrcInfo())
#define S(T, ...) make_unique<T>(__VA_ARGS__)
#define SP(T, ...) setSrcInfo(make_unique<T>(__VA_ARGS__), stmt->getSrcInfo())
#define ERROR(...) error(stmt->getSrcInfo(), __VA_ARGS__)

StmtPtr TransformStmtVisitor::apply(const StmtPtr &stmts) {
  auto tv = TransformStmtVisitor();
  stmts->accept(tv);
  return move(tv.result);
}

StmtPtr TransformStmtVisitor::transform(const Stmt *stmt) {
  if (!stmt) {
    return nullptr;
  }
  TransformStmtVisitor v;
  stmt->accept(v);
  return move(v.result);
}

StmtPtr TransformStmtVisitor::transform(const StmtPtr &stmt) {
    return transform(stmt.get());
}

ExprPtr TransformStmtVisitor::transform(const ExprPtr &expr) {
  if (!expr) {
    return nullptr;
  }
  TransformExprVisitor v;
  expr->accept(v);
  return move(v.result);
}

void TransformStmtVisitor::visit(const SuiteStmt *stmt) {
  vector<StmtPtr> result;
  for (auto &i : stmt->stmts) {
    if (auto s = transform(i)) {
      result.push_back(move(s));
    }
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
  if (auto expr = dynamic_cast<const IndexExpr *>(stmt->expr.get())) {
    RETURN(ExprStmt,
           transform(EP(CallExpr, EP(DotExpr, transform(expr->expr), "__delitem__"),
                        transform(expr->index))));
  } else {
    RETURN(DelStmt, transform(stmt->expr));
  }
}
void TransformStmtVisitor::visit(const PrintStmt *stmt) {
  vector<StmtPtr> suite;
  for (int i = 0; i < stmt->items.size(); i++) {
    suite.push_back(SP(PrintStmt, transform(stmt->items[i])));
    auto terminator = i == stmt->items.size() - 1 ? stmt->terminator : " ";
    suite.push_back(SP(PrintStmt, EPX(stmt, StringExpr, terminator)));
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
  vector<IfStmt::If> ifs;
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
  auto suite = make_unique<SuiteStmt>(stmt->getSrcInfo());
  for (auto s : stmt->suite->getStatements()) {
    if (dynamic_cast<FunctionStmt *>(s)) {
      suite->stmts.push_back(transform(s));
    } else {
      error(s->getSrcInfo(), "types can be extended with functions only");
    }
  }
  RETURN(ExtendStmt, transform(stmt->what), move(suite));
}
void TransformStmtVisitor::visit(const ImportStmt *stmt) {
  RETURN(ImportStmt, stmt->from, stmt->what);
}
void TransformStmtVisitor::visit(const ExternImportStmt *stmt) {
  vector<Param> args;
  for (auto &a : stmt->args) {
    args.push_back({a.name, transform(a.type), transform(a.deflt)});
  }
  RETURN(ExternImportStmt, stmt->name, transform(stmt->from),
         transform(stmt->ret), move(args), stmt->lang);
}
void TransformStmtVisitor::visit(const TryStmt *stmt) {
  vector<TryStmt::Catch> catches;
  for (auto &c : stmt->catches) {
    catches.push_back({c.var, transform(c.exc), transform(c.suite)});
  }
  RETURN(TryStmt, transform(stmt->suite), move(catches),
         transform(stmt->finally));
}
void TransformStmtVisitor::visit(const GlobalStmt *stmt) {
  RETURN(GlobalStmt, stmt->var);
}
void TransformStmtVisitor::visit(const ThrowStmt *stmt) {
  RETURN(ThrowStmt, transform(stmt->expr));
}
void TransformStmtVisitor::visit(const PrefetchStmt *stmt) {
  vector<ExprPtr> what;
  for (auto &i : stmt->what) {
    what.push_back(transform(i));
  }
  RETURN(PrefetchStmt, move(what));
}
void TransformStmtVisitor::visit(const FunctionStmt *stmt) {
  vector<Param> args;
  for (auto &a : stmt->args) {
    args.push_back({a.name, transform(a.type), transform(a.deflt)});
  }
  RETURN(FunctionStmt, stmt->name, transform(stmt->ret), stmt->generics,
         move(args), transform(stmt->suite), stmt->attributes);
}
void TransformStmtVisitor::visit(const ClassStmt *stmt) {
  vector<Param> args;
  for (auto &a : stmt->args) {
    args.push_back({a.name, transform(a.type), transform(a.deflt)});
  }
  auto suite = make_unique<SuiteStmt>(stmt->getSrcInfo());
  for (auto s : stmt->suite->getStatements()) {
    if (dynamic_cast<FunctionStmt *>(s)) {
      suite->stmts.push_back(transform(s));
    } else {
      error(s->getSrcInfo(), "types can only contain functions");
    }
  }
  RETURN(ClassStmt, stmt->is_type, stmt->name, stmt->generics, move(args),
         move(suite));
}
void TransformStmtVisitor::visit(const DeclareStmt *stmt) {
  RETURN(DeclareStmt, Param{stmt->param.name, transform(stmt->param.type),
                            transform(stmt->param.deflt)});
}
