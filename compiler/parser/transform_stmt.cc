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

#define RETURN(T, ...)                                                         \
  (this->result = setSrcInfo(make_unique<T>(__VA_ARGS__), stmt->getSrcInfo()))
#define E(T, ...) make_unique<T>(__VA_ARGS__)
#define EP(T, ...) setSrcInfo(make_unique<T>(__VA_ARGS__), expr->getSrcInfo())
#define EPX(e, T, ...) setSrcInfo(make_unique<T>(__VA_ARGS__), e->getSrcInfo())
#define S(T, ...) make_unique<T>(__VA_ARGS__)
#define SP(T, ...) setSrcInfo(make_unique<T>(__VA_ARGS__), stmt->getSrcInfo())
#define SPX(s, T, ...) setSrcInfo(make_unique<T>(__VA_ARGS__), s->getSrcInfo())
#define ERROR(...) error(stmt->getSrcInfo(), __VA_ARGS__)

int tmpVarCounter = 0;
string TransformStmtVisitor::getTemporaryVar(const string &prefix) const {
  return format("$_{}_{}", prefix, ++tmpVarCounter);
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
  fmt::print("I--> {}\n", stmt->to_string());
  TransformStmtVisitor v;
  stmt->accept(v);
  fmt::print("O--> {}\n", v.result->to_string());
  return move(v.result);
}

ExprPtr TransformStmtVisitor::transform(const Expr *expr) {
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

StmtPtr TransformStmtVisitor::addAssignment(const Expr *lhs, const Expr *rhs) {
  fmt::print("## ass {} = {}\n", *lhs, *rhs);
  if (auto l = dynamic_cast<const IndexExpr *>(lhs)) {
    vector<ExprPtr> args;
    args.push_back(transform(l->index));
    args.push_back(transform(rhs));
    return SPX(lhs, ExprStmt,
               EPX(lhs, CallExpr,
                   EPX(lhs, DotExpr, transform(l->expr), "__setitem__"),
                   move(args)));
  } else if (auto l = dynamic_cast<const DotExpr *>(lhs)) {
    return SPX(lhs, AssignStmt, transform(lhs), transform(rhs));
  } else if (auto l = dynamic_cast<const IdExpr *>(lhs)) {
    return SPX(lhs, AssignStmt, transform(lhs), transform(rhs));
  } else {
    error(lhs->getSrcInfo(), "invalid assignment");
    return nullptr;
  }
}

void TransformStmtVisitor::processAssignment(const Expr *lhs, const Expr *rhs,
                                             vector<StmtPtr> &stmts) {
  // fmt::print("!! ass {} = {}\n", *lhs, *rhs);
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
  if (unpack)
    processAssignment(unpack->what.get(),
                      EPX(rhs, IndexExpr, transform(rhs),
                          EPX(rhs, SliceExpr, EPX(rhs, IntExpr, st),
                              EPX(rhs, IntExpr, ed+1), nullptr))
                          .release(),
                      stmts);
}

void TransformStmtVisitor::visit(const AssignStmt *stmt) {
  // a, b, *x, c, d = y
  // (^) = y
  // [^] = y
  // *a = y NO ; *a, = y YES
  // (a, b), c = d, e
  // *(a, *b), c = this
  // a = *iterable

  if (stmt->type) {
    if (auto i = dynamic_cast<IdExpr *>(stmt->lhs.get())) {
      // TODO: wrap it in the constructor?
      // TODO: check list/sets etc
      ERROR("TODO type annotaions");
    } else {
      ERROR("only single target can be annotated");
    }
  }

  vector<StmtPtr> stmts;
  processAssignment(stmt->lhs.get(), stmt->rhs.get(), stmts);
  if (stmts.size() == 1) {
    this->result = move(stmts[0]);
  } else {
    RETURN(SuiteStmt, move(stmts));
  }
}
void TransformStmtVisitor::visit(const DelStmt *stmt) {
  if (auto expr = dynamic_cast<const IndexExpr *>(stmt->expr.get())) {
    RETURN(ExprStmt,
           transform(EP(CallExpr,
                        EP(DotExpr, transform(expr->expr), "__delitem__"),
                        transform(expr->index))));
  } else if (auto expr = dynamic_cast<const IdExpr *>(stmt->expr.get())) {
    RETURN(DelStmt, expr->value);
  } else {
    ERROR("this expression cannot be deleted");
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
  RETURN(ClassStmt, stmt->isType, stmt->name, stmt->generics, move(args),
         move(suite));
}
void TransformStmtVisitor::visit(const DeclareStmt *stmt) {
  RETURN(DeclareStmt, Param{stmt->param.name, transform(stmt->param.type),
                            transform(stmt->param.deflt)});
}

void TransformStmtVisitor::visit(const AssignEqStmt *stmt) { ERROR("TODO"); }

void TransformStmtVisitor::visit(const YieldFromStmt *stmt) {
  auto var = getTemporaryVar("yield");
  vector<StmtPtr> stmts;
  stmts.push_back(SP(YieldStmt, EPX(stmt, IdExpr, var)));
  RETURN(ForStmt, vector<string>{var}, transform(stmt->expr),
         SP(SuiteStmt, move(stmts)));
}

void TransformStmtVisitor::visit(const WithStmt *stmt) {
  /*
   let rec traverse (expr, var) lst =
      let var = opt_val var (fst expr, Id (new_assign ())) in
      let s1 = fst expr, Assign (var, expr, 1, None) in
      let s2 = fst expr, Expr (fst expr, Call ((fst expr, Dot (var,
   "__enter__")), [])) in let within = match lst with [] -> $4 | hd :: tl ->
   [traverse hd tl] in let s3 = fst expr, Try (within, [], [fst expr, Expr (fst
   expr, Call ((fst expr, Dot (var, "__exit__")), []))]) in fst expr, If [Some
   (fst expr, Bool true), [s1; s2; s3]] in traverse (List.hd $2) (List.tl $2)
  */
  ERROR("TODO");
  // RETURN(DeclareStmt, Param{stmt->param.name, transform(stmt->param.type),
  //                           transform(stmt->param.deflt)});
}

// void TransformStmtVisitor::visit(const PyStmt *stmt) {
//   RETURN(DeclareStmt, Param{stmt->param.name, transform(stmt->param.type),
//                             transform(stmt->param.deflt)});
// }
// pydef
/*  (* let str = Util.ppl ~sep:"\n" s ~f:(Ast.Stmt.to_string ~pythonic:true
   ~indent:1) in let p = $7 in
      (* py.exec ("""def foo(): [ind] ... """) *)
      (* from __main__ pyimport foo () -> ret *)
      let v = p, String
        (sprintf "def %s(%s):\n%s\n"
          (snd name)
          (Util.ppl fn_args ~f:(fun (_, { name; _ }) -> name))
          str) in
      let s = p, Call (
        (p, Id "_py_exec"),
        [p, { name = None; value = v }]) in
      let typ = opt_val typ ~default:($5, Id "pyobj") in
      let s' = p, ImportExtern
        [ { lang = "py"
          ; e_name = { name = snd name; typ = Some typ; default = None }
          ; e_args = []
          ; e_as = None
          ; e_from = Some (p, Id "__main__") } ]
      in
      [ p, Expr s; s' ] *) */