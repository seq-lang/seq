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

#include "parser/ast/codegen/expr.h"
#include "parser/ast/codegen/pattern.h"
#include "parser/ast/codegen/stmt.h"
#include "parser/ast/expr.h"
#include "parser/ast/stmt.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "parser/context.h"
#include "seq/seq.h"

using fmt::format;
using std::get;
using std::make_pair;
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
  (this->result = new T(__VA_ARGS__));                                         \
  return
#define ERROR(...) error(stmt->getSrcInfo(), __VA_ARGS__)

CodegenStmtVisitor::CodegenStmtVisitor(Context &ctx)
    : ctx(ctx), result(nullptr) {}

void CodegenStmtVisitor::apply(Context &ctx, const StmtPtr &stmts) {
  auto tv = CodegenStmtVisitor(ctx);
  tv.transform(stmts);
}

Context &CodegenStmtVisitor::getContext() { return ctx; }

seq::Stmt *CodegenStmtVisitor::transform(const StmtPtr &stmt) {
  // if (stmt->getSrcInfo().file.find("scratch.seq") != string::npos)
  // fmt::print("<codegen> {} :pos {}\n", *stmt, stmt->getSrcInfo());
  CodegenStmtVisitor v(ctx);
  stmt->accept(v);
  if (v.result) {
    v.result->setSrcInfo(stmt->getSrcInfo());
    v.result->setBase(ctx.getBase());
    ctx.getBlock()->add(v.result);
  }
  return v.result;
}

seq::Expr *CodegenStmtVisitor::transform(const ExprPtr &expr) {
  CodegenExprVisitor v(ctx, *this);
  expr->accept(v);
  return v.result;
}

seq::Pattern *CodegenStmtVisitor::transform(const PatternPtr &expr) {
  CodegenPatternVisitor v(*this);
  expr->accept(v);
  return v.result;
}

seq::types::Type *CodegenStmtVisitor::transformType(const ExprPtr &expr) {
  return CodegenExprVisitor(ctx, *this).transformType(expr);
}

void CodegenStmtVisitor::visit(const SuiteStmt *stmt) {
  for (auto &s : stmt->stmts) {
    transform(s);
  }
}

void CodegenStmtVisitor::visit(const PassStmt *stmt) {}

void CodegenStmtVisitor::visit(const BreakStmt *stmt) { RETURN(seq::Break, ); }

void CodegenStmtVisitor::visit(const ContinueStmt *stmt) {
  RETURN(seq::Continue, );
}

void CodegenStmtVisitor::visit(const ExprStmt *stmt) {
  RETURN(seq::ExprStmt, transform(stmt->expr));
}

void CodegenStmtVisitor::visit(const AssignStmt *stmt) {

  // TODO: JIT
  if (auto i = dynamic_cast<IdExpr *>(stmt->lhs.get())) {
    auto var = i->value;
    if (auto v =
            dynamic_cast<VarContextItem *>(ctx.find(var).get())) { // assignment
      RETURN(seq::Assign, v->getVar(), transform(stmt->rhs));
    } else {
      auto varStmt =
          new seq::VarStmt(transform(stmt->rhs),
                           stmt->type ? transformType(stmt->type) : nullptr);
      ctx.add(var, varStmt->getVar());
      this->result = varStmt;
    }
  } else if (auto i = dynamic_cast<DotExpr *>(stmt->lhs.get())) {
    RETURN(seq::AssignMember, transform(i->expr), i->member,
           transform(stmt->rhs));
  } else if (auto i = dynamic_cast<IndexExpr *>(stmt->lhs.get())) {
    RETURN(seq::AssignIndex, transform(i->expr), transform(i->index),
           transform(stmt->rhs));
  } else {
    ERROR("invalid assignment");
  }
}

void CodegenStmtVisitor::visit(const AssignEqStmt *stmt) {
  ERROR("unexpected assignEq statement");
}

void CodegenStmtVisitor::visit(const DelStmt *stmt) {
  if (auto expr = dynamic_cast<IdExpr *>(stmt->expr.get())) {
    if (auto v = dynamic_cast<VarContextItem *>(ctx.find(expr->value).get())) {
      ctx.remove(expr->value);
      RETURN(seq::Del, v->getVar());
    }
  } else if (auto i = dynamic_cast<IndexExpr *>(stmt->expr.get())) {
    RETURN(seq::DelIndex, transform(i->expr), transform(i->index));
  }
  ERROR("cannot delete non-variable");
}

