#include "util/fmt/format.h"
#include <memory>
#include <sstream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "lang/seq.h"
#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/codegen/codegen.h"
#include "parser/visitors/codegen/codegen_ctx.h"

using fmt::format;
using std::function;
using std::get;
using std::move;
using std::stack;

namespace seq {
namespace ast {

void CodegenVisitor::defaultVisit(Expr *n) {
  seqassert(false, "invalid node {}", n->toString());
}

void CodegenVisitor::defaultVisit(Stmt *n) {
  seqassert(false, "invalid node {}", n->toString());
}

void CodegenVisitor::defaultVisit(Pattern *n) {
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
    v.resultExpr->setType(realizeType(t.get()));
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
    v.resultPattern->setType(realizeType(t.get()));
  }
  return v.resultPattern;
}

seq::SeqModule *CodegenVisitor::apply(shared_ptr<Cache> cache, StmtPtr stmts) {
  auto module = new seq::SeqModule();
  module->setFileName("");
  auto block = module->getBlock();
  auto ctx =
      make_shared<CodegenContext>(cache, block, (seq::BaseFunc *)module, nullptr);

  for (auto &ff : cache->classes)
    for (auto &f : ff.second.realizations) {
      //      LOG("[codegen] add {} -> {} | {}", f.first,
      //      f.second.type->realizeString(),
      //          f.second.llvm->getName());
      ctx->addType(f.first, f.second.llvm);
    }
  // Now add all realization stubs
  for (auto &ff : cache->functions)
    for (auto &f : ff.second.realizations) {
      auto t = f.second.type;
      assert(t);
      auto ast = cache->functions[ff.first].ast.get();
      if (in(ast->attributes, ATTR_INTERNAL)) {
        vector<seq::types::Type *> types;
        auto p = t->parent;
        assert(in(ast->attributes, ATTR_PARENT_CLASS));
        if (!in(ast->attributes, ATTR_NOT_STATIC)) { // hack for non-generic types
          for (auto &x :
               ctx->cache->classes[ast->attributes[ATTR_PARENT_CLASS]].realizations) {
            if (startswith(t->realizeString(), x.first)) {
              p = x.second.type;
              break;
            }
          }
        }
        seqassert(p && p->getClass(), "parent must be set ({}) for {}; parent={}",
                  p ? p->toString() : "-", t->toString(),
                  ast->attributes[ATTR_PARENT_CLASS]);
        seq::types::Type *typ = ctx->find(p->getClass()->realizeString())->getType();
        int startI = 1;
        if (!ast->args.empty() &&
            ctx->cache->reverseIdentifierLookup[ast->args[0].name] == "self")
          startI = 2;
        for (int i = startI; i < t->args.size(); i++)
          types.push_back(ctx->find(t->args[i]->realizeString())->getType());

        auto names = split(ast->name, '.');
        auto name = names.back();
        if (std::isdigit(name[0])) // TODO: get rid of this hack
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

void CodegenVisitor::visit(BoolExpr *expr) {
  resultExpr = new seq::BoolExpr(expr->value);
}

void CodegenVisitor::visit(IntExpr *expr) {
  resultExpr = new seq::IntExpr(expr->intValue);
}

void CodegenVisitor::visit(FloatExpr *expr) {
  resultExpr = new seq::FloatExpr(expr->value);
}

void CodegenVisitor::visit(StringExpr *expr) {
  resultExpr = new seq::StrExpr(expr->value);
}

void CodegenVisitor::visit(IdExpr *expr) {
  auto val = ctx->find(expr->value);
  seqassert(val, "cannot find '{}'", expr->value);
  if (auto v = val->getVar())
    resultExpr = new seq::VarExpr(v);
  else if (auto f = val->getFunc())
    resultExpr = new seq::FuncExpr(f);
  else
    resultExpr = new seq::TypeExpr(val->getType());
}

void CodegenVisitor::visit(BinaryExpr *expr) {
  assert(expr->op == "&&" || expr->op == "||");
  auto op = expr->op == "&&" ? ShortCircuitExpr::AND : ShortCircuitExpr::OR;
  resultExpr =
      new seq::ShortCircuitExpr(op, transform(expr->lexpr), transform(expr->rexpr));
}

void CodegenVisitor::visit(IfExpr *expr) {
  resultExpr = new seq::CondExpr(transform(expr->cond), transform(expr->ifexpr),
                                 transform(expr->elsexpr));
}

void CodegenVisitor::visit(PipeExpr *expr) {
  vector<seq::Expr *> exprs{transform(expr->items[0].expr)};
  vector<seq::types::Type *> inTypes{realizeType(expr->inTypes[0]->getClass().get())};
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
        partials[ai] = realizeType(e->args[ai].value->getType()->getClass().get());
      }
    auto p = new seq::PartialCallExpr(pfn, items, names);
    p->setType(seq::types::PartialFuncType::get(pfn->getType(), partials));

    exprs.push_back(p);
    inTypes.push_back(realizeType(expr->inTypes[i]->getClass().get()));
  }
  auto p = new seq::PipeExpr(exprs);
  p->setIntermediateTypes(inTypes);
  for (int i = 0; i < expr->items.size(); i++)
    if (expr->items[i].op == "||>")
      p->setParallel(i);
  resultExpr = p;
}

void CodegenVisitor::visit(CallExpr *expr) {
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

void CodegenVisitor::visit(StackAllocExpr *expr) {
  auto c = expr->typeExpr->getType()->getClass();
  assert(c);
  resultExpr = new seq::ArrayExpr(realizeType(c.get()), transform(expr->expr), true);
}
void CodegenVisitor::visit(DotExpr *expr) {
  resultExpr = new seq::GetElemExpr(transform(expr->expr), expr->member);
}

void CodegenVisitor::visit(PtrExpr *expr) {
  auto e = CAST(expr->expr, IdExpr);
  assert(e);
  auto v = ctx->find(e->value, true)->getVar();
  assert(v);
  resultExpr = new seq::VarPtrExpr(v);
}

void CodegenVisitor::visit(YieldExpr *expr) {
  resultExpr = new seq::YieldExpr(ctx->getBase());
}

void CodegenVisitor::visit(StmtExpr *expr) {
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

void CodegenVisitor::visit(SuiteStmt *stmt) {
  for (auto &s : stmt->stmts)
    transform(s);
}

void CodegenVisitor::visit(PassStmt *stmt) {}

void CodegenVisitor::visit(BreakStmt *stmt) { resultStmt = new seq::Break(); }

void CodegenVisitor::visit(ContinueStmt *stmt) { resultStmt = new seq::Continue(); }

void CodegenVisitor::visit(ExprStmt *stmt) {
  resultStmt = new seq::ExprStmt(transform(stmt->expr));
}

void CodegenVisitor::visit(AssignStmt *stmt) {
  /// TODO: atomic operations & JIT
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;
  if (!stmt->rhs) {
    if (var == "__argv__")
      ctx->addVar(var, ctx->getModule()->getArgVar());
    else {
      //      LOG("{} . {}", var, stmt->lhs->getType()->getClass()->toString());
      auto varStmt = new seq::VarStmt(
          nullptr, realizeType(stmt->lhs->getType()->getClass().get()));
      if (in(ctx->cache->globals, var))
        varStmt->getVar()->setGlobal();
      varStmt->getVar()->setType(realizeType(stmt->lhs->getType()->getClass().get()));
      ctx->addVar(var, varStmt->getVar());
      resultStmt = varStmt;
    }
  } else if (stmt->rhs->isType()) {
    // ctx->addType(var, realizeType(stmt->rhs->getType()->getClass()));
  } else {
    auto varStmt = new seq::VarStmt(transform(stmt->rhs), nullptr);
    if (in(ctx->cache->globals, var))
      varStmt->getVar()->setGlobal();
    varStmt->getVar()->setType(realizeType(stmt->rhs->getType()->getClass().get()));
    ctx->addVar(var, varStmt->getVar());
    resultStmt = varStmt;
  }
}

void CodegenVisitor::visit(AssignMemberStmt *stmt) {
  resultStmt =
      new seq::AssignMember(transform(stmt->lhs), stmt->member, transform(stmt->rhs));
}

void CodegenVisitor::visit(UpdateStmt *stmt) {
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;
  auto val = ctx->find(var, true);
  assert(val && val->getVar());
  resultStmt = new seq::Assign(val->getVar(), transform(stmt->rhs));
}

void CodegenVisitor::visit(DelStmt *stmt) {
  auto expr = CAST(stmt->expr, IdExpr);
  assert(expr);
  auto v = ctx->find(expr->value, true)->getVar();
  assert(v);
  ctx->remove(expr->value);
  resultStmt = new seq::Del(v);
}

void CodegenVisitor::visit(ReturnStmt *stmt) {
  if (!stmt->expr) {
    resultStmt = new seq::Return(nullptr);
  } else {
    auto ret = new seq::Return(transform(stmt->expr));
    resultStmt = ret;
  }
}

void CodegenVisitor::visit(YieldStmt *stmt) {
  auto ret = new seq::Yield(stmt->expr ? transform(stmt->expr) : nullptr);
  ctx->getBase()->setGenerator();
  resultStmt = ret;
}

void CodegenVisitor::visit(WhileStmt *stmt) {
  auto r = new seq::While(transform(stmt->cond));
  ctx->addBlock(r->getBlock());
  transform(stmt->suite);
  ctx->popBlock();
  resultStmt = r;
}

void CodegenVisitor::visit(ForStmt *stmt) {
  auto r = new seq::For(transform(stmt->iter));
  string forVar;
  ctx->addBlock(r->getBlock());
  auto expr = CAST(stmt->var, IdExpr);
  assert(expr);
  ctx->addVar(expr->value, r->getVar());
  r->getVar()->setType(realizeType(expr->getType()->getClass().get()));
  transform(stmt->suite);
  ctx->popBlock();
  resultStmt = r;
}

void CodegenVisitor::visit(IfStmt *stmt) {
  auto r = new seq::If();
  for (auto &i : stmt->ifs) {
    ctx->addBlock(i.cond ? r->addCond(transform(i.cond)) : r->addElse());
    transform(i.suite);
    ctx->popBlock();
  }
  resultStmt = r;
}

void CodegenVisitor::visit(MatchStmt *stmt) {
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

void CodegenVisitor::visit(TryStmt *stmt) {
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
        r->addCatch(c.exc ? realizeType(c.exc->getType()->getClass().get()) : nullptr));
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

void CodegenVisitor::visit(ThrowStmt *stmt) {
  resultStmt = new seq::Throw(transform(stmt->expr));
}

void CodegenVisitor::visit(FunctionStmt *stmt) {
  for (auto &real : ctx->cache->functions[stmt->name].realizations) {
    auto &fp = ctx->functions[real.first];
    if (fp.second)
      continue;
    fp.second = true;

    const auto &ast = real.second.ast;
    assert(ast);
    if (in(ast->attributes, ATTR_INTERNAL))
      continue;

    vector<string> names;
    vector<seq::types::Type *> types;
    auto t = real.second.type;
    for (int i = 1; i < t->args.size(); i++) {
      types.push_back(realizeType(t->args[i]->getClass().get()));
      names.push_back(ctx->cache->reverseIdentifierLookup[ast->args[i - 1].name]);
    }

    LOG_REALIZE("[codegen] generating fn {}", real.first);
    if (in(stmt->attributes, "llvm")) {
      auto f = dynamic_cast<seq::LLVMFunc *>(fp.first);
      assert(f);
      f->setName(real.first);
      f->setIns(types);
      f->setArgNames(names);
      f->setOut(realizeType(t->args[0]->getClass().get()));
      // auto s = CAST(tmp->suite, SuiteStmt);
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
          store.push_back(
              realizeType(ex->getType()->getClass().get())->getLLVMTypeStr());
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
      f->setOut(realizeType(t->args[0]->getClass().get()));
      for (auto &a : ast->attributes) {
        f->addAttribute(a.first);
        if (a.first == "atomic")
          ctx->setFlag("atomic");
      }
      if (in(ast->attributes, ATTR_EXTERN_C)) {
        auto newName = ctx->cache->reverseIdentifierLookup[stmt->name];
        f->setName(newName);
        f->setExternal();
      } else {
        auto oldTryCatch = ctx->tryCatch;
        ctx->tryCatch = nullptr;
        for (int i = 0; i < names.size(); i++)
          ctx->addVar(ast->args[i].name, f->getArgVar(names[i]));
        transform(ast->suite);
        ctx->tryCatch = oldTryCatch;
      }
      ctx->popBlock();
    }
  }
} // namespace tmp

void CodegenVisitor::visit(ClassStmt *stmt) {
  // visitMethods(ctx->getRealizations()->getCanonicalName(stmt->getSrcInfo()));
}

void CodegenVisitor::visit(StarPattern *pat) { resultPattern = new seq::StarPattern(); }

void CodegenVisitor::visit(IntPattern *pat) {
  resultPattern = new seq::IntPattern(pat->value);
}

void CodegenVisitor::visit(BoolPattern *pat) {
  resultPattern = new seq::BoolPattern(pat->value);
}

void CodegenVisitor::visit(StrPattern *pat) {
  if (pat->prefix == "s")
    resultPattern = new seq::SeqPattern(pat->value);
  else
    resultPattern = new seq::StrPattern(pat->value);
}

void CodegenVisitor::visit(RangePattern *pat) {
  resultPattern = new seq::RangePattern(pat->start, pat->stop);
}

void CodegenVisitor::visit(TuplePattern *pat) {
  vector<seq::Pattern *> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p));
  resultPattern = new seq::RecordPattern(move(pp));
}

void CodegenVisitor::visit(ListPattern *pat) {
  vector<seq::Pattern *> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p));
  resultPattern = new seq::ArrayPattern(move(pp));
}

void CodegenVisitor::visit(OrPattern *pat) {
  vector<seq::Pattern *> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p));
  resultPattern = new seq::OrPattern(move(pp));
}

void CodegenVisitor::visit(WildcardPattern *pat) {
  auto p = new seq::Wildcard();
  if (!pat->var.empty())
    ctx->addVar(pat->var, p->getVar());
  p->getVar()->setType(realizeType(pat->getType()->getClass().get()));
  resultPattern = p;
}

void CodegenVisitor::visit(GuardedPattern *pat) {
  resultPattern =
      new seq::GuardedPattern(transform(pat->pattern), transform(pat->cond));
}

seq::types::Type *CodegenVisitor::realizeType(types::ClassType *t) {
  auto i = ctx->find(t->getClass()->realizeString());
  seqassert(i, "type {} not realized", t->toString());
  return i->getType();
}

} // namespace ast
} // namespace seq
