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
#include "parser/ast/transform.h"
#include "parser/common.h"
#include "parser/context.h"
#include "parser/ocaml.h"

using fmt::format;
using std::get;
using std::make_unique;
using std::move;
using std::ostream;
using std::pair;
using std::stack;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

#define RETURN(T, ...)                                                         \
  (this->result =                                                              \
       fwdSrcInfo(make_unique<T>(__VA_ARGS__), expr->getSrcInfo()));           \
  return
#define E(T, ...) make_unique<T>(__VA_ARGS__)
#define EP(T, ...) fwdSrcInfo(make_unique<T>(__VA_ARGS__), expr->getSrcInfo())
#define EPX(e, T, ...) fwdSrcInfo(make_unique<T>(__VA_ARGS__), e->getSrcInfo())
#define S(T, ...) make_unique<T>(__VA_ARGS__)
#define SP(T, ...) fwdSrcInfo(make_unique<T>(__VA_ARGS__), stmt->getSrcInfo())
#define SPX(s, T, ...) fwdSrcInfo(make_unique<T>(__VA_ARGS__), s->getSrcInfo())
#define ERROR(s, ...) error(s->getSrcInfo(), __VA_ARGS__)

namespace seq {
namespace ast {

TransformExprVisitor::TransformExprVisitor(vector<StmtPtr> &prepend)
    : prependStmts(prepend) {}

ExprPtr TransformExprVisitor::transform(const Expr *expr) {
  TransformExprVisitor v(this->prependStmts);
  expr->accept(v);
  return move(v.result);
}

vector<ExprPtr> TransformExprVisitor::transform(const vector<ExprPtr> &exprs) {
  vector<ExprPtr> r;
  for (auto &e : exprs) {
    r.push_back(transform(e));
  }
  return r;
}

void TransformExprVisitor::visit(const EmptyExpr *expr) { RETURN(EmptyExpr, ); }

void TransformExprVisitor::visit(const BoolExpr *expr) {
  RETURN(BoolExpr, expr->value);
}

void TransformExprVisitor::visit(const IntExpr *expr) {
  RETURN(IntExpr, expr->value);
}

void TransformExprVisitor::visit(const FloatExpr *expr) {
  RETURN(FloatExpr, expr->value);
}

void TransformExprVisitor::visit(const StringExpr *expr) {
  RETURN(StringExpr, expr->value);
}

void TransformExprVisitor::visit(const FStringExpr *expr) {
  int braces_count = 0, brace_start = 0;
  vector<ExprPtr> items;
  for (int i = 0; i < expr->value.size(); i++) {
    if (expr->value[i] == '{') {
      if (brace_start < i) {
        items.push_back(
            EP(StringExpr, expr->value.substr(brace_start, i - brace_start)));
      }
      if (!braces_count) {
        brace_start = i + 1;
      }
      braces_count++;
    } else if (expr->value[i] == '}') {
      braces_count--;
      if (!braces_count) {
        string code = expr->value.substr(brace_start, i - brace_start);
        auto offset = expr->getSrcInfo();
        offset.col += i;
        if (code.size() && code.back() == '=') {
          code = code.substr(0, code.size() - 1);
          items.push_back(EP(StringExpr, format("{}=", code)));
        }
        items.push_back(EP(CallExpr, EP(IdExpr, "str"),
                           transform(parse_expr(code, offset))));
      }
      brace_start = i + 1;
    }
  }
  if (braces_count) {
    ERROR(expr, "f-string braces not balanced");
  }
  if (brace_start != expr->value.size()) {
    items.push_back(transform(
        EP(StringExpr,
           expr->value.substr(brace_start, expr->value.size() - brace_start))));
  }
  this->result = transform(EP(CallExpr, EP(DotExpr, EP(IdExpr, "str"), "cat"),
                              EP(ListExpr, move(items))));
}

void TransformExprVisitor::visit(const KmerExpr *expr) {
  this->result = transform(
      EP(CallExpr,
         EP(IndexExpr, EP(IdExpr, "Kmer"), EP(IntExpr, expr->value.size())),
         EP(SeqExpr, expr->value)));
}

void TransformExprVisitor::visit(const SeqExpr *expr) {
  if (expr->prefix == "p") {
    this->result = transform(
        EP(CallExpr, EP(IdExpr, "pseq"), EP(StringExpr, expr->value)));
  } else if (expr->prefix == "s") {
    RETURN(SeqExpr, expr->value, expr->prefix);
  } else {
    ERROR(expr, "invalid seq prefix '{}'", expr->prefix);
  }
}

void TransformExprVisitor::visit(const IdExpr *expr) {
  RETURN(IdExpr, expr->value);
}

void TransformExprVisitor::visit(const UnpackExpr *expr) {
  RETURN(CallExpr, EP(IdExpr, "list"), transform(expr->what));
}

void TransformExprVisitor::visit(const TupleExpr *expr) {
  RETURN(TupleExpr, transform(expr->items));
}

void TransformExprVisitor::visit(const ListExpr *expr) {
  RETURN(ListExpr, transform(expr->items));
  // TODO later
  if (!expr->items.size()) {
    error("empty lists are not supported");
  }
  string headVar = getTemporaryVar("head");
  string listVar = getTemporaryVar("list");
  prependStmts.push_back(
      SPX(expr, AssignStmt, EP(IdExpr, headVar), transform(expr->items[0])));
  prependStmts.push_back(SPX(
      expr, AssignStmt, EP(IdExpr, listVar),
      EP(CallExpr,
         EP(IndexExpr, EP(IdExpr, "list"), EP(TypeOfExpr, EP(IdExpr, headVar))),
         EP(IntExpr, expr->items.size()))));

#define ADD(x)                                                                 \
  prependStmts.push_back(                                                      \
      SPX(expr, ExprStmt,                                                      \
          EP(CallExpr, EP(DotExpr, EP(IdExpr, listVar), "append"), x)))
  ADD(EP(IdExpr, headVar));
  for (int i = 1; i < expr->items.size(); i++) {
    ADD(transform(expr->items[i]));
  }
#undef ADD
  RETURN(IdExpr, listVar);
}

void TransformExprVisitor::visit(const SetExpr *expr) {
  RETURN(SetExpr, transform(expr->items));
  // TODO later
  if (!expr->items.size()) {
    error("empty sets are not supported");
  }
  string headVar = getTemporaryVar("head");
  string setVar = getTemporaryVar("set");
  prependStmts.push_back(
      SPX(expr, AssignStmt, EP(IdExpr, headVar), transform(expr->items[0])));
  prependStmts.push_back(
      SPX(expr, AssignStmt, EP(IdExpr, setVar),
          EP(CallExpr, EP(IndexExpr, EP(IdExpr, "set"),
                          EP(TypeOfExpr, EP(IdExpr, headVar))))));
#define ADD(x)                                                                 \
  prependStmts.push_back(                                                      \
      SPX(expr, ExprStmt,                                                      \
          EP(CallExpr, EP(DotExpr, EP(IdExpr, setVar), "add"), x)))
  ADD(EP(IdExpr, headVar));
  for (int i = 1; i < expr->items.size(); i++) {
    ADD(transform(expr->items[i]));
  }
#undef ADD
  RETURN(IdExpr, setVar);
}

void TransformExprVisitor::visit(const DictExpr *expr) {
  vector<DictExpr::KeyValue> items;
  for (auto &i : expr->items) {
    items.push_back({transform(i.key), transform(i.value)});
  }
  RETURN(DictExpr, move(items));
  // TODO later
  if (!expr->items.size()) {
    error("empty dicts are not supported");
  }
  string headKey = getTemporaryVar("headk");
  string headVal = getTemporaryVar("headv");
  string dictVar = getTemporaryVar("dict");
  prependStmts.push_back(SPX(expr, AssignStmt, EP(IdExpr, headKey),
                             transform(expr->items[0].key)));
  prependStmts.push_back(SPX(expr, AssignStmt, EP(IdExpr, headVal),
                             transform(expr->items[0].value)));
  vector<ExprPtr> types;
  types.push_back(EP(TypeOfExpr, EP(IdExpr, headKey)));
  types.push_back(EP(TypeOfExpr, EP(IdExpr, headVal)));
  prependStmts.push_back(SPX(expr, AssignStmt, EP(IdExpr, dictVar),
                             EP(CallExpr, EP(IndexExpr, EP(IdExpr, "dict"),
                                             EP(TupleExpr, move(types))))));

#define ADD(k, v)                                                              \
  vector<ExprPtr> _s;                                                          \
  _s.push_back(k);                                                             \
  _s.push_back(v);                                                             \
  prependStmts.push_back(                                                      \
      SPX(expr, ExprStmt,                                                      \
          EP(CallExpr, EP(DotExpr, EP(IdExpr, dictVar), "__setitem__"),        \
             move(_s))))
  ADD(EP(IdExpr, headKey), EP(IdExpr, headVal));
  for (int i = 1; i < expr->items.size(); i++) {
    ADD(transform(expr->items[i].key), transform(expr->items[i].value));
  }
#undef ADD
  RETURN(IdExpr, dictVar);
}

void TransformExprVisitor::visit(const GeneratorExpr *expr) {
  vector<GeneratorExpr::Body> loops;
  for (auto &l : expr->loops) {
    loops.push_back({l.vars, transform(l.gen), transform(l.conds)});
  }
  RETURN(GeneratorExpr, expr->kind, transform(expr->expr), move(loops));
  /* TODO transform: T = list[T]() for_1: cond_1: for_2: cond_2: expr */
}

void TransformExprVisitor::visit(const DictGeneratorExpr *expr) {
  vector<GeneratorExpr::Body> loops;
  for (auto &l : expr->loops) {
    loops.push_back({l.vars, transform(l.gen), transform(l.conds)});
  }
  RETURN(DictGeneratorExpr, transform(expr->key), transform(expr->expr),
         move(loops));
}

void TransformExprVisitor::visit(const IfExpr *expr) {
  RETURN(IfExpr, transform(expr->cond), transform(expr->eif),
         transform(expr->eelse));
}

void TransformExprVisitor::visit(const UnaryExpr *expr) {
  RETURN(UnaryExpr, expr->op, transform(expr->expr));
}

void TransformExprVisitor::visit(const BinaryExpr *expr) {
  RETURN(BinaryExpr, transform(expr->lexpr), expr->op, transform(expr->rexpr));
}

void TransformExprVisitor::visit(const PipeExpr *expr) {
  vector<PipeExpr::Pipe> items;
  for (auto &l : expr->items) {
    items.push_back({l.op, transform(l.expr)});
  }
  RETURN(PipeExpr, move(items));
}

void TransformExprVisitor::visit(const IndexExpr *expr) {
  RETURN(IndexExpr, transform(expr->expr), transform(expr->index));
}

void TransformExprVisitor::visit(const CallExpr *expr) {
  // TODO: name resolution should come here!
  vector<CallExpr::Arg> args;
  for (auto &i : expr->args) {
    args.push_back({i.name, transform(i.value)});
  }
  RETURN(CallExpr, transform(expr->expr), move(args));
}

void TransformExprVisitor::visit(const DotExpr *expr) {
  RETURN(DotExpr, transform(expr->expr), expr->member);
}

void TransformExprVisitor::visit(const SliceExpr *expr) {
  string prefix;
  if (!expr->st && expr->ed) {
    prefix = "l";
  } else if (expr->st && !expr->ed) {
    prefix = "r";
  } else if (!expr->st && !expr->ed) {
    prefix = "e";
  }
  if (expr->step) {
    prefix += "s";
  }
  vector<ExprPtr> args;
  if (expr->st) {
    args.push_back(transform(expr->st));
  }
  if (expr->ed) {
    args.push_back(transform(expr->ed));
  }
  if (expr->step) {
    args.push_back(transform(expr->step));
  }
  if (!args.size()) {
    args.push_back(transform(EP(IntExpr, 0)));
  }
  // TODO: might need transform later
  this->result = EP(CallExpr, EP(IdExpr, prefix + "slice"), move(args));
}

void TransformExprVisitor::visit(const EllipsisExpr *expr) {
  RETURN(EllipsisExpr, );
}

void TransformExprVisitor::visit(const TypeOfExpr *expr) {
  RETURN(TypeOfExpr, transform(expr->expr));
}

void TransformExprVisitor::visit(const PtrExpr *expr) {
  RETURN(PtrExpr, transform(expr->expr));
}

void TransformExprVisitor::visit(const LambdaExpr *expr) {
  ERROR(expr, "TODO");
}

void TransformExprVisitor::visit(const YieldExpr *expr) { RETURN(YieldExpr, ); }

#undef RETURN
#define RETURN(T, ...)                                                         \
  (this->result =                                                              \
       fwdSrcInfo(make_unique<T>(__VA_ARGS__), stmt->getSrcInfo()));           \
  return

void TransformStmtVisitor::prepend(StmtPtr s) {
  prependStmts.push_back(move(s));
}

StmtPtr TransformStmtVisitor::transform(const Stmt *stmt) {
  // if (stmt->getSrcInfo().file.find("scratch.seq") != string::npos)
  // fmt::print("<transform> {} :pos {}\n", *stmt, stmt->getSrcInfo());
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
  if (auto l = dynamic_cast<const IndexExpr *>(lhs)) {
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
      ERROR(stmt, "only single target can be annotated");
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
    ERROR(stmt, "this expression cannot be deleted");
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
    string from = "";
    if (auto i = dynamic_cast<IdExpr *>(stmt->from.get())) {
      from = i->value;
    } else {
      ERROR(stmt, "invalid pyimport query");
    }
    auto call =
        EPX(stmt, CallExpr, // _py_import(LIB)[WHAT].call ( ...
            EPX(stmt, DotExpr,
                EPX(stmt, IndexExpr,
                    EPX(stmt, CallExpr, EPX(stmt, IdExpr, "_py_import"),
                        EPX(stmt, StringExpr, from)),
                    EPX(stmt, StringExpr, stmt->name.first)),
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
    ERROR(stmt, "language {} not yet supported", stmt->lang);
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
    ERROR(stmt, "malformed with statement");
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
  // _py_exec(""" str """)
  vector<string> args;
  for (auto &a : stmt->args) {
    args.push_back(a.name);
  }
  string code = format("def {}({}):\n{}\n", stmt->name, fmt::join(args, ", "),
                       stmt->code);
  // DBG("py code:\n{}", code);
  vector<StmtPtr> stmts;
  stmts.push_back(
      SP(ExprStmt, EPX(stmt, CallExpr, EPX(stmt, IdExpr, "_py_exec"),
                       EPX(stmt, StringExpr, code))));
  // from __main__ pyimport foo () -> ret
  stmts.push_back(transform(SP(ExternImportStmt, make_pair(stmt->name, ""),
                               EPX(stmt, IdExpr, "__main__"),
                               transform(stmt->ret), vector<Param>(), "py")));
  RETURN(SuiteStmt, move(stmts));
}

#undef RETURN
#define RETURN(T, ...)                                                         \
  (this->result = fwdSrcInfo(make_unique<T>(__VA_ARGS__), pat->getSrcInfo())); \
  return

TransformPatternVisitor::TransformPatternVisitor(
    TransformStmtVisitor &stmtVisitor)
    : stmtVisitor(stmtVisitor) {}

PatternPtr TransformPatternVisitor::transform(const Pattern *ptr) {
  TransformPatternVisitor v(stmtVisitor);
  ptr->accept(v);
  return move(v.result);
}

vector<PatternPtr>
TransformPatternVisitor::transform(const vector<PatternPtr> &pats) {
  vector<PatternPtr> r;
  for (auto &e : pats) {
    r.push_back(transform(e));
  }
  return r;
}

void TransformPatternVisitor::visit(const StarPattern *pat) {
  RETURN(StarPattern, );
}

void TransformPatternVisitor::visit(const IntPattern *pat) {
  RETURN(IntPattern, pat->value);
}

void TransformPatternVisitor::visit(const BoolPattern *pat) {
  RETURN(BoolPattern, pat->value);
}

void TransformPatternVisitor::visit(const StrPattern *pat) {
  RETURN(StrPattern, pat->value);
}

void TransformPatternVisitor::visit(const SeqPattern *pat) {
  RETURN(SeqPattern, pat->value);
}

void TransformPatternVisitor::visit(const RangePattern *pat) {
  RETURN(RangePattern, pat->start, pat->end);
}

void TransformPatternVisitor::visit(const TuplePattern *pat) {
  RETURN(TuplePattern, transform(pat->patterns));
}

void TransformPatternVisitor::visit(const ListPattern *pat) {
  RETURN(ListPattern, transform(pat->patterns));
}

void TransformPatternVisitor::visit(const OrPattern *pat) {
  RETURN(OrPattern, transform(pat->patterns));
}

void TransformPatternVisitor::visit(const WildcardPattern *pat) {
  RETURN(WildcardPattern, pat->var);
}

void TransformPatternVisitor::visit(const GuardedPattern *pat) {
  RETURN(GuardedPattern, transform(pat->pattern),
         stmtVisitor.transform(pat->cond));
}

void TransformPatternVisitor::visit(const BoundPattern *pat) {
  RETURN(BoundPattern, pat->var, transform(pat->pattern));
}

} // namespace ast
} // namespace seq
