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
#include "parser/ast/codegen/codegen.h"
#include "parser/ast/codegen/codegen_ctx.h"
#include "parser/common.h"

using fmt::format;
using std::get;
using std::make_shared;
using std::make_unique;
using std::move;
using std::ostream;
using std::shared_ptr;
using std::stack;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace seq {
namespace ast {

void CodegenVisitor::defaultVisit(const Expr *n) {
  seqassert(false, "invalid node {}", *n);
}

void CodegenVisitor::defaultVisit(const Stmt *n) {
  seqassert(false, "invalid node {}", *n);
}

void CodegenVisitor::defaultVisit(const Pattern *n) {
  seqassert(false, "invalid node {}", *n);
}

CodegenVisitor::CodegenVisitor(std::shared_ptr<CodegenContext> ctx)
    : ctx(ctx), resultExpr(nullptr), resultStmt(nullptr), resultPattern(nullptr) {}

seq::Expr *CodegenVisitor::transform(const ExprPtr &expr) {
  if (!expr)
    return nullptr;
  CodegenVisitor v(ctx);
  v.setSrcInfo(expr->getSrcInfo());
  expr->accept(v);
  if (v.resultExpr) {
    v.resultExpr->setSrcInfo(expr->getSrcInfo());
    if (ctx->tryCatch)
      v.resultExpr->setTryCatch(ctx->tryCatch);
    auto t = expr->getType()->getClass();
    assert(t);
    v.resultExpr->setType(realizeType(t));
  }
  return v.resultExpr;
}

seq::Stmt *CodegenVisitor::transform(const StmtPtr &stmt) {
  CodegenVisitor v(ctx);
  stmt->accept(v);
  v.setSrcInfo(stmt->getSrcInfo());
  if (v.resultStmt) {
    v.resultStmt->setSrcInfo(stmt->getSrcInfo());
    v.resultStmt->setBase(ctx->getBase());
    ctx->getBlock()->add(v.resultStmt);
  }
  return v.resultStmt;
}

seq::Pattern *CodegenVisitor::transform(const PatternPtr &ptr) {
  CodegenVisitor v(ctx);
  v.setSrcInfo(ptr->getSrcInfo());
  ptr->accept(v);
  if (v.resultPattern) {
    v.resultPattern->setSrcInfo(ptr->getSrcInfo());
    if (ctx->tryCatch)
      v.resultPattern->setTryCatch(ctx->tryCatch);
  }
  return v.resultPattern;
}

seq::SeqModule *CodegenVisitor::apply(shared_ptr<Cache> cache, StmtPtr stmts) {
  auto module = new seq::SeqModule();
  module->setFileName("");
  auto block = module->getBlock();
  auto ctx =
      make_shared<CodegenContext>(cache, block, (seq::BaseFunc *)module, nullptr);

  // Now add all realization stubs
  for (auto &ff : cache->realizations)
    for (auto &f : ff.second) {
      auto t = ctx->realizeType(f.second->getClass());
      ctx->addType(f.first, t);
    }
  for (auto &ff : cache->realizations)
    for (auto &f : ff.second)
      if (auto t = f.second->getFunc()) {
        auto ast = (FunctionStmt *)(cache->asts[ff.first].get());
        if (in(ast->attributes, "internal")) {
          vector<seq::types::Type *> types;
          auto p = t->codegenParent ? t->codegenParent : t->parent;
          seqassert(p && p->getClass(), "parent must be set ({})",
                    p ? p->toString() : "-");
          seq::types::Type *typ = ctx->realizeType(p->getClass());
          int startI = 1;
          if (ast->args.size() && ast->args[0].name == "self")
            startI = 2;
          for (int i = startI; i < t->args.size(); i++)
            types.push_back(ctx->realizeType(t->args[i]->getClass()));

          auto names = split(ast->name, '.');
          auto name = names.back();
          if (isdigit(name[0])) // TODO: get rid of this hack
            name = names[names.size() - 2];
          LOG7("[codegen] generating internal fn {} -> {}", ast->name, name);
          ctx->functions[f.first] = typ->findMagic(name, types);
        } else {
          auto fn = new seq::Func();
          fn->setName(f.first);
          ctx->functions[f.first] = fn;
        }
        LOG7("adding {}", f.first);
        ctx->addFunc(f.first, ctx->functions[f.first]);
      }
  CodegenVisitor(ctx).transform(stmts);
  return module;
}

void CodegenVisitor::visit(const BoolExpr *expr) {
  resultExpr = new seq::BoolExpr(expr->value);
}

void CodegenVisitor::visit(const IntExpr *expr) {
  if (expr->sign)
    resultExpr = new seq::IntExpr(uint64_t(expr->intValue));
  else
    resultExpr = new seq::IntExpr(expr->intValue);
}

void CodegenVisitor::visit(const FloatExpr *expr) {
  resultExpr = new seq::FloatExpr(expr->value);
}

void CodegenVisitor::visit(const StringExpr *expr) {
  resultExpr = new seq::StrExpr(expr->value);
}

void CodegenVisitor::visit(const IdExpr *expr) {
  auto val = ctx->find(expr->value);
  seqassert(val, "cannot find '{}'", expr->value);
  // TODO: this makes no sense: why setAtomic on temporary expr?
  // if (var->isGlobal() && var->getBase() == ctx->getBase() &&
  //     ctx->hasFlag("atomic"))
  //   dynamic_cast<seq::VarExpr *>(i->getExpr())->setAtomic();
  if (auto v = val->getVar())
    resultExpr = new seq::VarExpr(v);
  else if (auto f = val->getFunc())
    resultExpr = new seq::FuncExpr(f);
  else
    resultExpr = new seq::TypeExpr(val->getType());
}

void CodegenVisitor::visit(const IfExpr *expr) {
  resultExpr = new seq::CondExpr(transform(expr->cond), transform(expr->eif),
                                 transform(expr->eelse));
}

void CodegenVisitor::visit(const PipeExpr *expr) {
  vector<seq::Expr *> exprs;
  vector<seq::types::Type *> inTypes;
  for (int i = 0; i < expr->items.size(); i++) {
    exprs.push_back(transform(expr->items[i].expr));
    inTypes.push_back(realizeType(expr->inTypes[i]->getClass()));
  }
  auto p = new seq::PipeExpr(exprs);
  p->setIntermediateTypes(inTypes);
  for (int i = 0; i < expr->items.size(); i++)
    if (expr->items[i].op == "||>")
      p->setParallel(i);
  resultExpr = p;
}

void CodegenVisitor::visit(const CallExpr *expr) {
  LOG7("-- {}", expr->expr->toString());
  auto lhs = transform(expr->expr);
  vector<seq::Expr *> items;
  vector<string> names;
  bool isPartial = false;
  for (auto &&i : expr->args) {
    if (CAST(i.value, EllipsisExpr)) {
      items.push_back(nullptr);
      isPartial = true;
    } else {
      items.push_back(transform(i.value));
    }
    names.push_back("");
  }
  if (isPartial)
    resultExpr = new seq::PartialCallExpr(lhs, items, names);
  else
    resultExpr = new seq::CallExpr(lhs, items, names);
}

void CodegenVisitor::visit(const StackAllocExpr *expr) {
  auto c = expr->typeExpr->getType()->getClass();
  assert(c);
  resultExpr = new seq::ArrayExpr(realizeType(c), transform(expr->expr), true);
}
void CodegenVisitor::visit(const DotExpr *expr) {
  resultExpr = new seq::GetElemExpr(transform(expr->expr), expr->member);
}

void CodegenVisitor::visit(const PtrExpr *expr) {
  auto e = CAST(expr->expr, IdExpr);
  assert(e);
  auto v = ctx->find(e->value, true)->getVar();
  assert(v);
  resultExpr = new seq::VarPtrExpr(v);
}

void CodegenVisitor::visit(const YieldExpr *expr) {
  resultExpr = new seq::YieldExpr(ctx->getBase());
}

void CodegenVisitor::visit(const SuiteStmt *stmt) {
  for (auto &s : stmt->stmts)
    transform(s);
}

void CodegenVisitor::visit(const PassStmt *stmt) {}

void CodegenVisitor::visit(const BreakStmt *stmt) { resultStmt = new seq::Break(); }

void CodegenVisitor::visit(const ContinueStmt *stmt) {
  resultStmt = new seq::Continue();
}

void CodegenVisitor::visit(const ExprStmt *stmt) {
  resultStmt = new seq::ExprStmt(transform(stmt->expr));
}

void CodegenVisitor::visit(const AssignStmt *stmt) {
  /// TODO: atomic operations & JIT
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;
  if (stmt->rhs->isType()) {
    // ctx->addType(var, realizeType(stmt->rhs->getType()->getClass()));
  } else {
    auto varStmt = new seq::VarStmt(transform(stmt->rhs), nullptr);
    if (ctx->isToplevel())
      varStmt->getVar()->setGlobal();
    varStmt->getVar()->setType(realizeType(stmt->rhs->getType()->getClass()));
    ctx->addVar(var, varStmt->getVar());
    resultStmt = varStmt;
  }
}

void CodegenVisitor::visit(const AssignMemberStmt *stmt) {
  resultStmt =
      new seq::AssignMember(transform(stmt->lhs), stmt->member, transform(stmt->rhs));
}

void CodegenVisitor::visit(const UpdateStmt *stmt) {
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;
  auto val = ctx->find(var, true);
  assert(val && val->getVar());
  resultStmt = new seq::Assign(val->getVar(), transform(stmt->rhs));
}

void CodegenVisitor::visit(const DelStmt *stmt) {
  auto expr = CAST(stmt->expr, IdExpr);
  assert(expr);
  auto v = ctx->find(expr->value, true)->getVar();
  assert(v);
  ctx->remove(expr->value);
  resultStmt = new seq::Del(v);
}

void CodegenVisitor::visit(const ReturnStmt *stmt) {
  if (!stmt->expr) {
    resultStmt = new seq::Return(nullptr);
  } else {
    auto ret = new seq::Return(transform(stmt->expr));
    resultStmt = ret;
  }
}

void CodegenVisitor::visit(const YieldStmt *stmt) {
  auto ret = new seq::Yield(stmt->expr ? transform(stmt->expr) : nullptr);
  ctx->getBase()->setGenerator();
  resultStmt = ret;
}

void CodegenVisitor::visit(const AssertStmt *stmt) {
  resultStmt = new seq::Assert(transform(stmt->expr));
}

void CodegenVisitor::visit(const WhileStmt *stmt) {
  auto r = new seq::While(transform(stmt->cond));
  ctx->addBlock(r->getBlock());
  transform(stmt->suite);
  ctx->popBlock();
  resultStmt = r;
}

void CodegenVisitor::visit(const ForStmt *stmt) {
  auto r = new seq::For(transform(stmt->iter));
  string forVar;
  ctx->addBlock(r->getBlock());
  auto expr = CAST(stmt->var, IdExpr);
  assert(expr);
  ctx->addVar(expr->value, r->getVar());
  r->getVar()->setType(realizeType(expr->getType()->getClass()));
  transform(stmt->suite);
  ctx->popBlock();
  resultStmt = r;
}

void CodegenVisitor::visit(const IfStmt *stmt) {
  auto r = new seq::If();
  for (auto &i : stmt->ifs) {
    ctx->addBlock(i.cond ? r->addCond(transform(i.cond)) : r->addElse());
    transform(i.suite);
    ctx->popBlock();
  }
  resultStmt = r;
}

void CodegenVisitor::visit(const MatchStmt *stmt) {
  auto m = new seq::Match();
  m->setValue(transform(stmt->what));
  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    string varName;
    seq::Var *var = nullptr;
    seq::Pattern *pat;
    if (auto p = CAST(stmt->patterns[ci], BoundPattern)) {
      ctx->addBlock();
      auto boundPat = new seq::BoundPattern(transform(p->pattern));
      var = boundPat->getVar();
      varName = p->var;
      pat = boundPat;
      ctx->popBlock();
    } else {
      ctx->addBlock();
      pat = transform(stmt->patterns[ci]);
      ctx->popBlock();
    }
    ctx->addBlock(m->addCase(pat));
    transform(stmt->cases[ci]);
    if (var)
      ctx->addVar(varName, var);
    ctx->popBlock();
  }
  resultStmt = m;
}

void CodegenVisitor::visit(const TryStmt *stmt) {
  auto r = new seq::TryCatch();
  auto oldTryCatch = ctx->tryCatch;
  ctx->tryCatch = r;
  ctx->addBlock(r->getBlock());
  transform(stmt->suite);
  ctx->popBlock();
  ctx->tryCatch = oldTryCatch;
  int varIdx = 0;
  for (auto &c : stmt->catches) {
    /// TODO: get rid of typeinfo here?
    ctx->addBlock(r->addCatch(
        c.exc->getType() ? realizeType(c.exc->getType()->getClass()) : nullptr));
    if (!c.var.empty())
      ctx->addVar(c.var, r->getVar(varIdx++));
    transform(c.suite);
    ctx->popBlock();
  }
  if (stmt->finally) {
    ctx->addBlock(r->getFinally());
    transform(stmt->finally);
    ctx->popBlock();
  }
  resultStmt = r;
}

void CodegenVisitor::visit(const ThrowStmt *stmt) {
  resultStmt = new seq::Throw(transform(stmt->expr));
}

void CodegenVisitor::visit(const FunctionStmt *stmt) {
  for (auto &real : ctx->cache->realizations[stmt->name]) {
    auto f = (seq::Func *)ctx->functions[real.first];
    assert(f);
    auto ast = (FunctionStmt *)(ctx->cache->realizationAsts[real.first].get());
    assert(ast);
    if (in(ast->attributes, "internal"))
      continue;
    LOG7("[codegen] generating fn {}", real.first);
    f->setName(real.first);
    f->setSrcInfo(getSrcInfo());
    if (!ctx->isToplevel())
      f->setEnclosingFunc(ctx->getBase());
    ctx->addBlock(f->getBlock(), f);
    vector<string> names;
    vector<seq::types::Type *> types;

    auto t = real.second->getFunc();
    for (int i = 1; i < t->args.size(); i++) {
      types.push_back(realizeType(t->args[i]->getClass()));
      names.push_back(ast->args[i - 1].name);
    }
    f->setIns(types);
    f->setArgNames(names);
    f->setOut(realizeType(t->args[0]->getClass()));
    for (auto a : ast->attributes) {
      f->addAttribute(a);
      if (a == "atomic")
        ctx->setFlag("atomic");
    }
    if (in(ast->attributes, ".c")) {
      auto newName = ctx->cache->reverseLookup[stmt->name];
      f->setName(newName);
      f->setExternal();
    } else {
      for (auto &arg : names)
        ctx->addVar(arg, f->getArgVar(arg));
      transform(ast->suite);
    }
    ctx->popBlock();
  }
}

void CodegenVisitor::visitMethods(const string &name) {
  // auto c = ctx->getRealizations()->findClass(name);
  // if (c)
  //   for (auto &m : c->methods)
  //     for (auto &mm : m.second) {
  //       FunctionStmt *f = CAST(ctx->getRealizations()->getAST(mm->name),
  //       FunctionStmt); visit(f);
  //     }
}

void CodegenVisitor::visit(const ClassStmt *stmt) {
  // visitMethods(ctx->getRealizations()->getCanonicalName(stmt->getSrcInfo()));
}

void CodegenVisitor::visit(const StarPattern *pat) {
  resultPattern = new seq::StarPattern();
}

void CodegenVisitor::visit(const IntPattern *pat) {
  resultPattern = new seq::IntPattern(pat->value);
}

void CodegenVisitor::visit(const BoolPattern *pat) {
  resultPattern = new seq::BoolPattern(pat->value);
}

void CodegenVisitor::visit(const StrPattern *pat) {
  resultPattern = new seq::StrPattern(pat->value);
}

void CodegenVisitor::visit(const SeqPattern *pat) {
  resultPattern = new seq::SeqPattern(pat->value);
}

void CodegenVisitor::visit(const RangePattern *pat) {
  resultPattern = new seq::RangePattern(pat->start, pat->end);
}

void CodegenVisitor::visit(const TuplePattern *pat) {
  vector<seq::Pattern *> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p));
  resultPattern = new seq::RecordPattern(move(pp));
}

void CodegenVisitor::visit(const ListPattern *pat) {
  vector<seq::Pattern *> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p));
  resultPattern = new seq::ArrayPattern(move(pp));
}

void CodegenVisitor::visit(const OrPattern *pat) {
  vector<seq::Pattern *> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p));
  resultPattern = new seq::OrPattern(move(pp));
}

void CodegenVisitor::visit(const WildcardPattern *pat) {
  auto p = new seq::Wildcard();
  if (pat->var.size())
    ctx->addVar(pat->var, p->getVar());
  resultPattern = p;
}

void CodegenVisitor::visit(const GuardedPattern *pat) {
  resultPattern =
      new seq::GuardedPattern(transform(pat->pattern), transform(pat->cond));
}

seq::types::Type *CodegenVisitor::realizeType(types::ClassTypePtr t) {
  auto i = ctx->types.find(t->getClass()->realizeString());
  assert(i != ctx->types.end());
  return i->second;
}

} // namespace ast
} // namespace seq
