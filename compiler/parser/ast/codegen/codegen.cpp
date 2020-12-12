#include "util/fmt/format.h"
#include <memory>
#include <sstream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/ast/ast.h"
#include "parser/ast/codegen/codegen.h"
#include "parser/ast/codegen/codegen_ctx.h"
#include "parser/common.h"

using fmt::format;
using std::function;
using std::get;
using std::move;
using std::stack;

namespace seq {
namespace ast {

void CodegenVisitor::defaultVisit(const Expr *n) {
  seqassert(false, "invalid node {}", n->toString());
}

void CodegenVisitor::defaultVisit(const Stmt *n) {
  seqassert(false, "invalid node {}", n->toString());
}

void CodegenVisitor::defaultVisit(const Pattern *n) {
  seqassert(false, "invalid node {}", n->toString());
}

CodegenVisitor::CodegenVisitor(shared_ptr<CodegenContext> ctx)
    : ctx(move(ctx)), resultExpr(nullptr), resultStmt(nullptr), resultPattern(nullptr) {
}

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
  return transform(stmt, true);
}

seq::Stmt *CodegenVisitor::transform(const StmtPtr &stmt, bool addToBlock) {
  CodegenVisitor v(ctx);
  stmt->accept(v);
  v.setSrcInfo(stmt->getSrcInfo());
  if (v.resultStmt) {
    v.resultStmt->setSrcInfo(stmt->getSrcInfo());
    v.resultStmt->setBase(ctx->getBase());
    if (addToBlock)
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
    auto t = ptr->getType()->getClass();
    assert(t);
    v.resultPattern->setType(realizeType(t));
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
          auto p = t->parent;
          assert(in(ast->attributes, ".class"));
          if (!in(ast->attributes, ".method")) { // hack for non-generic types
            for (auto &x : ctx->cache->realizations[ast->attributes[".class"]]) {
              if (startswith(t->realizeString(), x.first)) {
                p = x.second;
                break;
              }
            }
          }
          seqassert(p && p->getClass(), "parent must be set ({}) for {}; parent={}",
                    p ? p->toString() : "-", t->toString(), ast->attributes[".class"]);
          seq::types::Type *typ = ctx->realizeType(p->getClass());
          int startI = 1;
          if (!ast->args.empty() && ast->args[0].name == "self")
            startI = 2;
          for (int i = startI; i < t->args.size(); i++)
            types.push_back(ctx->realizeType(t->args[i]->getClass()));

          auto names = split(ast->name, '.');
          auto name = names.back();
          if (isdigit(name[0])) // TODO: get rid of this hack
            name = names[names.size() - 2];
          LOG_REALIZE("[codegen] generating internal fn {} -> {}", ast->name, name);
          ctx->functions[f.first] = {typ->findMagic(name, types), true};
        } else if (in(ast->attributes, "llvm")) {
          auto fn = new seq::LLVMFunc();
          fn->setName(f.first);
          ctx->functions[f.first] = {fn, false};
        } else {
          auto fn = new seq::Func();
          fn->setName(f.first);
          ctx->functions[f.first] = {fn, false};
        }
        ctx->addFunc(f.first, ctx->functions[f.first].first);
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

void CodegenVisitor::visit(const BinaryExpr *expr) {
  assert(expr->op == "&&" || expr->op == "||");
  auto op = expr->op == "&&" ? ShortCircuitExpr::AND : ShortCircuitExpr::OR;
  resultExpr =
      new seq::ShortCircuitExpr(op, transform(expr->lexpr), transform(expr->rexpr));
}

void CodegenVisitor::visit(const IfExpr *expr) {
  resultExpr = new seq::CondExpr(transform(expr->cond), transform(expr->ifexpr),
                                 transform(expr->elsexpr));
}

void CodegenVisitor::visit(const PipeExpr *expr) {
  vector<seq::Expr *> exprs{transform(expr->items[0].expr)};
  vector<seq::types::Type *> inTypes{realizeType(expr->inTypes[0]->getClass())};
  for (int i = 1; i < expr->items.size(); i++) {
    auto e = CAST(expr->items[i].expr, CallExpr);
    assert(e);

    auto pfn = transform(e->expr);
    vector<seq::Expr *> items(e->args.size(), nullptr);
    vector<string> names(e->args.size(), "");
    vector<seq::types::Type *> partials(e->args.size(), nullptr);
    for (int ai = 0; ai < e->args.size(); ai++)
      if (!CAST(e->args[ai].value, EllipsisExpr)) {
        items[ai] = transform(e->args[ai].value);
        partials[ai] = realizeType(e->args[ai].value->getType()->getClass());
      }
    auto p = new seq::PartialCallExpr(pfn, items, names);
    p->setType(seq::types::PartialFuncType::get(pfn->getType(), partials));

    exprs.push_back(p);
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
  auto lhs = transform(expr->expr);
  vector<seq::Expr *> items;
  vector<string> names;
  for (auto &&i : expr->args) {
    if (CAST(i.value, EllipsisExpr))
      assert(false);
    else
      items.push_back(transform(i.value));
    names.emplace_back("");
  }
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

void CodegenVisitor::visit(const StmtExpr *expr) {
  vector<seq::Stmt *> stmts;
  function<void(const vector<StmtPtr> &)> traverse = [&](const vector<StmtPtr> &vss) {
    for (auto &s : vss) {
      if (auto ss = CAST(s, SuiteStmt))
        traverse(ss->stmts);
      else if (auto ss = transform(s, false))
        stmts.push_back(ss);
    }
  };
  traverse(expr->stmts);
  resultExpr = new seq::StmtExpr(stmts, transform(expr->expr));
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
  if (!stmt->rhs) {
    assert(var == ".__argv__");
    ctx->addVar(var, ctx->getModule()->getArgVar());
  } else if (stmt->rhs->isType()) {
    // ctx->addType(var, realizeType(stmt->rhs->getType()->getClass()));
  } else {
    auto varStmt = new seq::VarStmt(transform(stmt->rhs), nullptr);
    if (var[0] == '.')
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
    ctx->addBlock(
        r->addCatch(c.exc ? realizeType(c.exc->getType()->getClass()) : nullptr));
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
    auto &fp = ctx->functions[real.first];
    if (fp.second)
      continue;
    fp.second = true;

    auto ast = (FunctionStmt *)(ctx->cache->realizationAsts[real.first].get());
    assert(ast);
    if (in(ast->attributes, "internal"))
      continue;

    vector<string> names;
    vector<seq::types::Type *> types;
    auto t = real.second->getFunc();
    for (int i = 1; i < t->args.size(); i++) {
      types.push_back(realizeType(t->args[i]->getClass()));
      names.push_back(ast->args[i - 1].name);
    }

    LOG_REALIZE("[codegen] generating fn {}", real.first);
    if (in(stmt->attributes, "llvm")) {
      auto f = dynamic_cast<seq::LLVMFunc *>(fp.first);
      assert(f);
      f->setName(real.first);
      f->setIns(types);
      f->setArgNames(names);
      f->setOut(realizeType(t->args[0]->getClass()));
      // auto s = CAST(ast->suite, SuiteStmt);
      // assert(s && s->stmts.size() == 1)
      auto c = ast->suite->firstInBlock();
      assert(c);
      auto e = c->getExpr();
      assert(e);
      auto sp = CAST(e->expr, StringExpr);
      assert(sp);

      fmt::dynamic_format_arg_store<fmt::format_context> store;
      //        LOG("{}", real.first);
      //        LOG("--> {}", sp->value);
      auto &ss = ast->suite->getSuite()->stmts;
      for (int i = 1; i < ss.size(); i++) {
        auto &ex = ss[i]->getExpr()->expr;
        if (auto ei = ex->getInt()) { // static expr
          store.push_back(ei->intValue);
        } else {
          seqassert(ex->isType() && ex->getType(), "invalid LLVM type argument {}",
                    ex->toString());
          store.push_back(realizeType(ex->getType()->getClass())->getLLVMTypeStr());
        }
        //        LOG("--> {}", ex->getType() ? ex->getType()->toString() : "-");
      }
      string res = fmt::vformat(sp->value, store);
      // if (ss.size() > 1)
      // LOG("[FINAL] {} -->\n {}", real.first, res);

      std::istringstream sin(res);
      string l, declare, code;
      bool isDeclare = true;
      vector<string> lines;
      while (std::getline(sin, l)) {
        string lp = l;
        ltrim(lp);
        rtrim(lp);
        if (isDeclare && !startswith(lp, "declare ")) {
          bool isConst = lp.find("private constant") != string::npos;
          if (!isConst) {
            isDeclare = false;
            if (!lp.empty() && lp.back() != ':')
              lines.push_back("entry:");
          }
        }
        if (isDeclare)
          declare += lp + "\n";
        else
          lines.push_back(l);
      }
      f->setDeclares(declare);
      // LOG("--\n{}\n", join(lines, "\n"));
      f->setCode(join(lines, "\n"));
    } else {
      auto f = dynamic_cast<seq::Func *>(fp.first);
      assert(f);
      f->setName(real.first);
      f->setSrcInfo(getSrcInfo());
      if (!ctx->isToplevel())
        f->setEnclosingFunc(ctx->getBase());
      ctx->addBlock(f->getBlock(), f);
      f->setIns(types);
      f->setArgNames(names);
      f->setOut(realizeType(t->args[0]->getClass()));
      for (auto &a : ast->attributes) {
        f->addAttribute(a.first);
        if (a.first == "atomic")
          ctx->setFlag("atomic");
      }
      if (in(ast->attributes, ".c")) {
        auto newName = ctx->cache->reverseLookup[stmt->name];
        f->setName(newName);
        f->setExternal();
      } else {
        auto oldTryCatch = ctx->tryCatch;
        ctx->tryCatch = nullptr;
        for (auto &arg : names)
          ctx->addVar(arg, f->getArgVar(arg));

        transform(ast->suite);
        ctx->tryCatch = oldTryCatch;
      }
      ctx->popBlock();
    }
  }
} // namespace ast

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
  if (pat->prefix == "s")
    resultPattern = new seq::SeqPattern(pat->value);
  else
    resultPattern = new seq::StrPattern(pat->value);
}

void CodegenVisitor::visit(const RangePattern *pat) {
  resultPattern = new seq::RangePattern(pat->start, pat->stop);
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
  if (!pat->var.empty())
    ctx->addVar(pat->var, p->getVar());
  p->getVar()->setType(realizeType(pat->getType()->getClass()));
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