void CodegenStmtVisitor::visit(const PrintStmt *stmt) {
  RETURN(seq::Print, transform(stmt->expr));
}

void CodegenStmtVisitor::visit(const ReturnStmt *stmt) {
  if (!stmt->expr) {
    RETURN(seq::Return, nullptr);
  } else if (auto f = dynamic_cast<seq::Func *>(ctx.getBase())) {
    auto ret = new seq::Return(transform(stmt->expr));
    f->sawReturn(ret);
    this->result = ret;
  } else {
    ERROR("return outside function");
  }
}

void CodegenStmtVisitor::visit(const YieldStmt *stmt) {
  if (!stmt->expr) {
    RETURN(seq::Yield, nullptr);
  } else if (auto f = dynamic_cast<seq::Func *>(ctx.getBase())) {
    auto ret = new seq::Yield(transform(stmt->expr));
    f->sawYield(ret);
    this->result = ret;
  } else {
    ERROR("yield outside function");
  }
}

void CodegenStmtVisitor::visit(const AssertStmt *stmt) {
  RETURN(seq::Assert, transform(stmt->expr));
}

void CodegenStmtVisitor::visit(const TypeAliasStmt *stmt) {
  ctx.add(stmt->name, transformType(stmt->expr));
}

void CodegenStmtVisitor::visit(const WhileStmt *stmt) {
  auto r = new seq::While(transform(stmt->cond));
  ctx.addBlock(r->getBlock());
  transform(stmt->suite);
  ctx.popBlock();
  this->result = r;
}

