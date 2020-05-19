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

#include "lang/seq.h"
#include "parser/ast/ast.h"
#include "parser/ast/codegen.h"
#include "parser/common.h"
#include "parser/context.h"

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

namespace seq {
namespace ast {

#if 0

void CodegenVisitor::defaultVisit(const Expr *n) {
  error(n, "invalid node {}", *n);
}

void CodegenVisitor::defaultVisit(const Stmt *n) {
  error(n, "invalid node {}", *n);
}

void CodegenVisitor::defaultVisit(const Pattern *n) {
  error(n, "invalid node {}", *n);
}

CodegenVisitor::CodegenVisitor(Context &ctx)
    : ctx(ctx), resultExpr(nullptr), resultStmt(nullptr),
      resultPattern(nullptr) {}

seq::Expr *CodegenVisitor::transform(const Expr *expr) {
  if (!expr)
    return nullptr;
  CodegenVisitor v(ctx);
  expr->accept(v);
  if (v.resultExpr) {
    v.resultExpr->setSrcInfo(expr->getSrcInfo());
    if (auto t = ctx.getTryCatch())
      v.resultExpr->setTryCatch(t);
  }
  return v.resultExpr;
}

void CodegenVisitor::visit(const NoneExpr *expr) {
  resultExpr = N<seq::NoneExpr>();
}

void CodegenVisitor::visit(const BoolExpr *expr) {
  resultExpr = N<seq::BoolExpr>(expr->value);
}

void CodegenVisitor::visit(const IntExpr *expr) {
  try {
    if (expr->suffix == "u") {
      uint64_t i = std::stoull(expr->value, nullptr, 0);
      resultExpr = N<seq::IntExpr>(i);
    } else {
      int64_t i = std::stoull(expr->value, nullptr, 0);
      resultExpr = N<seq::IntExpr>(i);
    }
  } catch (std::out_of_range &) {
    error(expr, "integer {} out of range", expr->value);
  }
}

void CodegenVisitor::visit(const FloatExpr *expr) {
  resultExpr = N<seq::FloatExpr>(expr->value);
}

void CodegenVisitor::visit(const StringExpr *expr) {
  resultExpr = N<seq::StrExpr>(expr->value);
}

void CodegenVisitor::visit(const IdExpr *expr) {
  auto i = ctx.find(expr->value);
  if (!i)
    error(expr, "identifier '{}' not found", expr->value);
  if (auto var = dynamic_cast<VarContextItem *>(i.get())) {
    if (!var->isGlobal() && var->getBase() != ctx.getBase())
      error(expr, "identifier '{}' not found", expr->value);
    if (var->isGlobal() && var->getBase() == ctx.getBase() &&
        ctx.hasFlag("atomic"))
      dynamic_cast<seq::VarExpr *>(i->getExpr())->setAtomic();
  }
  resultExpr = i->getExpr();
}

void CodegenVisitor::visit(const TupleExpr *expr) {
  vector<seq::Expr *> items;
  for (auto &&i : expr->items)
    items.push_back(transform(i));
  resultExpr = N<seq::RecordExpr>(items, vector<string>(items.size(), ""));
}

void CodegenVisitor::visit(const IfExpr *expr) {
  resultExpr = N<seq::CondExpr>(transform(expr->cond), transform(expr->eif),
                                transform(expr->eelse));
}

void CodegenVisitor::visit(const UnaryExpr *expr) {
  assert(expr->op == "!");
  resultExpr = N<seq::UOpExpr>(seq::uop(expr->op), transform(expr->expr));
}

void CodegenVisitor::visit(const BinaryExpr *expr) {
  assert(expr->op == "is" || expr->op == "&&" || expr->op == "||");
  if (expr->op == "is")
    resultExpr = N<seq::IsExpr>(transform(expr->lexpr), transform(expr->rexpr));
  else
    resultExpr = N<seq::BOpExpr>(seq::bop(expr->op), transform(expr->lexpr),
                                 transform(expr->rexpr));
}

void CodegenVisitor::visit(const PipeExpr *expr) {
  auto pexpr = new seq::PipeExpr(transform(expr->items));
  for (int i = 0; i < expr->items.size(); i++) {
    if (expr->items[i].op == "||>")
      pexpr->setParallel(i);
  }
  resultExpr = pexpr;
}

void CodegenVisitor::visit(const IndexExpr *expr) {
  // Tuple access
  resultExpr = N<seq::ArrayLookupExpr>(transform(expr->expr), transform(expr->index));
}

void CodegenVisitor::visit(const CallExpr *expr) {
  auto f = getFunction(expr->expr->getType());
  assert(f);

  // TODO: Special case: __array__ transformation
  if (f->name == "__array__") {
    assert(expr->args.size() == 1);
    resultExpr = N<seq::ArrayExpr>(transformType(f->args[0].second),
                                  transform(expr->args[0].value), true);
    return;
  }

  auto lhs = transform(expr->expr);
  vector<seq::Expr *> items;
  vector<string> names;
  for (auto &&i : expr->args) {
    items.push_back(transform(i.value));
    names.push_back("");
  }
  if (f->countPartial())
    resultExpr = N<seq::PartialCallExpr>(lhs, items, names);
  else
    resultExpr = N<seq::CallExpr>(lhs, items, names);
}

void CodegenVisitor::visit(const DotExpr *expr) {
  // Check if this is an import
  vector<string> imports;
  auto e = expr->expr.get();
  while (true) {
    if (auto en = dynamic_cast<DotExpr *>(e)) {
      imports.push_back(en->member);
      e = en->expr.get();
    } else if (auto en = dynamic_cast<IdExpr *>(e)) {
      imports.push_back(en->value);
      break;
    } else {
      imports.clear();
      break;
    }
  }
  bool isImport = imports.size();
  Context *c = &ctx;
  for (int i = imports.size() - 1; i >= 0; i--) {
    if (auto f = dynamic_cast<ImportContextItem *>(c->find(imports[i]).get())) {
      c = c->importFile(f->getFile()).get();
    } else {
      isImport = false;
      break;
    }
  }
  if (isImport) {
    // DBG(">> import {}", expr->member);
    if (auto i = c->find(expr->member)) {
      resultExpr = i->getExpr();
    } else {
      error(expr, "cannot locate '{}'", expr->member);
    }
    return;
  }

  // Not an import
  auto lhs = transform(expr->expr);
  if (auto e = dynamic_cast<seq::TypeExpr *>(lhs)) {
    // DBG(">> sta_elem {}", expr->member);
    resultExpr = N<seq::GetStaticElemExpr>(e->getType(), expr->member);
  } else {
    // DBG(">> elem {}", expr->member);
    resultExpr = N<seq::GetElemExpr>(lhs, expr->member);
  }
}

void CodegenVisitor::visit(const SliceExpr *expr) {
  error(expr, "unexpected slice");
}

void CodegenVisitor::visit(const EllipsisExpr *expr) {}

void CodegenVisitor::visit(const TypeOfExpr *expr) {
  resultExpr =
      N<seq::TypeExpr>(seq::types::GenericType::get(transform(expr->expr)));
}

void CodegenVisitor::visit(const PtrExpr *expr) {
  if (auto e = dynamic_cast<IdExpr *>(expr->expr.get())) {
    if (auto v =
            dynamic_cast<VarContextItem *>(ctx.find(e->value, true).get())) {
      resultExpr = N<seq::VarPtrExpr>(v->getVar());
    } else {
      error(expr, "identifier '{}' not found", e->value);
    }
  } else {
    error(expr, "not an identifier");
  }
}

void CodegenVisitor::visit(const LambdaExpr *expr) { error(expr, "TODO"); }

void CodegenVisitor::visit(const YieldExpr *expr) {
  resultExpr = N<seq::YieldExpr>(ctx.getBase());
}

CodegenVisitor::CodegenVisitor(Context &ctx) : ctx(ctx), result(nullptr) {}

Context &CodegenVisitor::getContext() { return ctx; }

seq::Stmt *CodegenVisitor::transform(const StmtPtr &stmt) {
  return transform(stmt.get());
}

seq::Stmt *CodegenVisitor::transform(const Stmt *stmt) {
  // if (stmt->getSrcInfo().file.find("scratch.seq") != string::npos)
  // fmt::print("<codegen> {} :pos {}\n", *stmt, stmt->getSrcInfo());
  CodegenVisitor v(ctx);
  stmt->accept(v);
  if (v.result) {
    v.result->setSrcInfo(stmt->getSrcInfo());
    v.result->setBase(ctx.getBase());
    ctx.getBlock()->add(v.result);
  }
  return v.result;
}

seq::Expr *CodegenVisitor::transform(const ExprPtr &expr) {
  return CodegenVisitor(ctx, *this).transform(expr);
}

seq::Pattern *CodegenVisitor::transform(const PatternPtr &expr) {
  return CodegenVisitor(*this).transform(expr);
}

seq::types::Type *CodegenVisitor::transformType(const ExprPtr &expr) {
  return CodegenVisitor(ctx, *this).transformType(expr);
}

void CodegenVisitor::visit(const SuiteStmt *stmt) {
  for (auto &s : stmt->stmts) {
    transform(s);
  }
}

void CodegenVisitor::visit(const PassStmt *stmt) {}

void CodegenVisitor::visit(const BreakStmt *stmt) { result = N<seq::Break>(); }

void CodegenVisitor::visit(const ContinueStmt *stmt) {
  result = N<seq::Continue>();
}

void CodegenVisitor::visit(const ExprStmt *stmt) {
  result = N<seq::ExprStmt>(transform(stmt->expr));
}

void CodegenVisitor::visit(const AssignStmt *stmt) {
  auto getAtomicOp = [](const string &op) {
    if (op == "+") {
      return seq::AtomicExpr::Op::ADD;
    } else if (op == "-") {
      return seq::AtomicExpr::Op::SUB;
    } else if (op == "&") {
      return seq::AtomicExpr::Op::AND;
    } else if (op == "|") {
      return seq::AtomicExpr::Op::OR;
    } else if (op == "^") {
      return seq::AtomicExpr::Op::XOR;
    } else if (op == "min") {
      return seq::AtomicExpr::Op::MIN;
    } else if (op == "max") {
      return seq::AtomicExpr::Op::MAX;
    } else { // TODO: XCHG, NAND
      return (seq::AtomicExpr::Op)0;
    }
  };
  /* Currently, a var can shadow a function or a type, but not another var. */
  if (auto i = dynamic_cast<IdExpr *>(stmt->lhs.get())) {
    auto var = i->value;
    auto v = dynamic_cast<VarContextItem *>(ctx.find(var, true).get());
    if (!stmt->force && v) {
      // Variable update
      bool isAtomic = v->isGlobal() && ctx.hasFlag("atomic");
      seq::AtomicExpr::Op op = (seq::AtomicExpr::Op)0;
      seq::Expr *expr = nullptr;
      if (isAtomic) {
        if (auto b = dynamic_cast<BinaryExpr *>(stmt->rhs.get())) {
          // First possibility: += / -= / other inplace operators
          op = getAtomicOp(b->op);
          if (b->inPlace && op) {
            expr = transform(b->rexpr);
          }
        } else if (auto b = dynamic_cast<CallExpr *>(stmt->rhs.get())) {
          // Second possibility: min/max operator
          if (auto i = dynamic_cast<IdExpr *>(b->expr.get())) {
            if (b->args.size() == 2 &&
                (i->value == "min" || i->value == "max")) {
              string expected = format("(#id {})", var);
              if (b->args[0].value->toString() == expected) {
                expr = transform(b->args[1].value);
              } else if (b->args[1].value->toString() == expected) {
                expr = transform(b->args[0].value);
              }
              if (expr) {
                op = getAtomicOp(i->value);
              }
            }
          }
        }
      }
      if (op && expr) {
        result = N<seq::ExprStmt>(new seq::AtomicExpr(op, v->getVar(), expr));
      } else {
        auto s = new seq::Assign(v->getVar(), transform(stmt->rhs));
        if (isAtomic) {
          s->setAtomic();
        }
        this->result = s;
        return;
      }
    } else if (!stmt->mustExist) {
      // New variable
      if (ctx.getJIT() && ctx.isToplevel()) {
        DBG("adding jit var {} = {}", var, *stmt->rhs);
        auto rhs = transform(stmt->rhs);
        ctx.execJIT(var, rhs);
        DBG("done with var {}", var);
      } else {
        auto varStmt =
            new seq::VarStmt(transform(stmt->rhs),
                             stmt->type ? transformType(stmt->type) : nullptr);
        if (ctx.isToplevel()) {
          varStmt->getVar()->setGlobal();
        }
        ctx.add(var, varStmt->getVar());
        this->result = varStmt;
      }
      return;
    }
  } else if (auto i = dynamic_cast<DotExpr *>(stmt->lhs.get())) {
    result = N<seq::AssignMember>(transform(i->expr), i->member,
                                  transform(stmt->rhs));
  } else if (auto i = dynamic_cast<IndexExpr *>(stmt->lhs.get())) {
    result = N<seq::AssignIndex>(transform(i->expr), transform(i->index),
                                 transform(stmt->rhs));
  }
  error(stmt, "invalid assignment");
}

void CodegenVisitor::visit(const AssignEqStmt *stmt) {
  error(stmt, "unexpected assignEq statement");
}

void CodegenVisitor::visit(const DelStmt *stmt) {
  if (auto expr = dynamic_cast<IdExpr *>(stmt->expr.get())) {
    if (auto v =
            dynamic_cast<VarContextItem *>(ctx.find(expr->value, true).get())) {
      ctx.remove(expr->value);
      result = N<seq::Del>(v->getVar());
    }
  } else if (auto i = dynamic_cast<IndexExpr *>(stmt->expr.get())) {
    result = N<seq::DelIndex>(transform(i->expr), transform(i->index));
  }
  error(stmt, "cannot delete non-variable");
}

void CodegenVisitor::visit(const PrintStmt *stmt) {
  result = N<seq::Print>(transform(stmt->expr), ctx.getJIT() != nullptr);
}

void CodegenVisitor::visit(const ReturnStmt *stmt) {
  if (!stmt->expr) {
    result = N<seq::Return>(nullptr);
  } else if (auto f = dynamic_cast<seq::Func *>(ctx.getBase())) {
    auto ret = new seq::Return(transform(stmt->expr));
    f->sawReturn(ret);
    this->result = ret;
  } else {
    error(stmt, "return outside function");
  }
}

void CodegenVisitor::visit(const YieldStmt *stmt) {
  if (!stmt->expr) {
    result = N<seq::Yield>(nullptr);
  } else if (auto f = dynamic_cast<seq::Func *>(ctx.getBase())) {
    auto ret = new seq::Yield(transform(stmt->expr));
    f->sawYield(ret);
    this->result = ret;
  } else {
    error(stmt, "yield outside function");
  }
}

void CodegenVisitor::visit(const AssertStmt *stmt) {
  result = N<seq::Assert>(transform(stmt->expr));
}

// void CodegenVisitor::visit(const TypeAliasStmt *stmt) {
//   ctx.add(stmt->name, transformType(stmt->expr));
// }

void CodegenVisitor::visit(const WhileStmt *stmt) {
  auto r = new seq::While(transform(stmt->cond));
  ctx.addBlock(r->getBlock());
  transform(stmt->suite);
  ctx.popBlock();
  this->result = r;
}

void CodegenVisitor::visit(const ForStmt *stmt) {
  auto r = new seq::For(transform(stmt->iter));
  string forVar;
  if (auto expr = dynamic_cast<IdExpr *>(stmt->var.get())) {
    forVar = expr->value;
  } else {
    error("expected valid assignment statement");
  }
  ctx.addBlock(r->getBlock());
  ctx.add(forVar, r->getVar());
  transform(stmt->suite);
  ctx.popBlock();
  this->result = r;
}

void CodegenVisitor::visit(const IfStmt *stmt) {
  auto r = new seq::If();
  for (auto &i : stmt->ifs) {
    auto b = i.cond ? r->addCond(transform(i.cond)) : r->addElse();
    ctx.addBlock(b);
    transform(i.suite);
    ctx.popBlock();
  }
  this->result = r;
}

void CodegenVisitor::visit(const MatchStmt *stmt) {
  auto m = new seq::Match();
  m->setValue(transform(stmt->what));
  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    string varName;
    seq::Var *var = nullptr;
    seq::Pattern *pat;
    if (auto p = dynamic_cast<BoundPattern *>(stmt->patterns[ci].get())) {
      ctx.addBlock();
      auto boundPat = new seq::BoundPattern(transform(p->pattern));
      var = boundPat->getVar();
      varName = p->var;
      pat = boundPat;
      ctx.popBlock();
    } else {
      ctx.addBlock();
      pat = transform(stmt->patterns[ci]);
      ctx.popBlock();
    }
    auto block = m->addCase(pat);
    ctx.addBlock(block);
    transform(stmt->cases[ci]);
    if (var) {
      ctx.add(varName, var);
    }
    ctx.popBlock();
  }
  this->result = m;
}

void CodegenVisitor::visit(const ImportStmt *stmt) {
  auto file =
      ctx.getCache()->getImportFile(stmt->from.first, ctx.getFilename());
  if (file == "") {
    error(stmt, "cannot locate import '{}'", stmt->from.first);
  }
  auto table = ctx.importFile(file);
  if (!stmt->what.size()) {
    ctx.add(stmt->from.second == "" ? stmt->from.first : stmt->from.second,
            file);
  } else if (stmt->what.size() == 1 && stmt->what[0].first == "*") {
    if (stmt->what[0].second != "") {
      error(stmt, "cannot rename star-import");
    }
    for (auto &i : *table) {
      ctx.add(i.first, i.second.top());
    }
  } else
    for (auto &w : stmt->what) {
      if (auto c = table->find(w.first)) {
        ctx.add(w.second == "" ? w.first : w.second, c);
      } else {
        error(stmt, "symbol '{}' not found in {}", w.first, file);
      }
    }
}

void CodegenVisitor::visit(const ExternImportStmt *stmt) {
  vector<string> names;
  vector<seq::types::Type *> types;
  for (auto &arg : stmt->args) {
    if (!arg.type) {
      error(stmt, "C imports need a type for each argument");
    }
    if (arg.name != "" &&
        std::find(names.begin(), names.end(), arg.name) != names.end()) {
      error(stmt, "argument '{}' already specified", arg.name);
    }
    names.push_back(arg.name);
    types.push_back(transformType(arg.type));
  }
  auto f = new seq::Func();
  f->setSrcInfo(stmt->getSrcInfo());
  f->setName(stmt->name.first);
  ctx.add(stmt->name.second != "" ? stmt->name.second : stmt->name.first, f,
          names);
  f->setExternal();
  f->setIns(types);
  f->setArgNames(names);
  if (!stmt->ret) {
    error(stmt, "C imports need a return type");
  }
  f->setOut(transformType(stmt->ret));
  if (ctx.getJIT() && ctx.isToplevel() && !ctx.getEnclosingType()) {
    // DBG("adding jit fn {}", stmt->name.first);
    auto fs = new seq::FuncStmt(f);
    fs->setSrcInfo(stmt->getSrcInfo());
    fs->setBase(ctx.getBase());
  } else {
    result = N<seq::FuncStmt>(f);
  }
}

void CodegenVisitor::visit(const TryStmt *stmt) {
  auto r = new seq::TryCatch();
  auto oldTryCatch = ctx.getTryCatch();
  ctx.setTryCatch(r);
  ctx.addBlock(r->getBlock());
  transform(stmt->suite);
  ctx.popBlock();
  ctx.setTryCatch(oldTryCatch);
  int varIdx = 0;
  for (auto &c : stmt->catches) {
    ctx.addBlock(r->addCatch(c.exc ? transformType(c.exc) : nullptr));
    ctx.add(c.var, r->getVar(varIdx++));
    transform(c.suite);
    ctx.popBlock();
  }
  if (stmt->finally) {
    ctx.addBlock(r->getFinally());
    transform(stmt->finally);
    ctx.popBlock();
  }
  this->result = r;
}

void CodegenVisitor::visit(const GlobalStmt *stmt) {
  if (ctx.isToplevel()) {
    error(stmt, "can only use global within function blocks");
  }
  if (auto var = dynamic_cast<VarContextItem *>(ctx.find(stmt->var).get())) {
    if (!var->isGlobal()) { // must be toplevel!
      error(stmt, "can only mark toplevel variables as global");
    }
    if (var->getBase() == ctx.getBase()) {
      error(stmt, "can only mark outer variables as global");
    }
    ctx.add(stmt->var, var->getVar(), true);
  } else {
    error(stmt, "identifier '{}' not found", stmt->var);
  }
}

void CodegenVisitor::visit(const ThrowStmt *stmt) {
  result = N<seq::Throw>(transform(stmt->expr));
}

void CodegenVisitor::visit(const FunctionStmt *stmt) {
  auto f = new seq::Func();
  f->setName(stmt->name);
  f->setSrcInfo(stmt->getSrcInfo());
  if (ctx.getEnclosingType()) {
    ctx.getEnclosingType()->addMethod(stmt->name, f, false);
  } else {
    if (!ctx.isToplevel()) {
      f->setEnclosingFunc(dynamic_cast<seq::Func *>(ctx.getBase()));
    }
    vector<string> names;
    for (auto &n : stmt->args) {
      names.push_back(n.name);
    }
    ctx.add(stmt->name, f, names);
  }
  ctx.addBlock(f->getBlock(), f);

  unordered_set<string> seen;
  auto generics = stmt->generics;
  bool hasDefault = false;
  for (auto &arg : stmt->args) {
    if (!arg.type) {
      string typName = format("'{}", arg.name);
      generics.push_back(typName);
    }
    if (seen.find(arg.name) != seen.end()) {
      error(stmt, "argument '{}' already specified", arg.name);
    }
    seen.insert(arg.name);
    if (arg.deflt) {
      hasDefault = true;
    } else if (hasDefault) {
      error(stmt, "argument '{}' has no default value", arg.name);
    }
  }
  f->addGenerics(generics.size());
  seen.clear();
  for (int g = 0; g < generics.size(); g++) {
    if (seen.find(generics[g]) != seen.end()) {
      error(stmt, "repeated generic identifier '{}'", generics[g]);
    }
    f->getGeneric(g)->setName(generics[g]);
    ctx.add(generics[g], f->getGeneric(g));
    seen.insert(generics[g]);
  }
  vector<seq::types::Type *> types;
  vector<string> names;
  vector<seq::Expr *> defaults;
  for (auto &arg : stmt->args) {
    if (!arg.type) {
      types.push_back(
          transformType(make_unique<IdExpr>(format("'{}", arg.name))));
    } else {
      types.push_back(transformType(arg.type));
    }
    names.push_back(arg.name);
    defaults.push_back(arg.deflt ? transform(arg.deflt) : nullptr);
  }
  f->setIns(types);
  f->setArgNames(names);
  f->setDefaults(defaults);

  if (stmt->ret) {
    f->setOut(transformType(stmt->ret));
  }
  for (auto a : stmt->attributes) {
    f->addAttribute(a);
    if (a == "atomic") {
      ctx.setFlag("atomic");
    }
  }
  for (auto &arg : stmt->args) {
    ctx.add(arg.name, f->getArgVar(arg.name));
  }

  auto oldEnclosing = ctx.getEnclosingType();
  // ensure that nested functions do not end up as class methods
  ctx.setEnclosingType(nullptr);
  transform(stmt->suite.get());
  ctx.setEnclosingType(oldEnclosing);
  ctx.popBlock();
  if (ctx.getJIT() && ctx.isToplevel() && !ctx.getEnclosingType()) {
    auto fs = new seq::FuncStmt(f);
    fs->setSrcInfo(stmt->getSrcInfo());
    fs->setBase(ctx.getBase());
  } else {
    result = N<seq::FuncStmt>(f);
  }
}

void CodegenVisitor::visit(const ClassStmt *stmt) {
  auto getMembers = [&]() {
    vector<seq::types::Type *> types;
    vector<string> names;
    if (stmt->isRecord && !stmt->args.size()) {
      error(stmt, "types need at least one member");
    } else
      for (auto &arg : stmt->args) {
        if (!arg.type) {
          error(stmt, "type information needed for '{}'", arg.name);
        }
        types.push_back(transformType(arg.type));
        names.push_back(arg.name);
      }
    return make_pair(types, names);
  };

  if (stmt->isRecord) {
    auto t = seq::types::RecordType::get({}, {}, stmt->name);
    ctx.add(stmt->name, t);
    ctx.setEnclosingType(t);
    ctx.addBlock();
    if (stmt->generics.size()) {
      error(stmt, "types cannot be generic");
    }
    auto tn = getMembers();
    t->setContents(tn.first, tn.second);
    transform(stmt->suite);
    ctx.popBlock();
  } else {
    auto t = seq::types::RefType::get(stmt->name);
    ctx.add(stmt->name, t);
    ctx.setEnclosingType(t);
    ctx.addBlock();
    unordered_set<string> seenGenerics;
    t->addGenerics(stmt->generics.size());
    for (int g = 0; g < stmt->generics.size(); g++) {
      if (seenGenerics.find(stmt->generics[g]) != seenGenerics.end()) {
        error(stmt, "repeated generic identifier '{}'", stmt->generics[g]);
      }
      t->getGeneric(g)->setName(stmt->generics[g]);
      ctx.add(stmt->generics[g], t->getGeneric(g));
      seenGenerics.insert(stmt->generics[g]);
    }
    auto tn = getMembers();
    t->setContents(seq::types::RecordType::get(tn.first, tn.second, ""));
    transform(stmt->suite);
    ctx.popBlock();
    t->setDone();
  }
  ctx.setEnclosingType(nullptr);
}

void CodegenVisitor::visit(const ExtendStmt *stmt) {
  vector<string> generics;
  seq::types::Type *type = nullptr;
  if (auto w = dynamic_cast<IdExpr *>(stmt->what.get())) {
    type = transformType(stmt->what);
  } else if (auto w = dynamic_cast<IndexExpr *>(stmt->what.get())) {
    type = transformType(w->expr);
    if (auto t = dynamic_cast<TupleExpr *>(w->index.get())) {
      for (auto &ti : t->items) {
        if (auto l = dynamic_cast<IdExpr *>(ti.get())) {
          generics.push_back(l->value);
        } else {
          error(stmt, "invalid generic variable");
        }
      }
    } else if (auto l = dynamic_cast<IdExpr *>(w->index.get())) {
      generics.push_back(l->value);
    } else {
      error(stmt, "invalid generic variable");
    }
  } else {
    error(stmt, "cannot extend non-type");
  }
  ctx.setEnclosingType(type);
  ctx.addBlock();
  int count = 0;
  if (auto g = dynamic_cast<seq::types::RefType *>(type)) {
    if (g->numGenerics() != generics.size()) {
      error(stmt, "generic count mismatch");
    }
    for (int i = 0; i < g->numGenerics(); i++) {
      ctx.add(generics[i], g->getGeneric(i));
    }
  } else if (count) {
    error(stmt, "unexpected generics");
  }
  transform(stmt->suite);
  ctx.popBlock();
  ctx.setEnclosingType(nullptr);
}

void CodegenVisitor::visit(const YieldFromStmt *stmt) {
  error(stmt, "unexpected yieldFrom statement");
}

void CodegenVisitor::visit(const WithStmt *stmt) {
  error(stmt, "unexpected with statement");
}

void CodegenVisitor::visit(const PyDefStmt *stmt) {
  error(stmt, "unexpected pyDef statement");
}

CodegenVisitor::CodegenVisitor(CodegenVisitor &stmtVisitor)
    : stmtVisitor(stmtVisitor), result(nullptr) {}

seq::Pattern *CodegenVisitor::transform(const PatternPtr &ptr) {
  CodegenVisitor v(stmtVisitor);
  ptr->accept(v);
  if (v.result) {
    v.result->setSrcInfo(ptr->getSrcInfo());
    if (auto t = stmtVisitor.getContext().getTryCatch()) {
      v.result->setTryCatch(t);
    }
  }
  return v.result;
}

void CodegenVisitor::visit(const StarPattern *pat) {
  result = N<seq::StarPattern>();
}

void CodegenVisitor::visit(const IntPattern *pat) {
  result = N<seq::IntPattern>(pat->value);
}

void CodegenVisitor::visit(const BoolPattern *pat) {
  result = N<seq::BoolPattern>(pat->value);
}

void CodegenVisitor::visit(const StrPattern *pat) {
  result = N<seq::StrPattern>(pat->value);
}

void CodegenVisitor::visit(const SeqPattern *pat) {
  result = N<seq::SeqPattern>(pat->value);
}

void CodegenVisitor::visit(const RangePattern *pat) {
  result = N<seq::RangePattern>(pat->start, pat->end);
}

void CodegenVisitor::visit(const TuplePattern *pat) {
  vector<seq::Pattern *> result;
  for (auto &p : pat->patterns) {
    result.push_back(transform(p));
  }
  result = N<seq::RecordPattern>(result);
}

void CodegenVisitor::visit(const ListPattern *pat) {
  vector<seq::Pattern *> result;
  for (auto &p : pat->patterns) {
    result.push_back(transform(p));
  }
  result = N<seq::ArrayPattern>(result);
}

void CodegenVisitor::visit(const OrPattern *pat) {
  vector<seq::Pattern *> result;
  for (auto &p : pat->patterns) {
    result.push_back(transform(p));
  }
  result = N<seq::OrPattern>(result);
}

void CodegenVisitor::visit(const WildcardPattern *pat) {
  auto p = new seq::Wildcard();
  if (pat->var.size()) {
    stmtVisitor.getContext().add(pat->var, p->getVar());
  }
  this->result = p;
}

void CodegenVisitor::visit(const GuardedPattern *pat) {
  result = N<seq::GuardedPattern>(transform(pat->pattern),
                                  stmtVisitor.transform(pat->cond));
}

void CodegenVisitor::visit(const BoundPattern *pat) {
  error(pat->getSrcInfo(), "unexpected bound pattern");
}

#endif

} // namespace ast
} // namespace seq
