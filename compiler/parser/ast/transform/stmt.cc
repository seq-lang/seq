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

#include "parser/ast/expr.h"
#include "parser/ast/stmt.h"
#include "parser/ast/transform/expr.h"
#include "parser/ast/transform/pattern.h"
#include "parser/ast/transform/stmt.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "parser/context.h"
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

#define RETURN(T, ...)                                                         \
  (this->result =                                                              \
       fwdSrcInfo(make_unique<T>(__VA_ARGS__), stmt->getSrcInfo()));           \
  return

#define E(T, ...) make_unique<T>(__VA_ARGS__)
#define EP(T, ...) fwdSrcInfo(make_unique<T>(__VA_ARGS__), expr->getSrcInfo())
#define EPX(e, T, ...) fwdSrcInfo(make_unique<T>(__VA_ARGS__), e->getSrcInfo())
#define S(T, ...) make_unique<T>(__VA_ARGS__)
#define SP(T, ...) fwdSrcInfo(make_unique<T>(__VA_ARGS__), stmt->getSrcInfo())
#define SPX(s, T, ...) fwdSrcInfo(make_unique<T>(__VA_ARGS__), s->getSrcInfo())
#define ERROR(...) error(stmt->getSrcInfo(), __VA_ARGS__)

void TransformStmtVisitor::prepend(StmtPtr s) {
  prependStmts.push_back(move(s));
}

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
  if (v.prependStmts.size()) {
    v.prependStmts.push_back(move(v.result));
    return make_unique<SuiteStmt>(move(v.prependStmts));
  } else {
    return move(v.result);
  }
}

ExprPtr TransformStmtVisitor::transform(const Expr *expr) {
  if (!expr) {
    return nullptr;
  }
  vector<StmtPtr> prepend;
  TransformExprVisitor v(prepend);
  expr->accept(v);
  for (auto &s : prepend) {
    prependStmts.push_back(move(s));
  }
  return move(v.result);
}