void CodegenStmtVisitor::visit(const ForStmt *stmt) {
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

void CodegenStmtVisitor::visit(const IfStmt *stmt) {
  auto r = new seq::If();
  for (auto &i : stmt->ifs) {
    auto b = i.cond ? r->addCond(transform(i.cond)) : r->addElse();
    ctx.addBlock(b);
    transform(i.suite);
    ctx.popBlock();
  }
  this->result = r;
}

void CodegenStmtVisitor::visit(const MatchStmt *stmt) {
  auto m = new seq::Match();
  m->setValue(transform(stmt->what));
  for (auto &c : stmt->cases) {
    string varName;
    seq::Var *var = nullptr;
    seq::Pattern *pat;
    if (auto p = dynamic_cast<BoundPattern *>(c.first.get())) {
      ctx.addBlock();
      auto boundPat = new seq::BoundPattern(transform(p->pattern));
      var = boundPat->getVar();
      varName = p->var;
      pat = boundPat;
      ctx.popBlock();
    } else {
      ctx.addBlock();
      pat = transform(c.first);
      ctx.popBlock();
    }
    auto block = m->addCase(pat);
    ctx.addBlock(block);
    transform(c.second);
    if (var) {
      ctx.add(varName, var);
    }
    ctx.popBlock();
  }
  this->result = m;
}

void CodegenStmtVisitor::visit(const ImportStmt *stmt) {
  auto file = ctx.getCache().getImportFile(stmt->from.first, ctx.getFilename());
  if (file == "") {
    ERROR("cannot locate import '{}'", stmt->from.first);
  }
  auto table = ctx.getCache().importFile(ctx.getModule(), file);
  if (!stmt->what.size()) {
    ctx.add(stmt->from.second == "" ? stmt->from.first : stmt->from.second,
            file);
  } else if (stmt->what.size() == 1 && stmt->what[0].first == "*") {
    if (stmt->what[0].second != "") {
      ERROR("cannot rename star-import");
    }
    for (auto &i : *table) {
      ctx.add(i.first, i.second.top());
    }
  } else
    for (auto &w : stmt->what) {
      if (auto c = table->find(w.first)) {
        ctx.add(w.second == "" ? w.first : w.second, c);
      } else {
        ERROR("symbol '{}' not found in {}", w.first, file);
      }
    }
}

void CodegenStmtVisitor::visit(const ExternImportStmt *stmt) {
  vector<string> names;
  vector<seq::types::Type *> types;
  for (auto &arg : stmt->args) {
    if (!arg.type) {
      ERROR("C imports need a type for each argument");
    }
    if (arg.name != "" &&
        std::find(names.begin(), names.end(), arg.name) != names.end()) {
      ERROR("argument '{}' already specified", arg.name);
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
    ERROR("C imports need a return type");
  }
  f->setOut(transformType(stmt->ret));
  RETURN(seq::FuncStmt, f);
}

void CodegenStmtVisitor::visit(const TryStmt *stmt) {
  auto r = new seq::TryCatch();
  ctx.addBlock(r->getBlock());
  transform(stmt->suite);
  ctx.popBlock();
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

void CodegenStmtVisitor::visit(const GlobalStmt *stmt) {
  if (auto var = dynamic_cast<VarContextItem *>(ctx.find(stmt->var).get())) {
    if (var->isGlobal()) {
      if (var->getBase() != ctx.getBase()) {
        ctx.add(stmt->var, var->getVar(), true);
      }
      return;
    }
  }
  error("identifier '{}' not found", stmt->var);
}

void CodegenStmtVisitor::visit(const ThrowStmt *stmt) {
  RETURN(seq::Throw, transform(stmt->expr));
}

void CodegenStmtVisitor::visit(const PrefetchStmt *stmt) {
  if (auto e = dynamic_cast<IndexExpr *>(stmt->expr.get())) {
    if (auto f = dynamic_cast<seq::Func *>(ctx.getBase())) {
      auto r = new seq::Prefetch({transform(e->expr)}, {transform(e->index)});
      f->sawPrefetch(r);
      this->result = r;
    } else {
      ERROR("prefetch outside of function");
    }
  } else {
    ERROR("prefetch needs index expression");
  }
}

void CodegenStmtVisitor::visit(const FunctionStmt *stmt) {
  auto f = new seq::Func();
  f->setName(stmt->name);
  f->setSrcInfo(stmt->getSrcInfo());
  if (ctx.isToplevel() && ctx.getEnclosingType()) {
    // Make sure that it is toplevel--- otherwise it is a nested function
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
      ERROR("argument '{}' already specified", arg.name);
    }
    seen.insert(arg.name);
    if (arg.deflt) {
      hasDefault = true;
    } else if (hasDefault) {
      ERROR("argument '{}' has no default value", arg.name);
    }
  }
  f->addGenerics(generics.size());
  seen.clear();
  for (int g = 0; g < generics.size(); g++) {
    if (seen.find(generics[g]) != seen.end()) {
      ERROR("repeated generic identifier '{}'", generics[g]);
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
  transform(stmt->suite);
  ctx.popBlock();
  RETURN(seq::FuncStmt, f);
}

void CodegenStmtVisitor::visit(const ClassStmt *stmt) {
  auto getMembers = [&]() {
    vector<seq::types::Type *> types;
    vector<string> names;
    if (stmt->isType && !stmt->args.size()) {
      ERROR("types need at least one member");
    } else
      for (auto &arg : stmt->args) {
        if (!arg.type) {
          ERROR("type information needed for '{}'", arg.name);
        }
        types.push_back(transformType(arg.type));
        names.push_back(arg.name);
      }
    return make_pair(types, names);
  };

  if (stmt->isType) {
    auto t = seq::types::RecordType::get({}, {}, stmt->name);
    ctx.add(stmt->name, t);
    ctx.setEnclosingType(t);
    ctx.addBlock();
    if (stmt->generics.size()) {
      ERROR("types cannot be generic");
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
        ERROR("repeated generic identifier '{}'", stmt->generics[g]);
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

void CodegenStmtVisitor::visit(const ExtendStmt *stmt) {
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
          ERROR("invalid generic variable");
        }
      }
    } else if (auto l = dynamic_cast<IdExpr *>(w->index.get())) {
      generics.push_back(l->value);
    } else {
      ERROR("invalid generic variable");
    }
  } else {
    ERROR("cannot extend non-type");
  }
  ctx.setEnclosingType(type);
  ctx.addBlock();
  int count = 0;
  if (auto g = dynamic_cast<seq::types::RefType *>(type)) {
    if (g->numGenerics() != generics.size()) {
      ERROR("generic count mismatch");
    }
    for (int i = 0; i < g->numGenerics(); i++) {
      ctx.add(generics[i], g->getGeneric(i));
    }
  } else if (count) {
    ERROR("unexpected generics");
  }
  transform(stmt->suite);
  ctx.popBlock();
  ctx.setEnclosingType(nullptr);
}