PatternPtr TransformStmtVisitor::transform(const Pattern *pat) {
  if (!pat) {
    return nullptr;
  }
  TransformPatternVisitor v(*this);
  pat->accept(v);
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

StmtPtr TransformStmtVisitor::addAssignment(const Expr *lhs, const Expr *rhs,
                                            const Expr *type) {
  // fmt::print("## ass {} = {}\n", *lhs, *rhs);
  if (auto l = dynamic_cast<const IndexExpr *>(lhs)) {
    // vector<ExprPtr> args;
    // args.push_back(transform(l->index));
    // args.push_back(transform(rhs));
    // return SPX(lhs, ExprStmt,
    //            EPX(lhs, CallExpr,
    //                EPX(lhs, DotExpr, transform(l->expr), "__setitem__"),
    //                move(args)));
    return SPX(lhs, AssignStmt, transform(lhs), transform(rhs));
  } else if (auto l = dynamic_cast<const DotExpr *>(lhs)) {
    return SPX(lhs, AssignStmt, transform(lhs), transform(rhs));
  } else if (auto l = dynamic_cast<const IdExpr *>(lhs)) {
    return SPX(lhs, AssignStmt, transform(lhs), transform(rhs),
               transform(type));
  } else {
    error(lhs->getSrcInfo(), "invalid assignment");
    return nullptr;
  }
}

void TransformStmtVisitor::processAssignment(const Expr *lhs, const Expr *rhs,
                                             vector<StmtPtr> &stmts) {
  vector<Expr *> lefts;
  if (auto l = dynamic_cast<const TupleExpr *>(lhs)) {
    for (auto &i : l->items) {
      lefts.push_back(i.get());
    }
  } else if (auto l = dynamic_cast<const ListExpr *>(lhs)) {
    for (auto &i : l->items) {
      lefts.push_back(i.get());
    }
  } else {
    stmts.push_back(addAssignment(lhs, rhs));
    return;
  }
  if (!dynamic_cast<const IdExpr *>(rhs)) { // store any non-trivial expression
    auto var = getTemporaryVar("assign");
    auto newRhs = EPX(rhs, IdExpr, var).release();
    stmts.push_back(addAssignment(newRhs, rhs));
    rhs = newRhs;
  }
  UnpackExpr *unpack = nullptr;
  int st = 0, ed = lefts.size() - 1;
  for (; st < lefts.size(); st++) {
    if (auto u = dynamic_cast<UnpackExpr *>(lefts[st])) {
      unpack = u;
      break;
    }
    // TODO: RHS here (and below) will be transformed twice in order to avoid
    // messing up with unique_ptr. Better solution needed?
    stmts.push_back(addAssignment(
        lefts[st],
        EPX(rhs, IndexExpr, transform(rhs), EPX(rhs, IntExpr, st)).release()));
  }
  for (int i = 1; ed > st; i++, ed--) {
    if (dynamic_cast<UnpackExpr *>(lefts[ed])) {
      break;
    }
    stmts.push_back(addAssignment(
        lefts[ed],
        EPX(rhs, IndexExpr, transform(rhs), EPX(rhs, IntExpr, -i)).release()));
  }
  if (st < lefts.size() && st != ed) {
    error(lefts[st]->getSrcInfo(), "two starred expressions in assignment");
  }
  if (unpack) {
    processAssignment(unpack->what.get(),
                      EPX(rhs, IndexExpr, transform(rhs),
                          EPX(rhs, SliceExpr, EPX(rhs, IntExpr, st),
                              EPX(rhs, IntExpr, ed + 1), nullptr))
                          .release(),
                      stmts);
  }
}

void TransformStmtVisitor::visit(const AssignStmt *stmt) {
  // a, b, *x, c, d = y
  // (^) = y
  // [^] = y
  // *a = y NO ; *a, = y YES
  // (a, b), c = d, e
  // *(a, *b), c = this
  // a = *iterable

  vector<StmtPtr> stmts;
  if (stmt->type) {
    if (auto i = dynamic_cast<IdExpr *>(stmt->lhs.get())) {
      stmts.push_back(
          addAssignment(stmt->lhs.get(), stmt->rhs.get(), stmt->type.get()));
    } else {
      ERROR("only single target can be annotated");
    }
  } else {
    processAssignment(stmt->lhs.get(), stmt->rhs.get(), stmts);
  }
  if (stmts.size() == 1) {
    this->result = move(stmts[0]);
  } else {
    RETURN(SuiteStmt, move(stmts));
  }
}

void TransformStmtVisitor::visit(const DelStmt *stmt) {
  RETURN(DelStmt, transform(stmt->expr));

  // TODO later with types
  if (auto expr = dynamic_cast<const IndexExpr *>(stmt->expr.get())) {
    RETURN(ExprStmt,
           transform(EP(CallExpr,
                        EP(DotExpr, transform(expr->expr), "__delitem__"),
                        transform(expr->index))));
  } else if (auto expr = dynamic_cast<const IdExpr *>(stmt->expr.get())) {
    RETURN(DelStmt, transform(expr));
  } else {
    ERROR("this expression cannot be deleted");
  }
}

void TransformStmtVisitor::visit(const PrintStmt *stmt) {
  RETURN(PrintStmt, transform(stmt->expr));
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
  auto iter = transform(stmt->iter);
  StmtPtr suite;
  ExprPtr var;
  if (dynamic_cast<IdExpr *>(stmt->var.get())) {
    var = transform(stmt->var);
    suite = transform(stmt->suite);
  } else {
    string varName = getTemporaryVar("for");
    vector<StmtPtr> stmts;
    var = EPX(stmt, IdExpr, varName);
    processAssignment(stmt->var.get(), var.get(), stmts);
    stmts.push_back(transform(stmt->suite));
    suite = SP(SuiteStmt, move(stmts));
  }
  RETURN(ForStmt, move(var), move(iter), move(suite));
}

void TransformStmtVisitor::visit(const IfStmt *stmt) {
  vector<IfStmt::If> ifs;
  for (auto &ifc : stmt->ifs) {
    ifs.push_back({transform(ifc.cond), transform(ifc.suite)});
  }
  RETURN(IfStmt, move(ifs));
}

void TransformStmtVisitor::visit(const MatchStmt *stmt) {
  vector<pair<PatternPtr, StmtPtr>> cases;
  for (auto &c : stmt->cases) {
    cases.push_back({transform(c.first), transform(c.second)});
  }
  RETURN(MatchStmt, transform(stmt->what), move(cases));
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
  if (stmt->lang == "c" && stmt->from) {
    vector<StmtPtr> stmts;
    // ptr = _dlsym(FROM, WHAT)
    vector<ExprPtr> args;
    args.push_back(transform(stmt->from));
    args.push_back(EPX(stmt, StringExpr, stmt->name.first));
    stmts.push_back(
        SP(AssignStmt, EPX(stmt, IdExpr, "ptr"),
           EPX(stmt, CallExpr, EPX(stmt, IdExpr, "_dlsym"), move(args))));
    // f = function[ARGS](ptr)
    args.clear();
    args.push_back(stmt->ret ? transform(stmt->ret)
                             : EPX(stmt, IdExpr, "void"));
    for (auto &a : stmt->args) {
      args.push_back(transform(a.type));
    }
    stmts.push_back(SP(AssignStmt, EPX(stmt, IdExpr, "f"),
                       EPX(stmt, CallExpr,
                           EPX(stmt, IndexExpr, EPX(stmt, IdExpr, "function"),
                               EPX(stmt, TupleExpr, move(args))),
                           EPX(stmt, IdExpr, "ptr"))));
    bool isVoid = true;
    if (stmt->ret) {
      if (auto f = dynamic_cast<IdExpr *>(stmt->ret.get())) {
        isVoid = f->value == "void";
      } else {
        isVoid = false;
      }
    }
    args.clear();
    int ia = 0;
    for (auto &a : stmt->args) {
      args.push_back(
          EPX(stmt, IdExpr, a.name != "" ? a.name : format("$a{}", ia++)));
    }
    // return f(args)
    auto call = EPX(stmt, CallExpr, EPX(stmt, IdExpr, "f"), move(args));
    if (!isVoid) {
      stmts.push_back(SP(ReturnStmt, move(call)));
    } else {
      stmts.push_back(SP(ExprStmt, move(call)));
    }
    // def WHAT(args):
    vector<Param> params;
    ia = 0;
    for (auto &a : stmt->args) {
      params.push_back(
          {a.name != "" ? a.name : format("$a{}", ia++), transform(a.type)});
    }
    RETURN(FunctionStmt,
           stmt->name.second != "" ? stmt->name.second : stmt->name.first,
           transform(stmt->ret), vector<string>(), move(params),
           SP(SuiteStmt, move(stmts)), vector<string>());
  } else if (stmt->lang == "c") {
    vector<Param> args;
    for (auto &a : stmt->args) {
      args.push_back({a.name, transform(a.type), transform(a.deflt)});
    }
    RETURN(ExternImportStmt, stmt->name, transform(stmt->from),
           transform(stmt->ret), move(args), stmt->lang);
  } else if (stmt->lang == "py") {
    vector<StmtPtr> stmts;
    auto call =
        EPX(stmt, CallExpr, // _py_import(LIB)[WHAT].call ( ...
            EPX(stmt, DotExpr,
                EPX(stmt, IndexExpr,
                    EPX(stmt, CallExpr, EPX(stmt, IdExpr, "_py_import"),
                        transform(stmt->from)),
                    EPX(stmt, IdExpr, stmt->name.first)),
                "call"),
            EPX(stmt, CallExpr, // ... x.__to_py__() )
                EPX(stmt, DotExpr, EPX(stmt, IdExpr, "x"), "__to_py__")));
    bool isVoid = true;
    if (stmt->ret) {
      if (auto f = dynamic_cast<IdExpr *>(stmt->ret.get())) {
        isVoid = f->value == "void";
      } else {
        isVoid = false;
      }
    }
    if (!isVoid) {
      // return TYP.__from_py__(call)
      stmts.push_back(
          SP(ReturnStmt,
             EPX(stmt, CallExpr,
                 EPX(stmt, DotExpr, transform(stmt->ret), "__from_py__"),
                 move(call))));
    } else {
      stmts.push_back(SP(ExprStmt, move(call)));
    }
    vector<Param> params;
    params.push_back({"x", nullptr, nullptr});
    RETURN(FunctionStmt,
           stmt->name.second != "" ? stmt->name.second : stmt->name.first,
           transform(stmt->ret), vector<string>(), move(params),
           SP(SuiteStmt, move(stmts)), vector<string>{"pyhandle"});
  } else {
    ERROR("language {} not yet supported", stmt->lang);
  }
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
  RETURN(PrefetchStmt, transform(stmt->expr));
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
  RETURN(ClassStmt, stmt->isType, stmt->name, stmt->generics, move(args),
         move(suite));
}

void TransformStmtVisitor::visit(const DeclareStmt *stmt) {
  RETURN(DeclareStmt, Param{stmt->param.name, transform(stmt->param.type),
                            transform(stmt->param.deflt)});
}

void TransformStmtVisitor::visit(const AssignEqStmt *stmt) {
  RETURN(AssignStmt, transform(stmt->lhs),
         EPX(stmt, BinaryExpr, transform(stmt->lhs), stmt->op,
             transform(stmt->rhs), true),
         nullptr, true);
}

void TransformStmtVisitor::visit(const YieldFromStmt *stmt) {
  auto var = getTemporaryVar("yield");
  vector<StmtPtr> stmts;
  stmts.push_back(SP(YieldStmt, EPX(stmt, IdExpr, var)));
  RETURN(ForStmt, EPX(stmt, IdExpr, var), transform(stmt->expr),
         SP(SuiteStmt, move(stmts)));
}

void TransformStmtVisitor::visit(const WithStmt *stmt) {
  if (!stmt->items.size()) {
    ERROR("malformed with statement");
  }
  vector<StmtPtr> content;
  for (int i = stmt->items.size() - 1; i >= 0; i--) {
    vector<StmtPtr> internals;
    string var = stmt->items[i].second == "" ? getTemporaryVar("with")
                                             : stmt->items[i].second;
    internals.push_back(SP(AssignStmt, EPX(stmt, IdExpr, var),
                           transform(stmt->items[i].first)));
    internals.push_back(
        SP(ExprStmt,
           EPX(stmt, CallExpr,
               EPX(stmt, DotExpr, EPX(stmt, IdExpr, var), "__enter__"))));
    internals.push_back(
        SP(TryStmt,
           content.size() ? transform(SP(SuiteStmt, move(content)))
                          : transform(stmt->suite),
           vector<TryStmt::Catch>(),
           transform(SP(ExprStmt, EPX(stmt, CallExpr,
                                      EPX(stmt, DotExpr, EPX(stmt, IdExpr, var),
                                          "__exit__"))))));
    content = move(internals);
  }
  vector<IfStmt::If> ifs;
  ifs.push_back({EPX(stmt, BoolExpr, true), SP(SuiteStmt, move(content))});
  RETURN(IfStmt, move(ifs));
}

void TransformStmtVisitor::visit(const PyDefStmt *stmt) {
  // py.exec(""" str """)
  vector<string> args;
  for (auto &a : stmt->args) {
    args.push_back(a.name);
  }
  auto code = stmt->code;
  code = format("def %s(%s):\n%s\n", stmt->name, fmt::join(args, ", "), stmt->code);
  vector<StmtPtr> stmts;
  stmts.push_back(
      SP(ExprStmt,
         EPX(stmt, CallExpr,
             EPX(stmt, DotExpr, EPX(stmt, IdExpr, "py"), "exec"),
             EPX(stmt, StringExpr, code))));
  // from __main__ pyimport foo () -> ret
  stmts.push_back(transform(SP(ExternImportStmt, make_pair(stmt->name, ""),
                               EPX(stmt, IdExpr, "__main__"),
                               transform(stmt->ret), vector<Param>(), "py")));
  RETURN(SuiteStmt, move(stmts));
}
