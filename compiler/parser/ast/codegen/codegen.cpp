#include "util/fmt/format.h"
#include <memory>
#include <sstream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "parser/ast/ast/ast.h"
#include "parser/ast/codegen/codegen.h"
#include "parser/ast/codegen/codegen_ctx.h"
#include "parser/common.h"

using fmt::format;
using std::function;
using std::get;
using std::make_unique;
using std::move;
using std::stack;
using std::unique_ptr;
using std::vector;

using namespace seq::ir;

namespace seq {
namespace ast {

CodegenResult::~CodegenResult() noexcept = default;

void CodegenVisitor::defaultVisit(const Expr *n) {
  seqassert(false, "invalid node {}", *n);
}

void CodegenVisitor::defaultVisit(const Stmt *n) {
  seqassert(false, "invalid node {}", *n);
}

void CodegenVisitor::defaultVisit(const Pattern *n) {
  seqassert(false, "invalid node {}", *n);
}

CodegenVisitor::CodegenVisitor(shared_ptr<CodegenContext> ctx)
    : ctx(move(ctx)), result() {}

CodegenResult CodegenVisitor::transform(const ExprPtr &expr) {
  CodegenVisitor v(ctx);
  v.setSrcInfo(expr->getSrcInfo());
  expr->accept(v);
  return move(v.result);
}

CodegenResult CodegenVisitor::transform(const StmtPtr &stmt) {
  CodegenVisitor v(ctx);
  v.setSrcInfo(stmt->getSrcInfo());
  stmt->accept(v);
  return move(v.result);
}

SIRModulePtr CodegenVisitor::apply(shared_ptr<Cache> cache, StmtPtr stmts) {
  auto *module = new SIRModule("module");

  auto *block = module->mainFunc->body.get();
  auto ctx = make_shared<CodegenContext>(cache, block, module->mainFunc.get());

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
          vector<seq::ir::types::Type *> types;
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
          seq::ir::types::Type *typ = ctx->realizeType(p->getClass());
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

          auto fn = make_unique<seq::ir::Func>(ast->name);
          fn->setInternal(typ, name);
          ctx->functions[f.first] = {fn.get(), false};
          ctx->getModule()->globals.push_back(move(fn));
        } else if (in(ast->attributes, "llvm")) {
          auto fn = make_unique<seq::ir::Func>(ast->name);
          fn->llvm = true;
          ctx->functions[f.first] = {fn.get(), false};
          ctx->getModule()->globals.push_back(move(fn));
        } else {
          auto fn = make_unique<seq::ir::Func>(ast->name);
          ctx->functions[f.first] = {fn.get(), false};

          if (in(ast->attributes, "builtin")) {
            auto names = split(ast->name, '.');
            fn->setBuiltin(names.back());
          }

          ctx->getModule()->globals.push_back(move(fn));
        }
        ctx->addFunc(f.first, ctx->functions[f.first].first);
      }
  ctx->getBase()->body->series.push_back(make_unique<BlockFlow>("start"));
  CodegenVisitor(ctx).transform(stmts);
  return unique_ptr<SIRModule>(module);
}

void CodegenVisitor::visit(const BoolExpr *expr) {
  result = CodegenResult(
      Nx<LiteralOperand>(expr, expr->value, realizeType(expr->getType()->getClass())));
}

void CodegenVisitor::visit(const IntExpr *expr) {
  if (!expr->sign)
    result = CodegenResult(
        Nx<LiteralOperand>(expr, int64_t(uint64_t(expr->intValue)),
                           realizeType(expr->getType()->getClass())));
  else
    result = CodegenResult(Nx<LiteralOperand>(expr, expr->intValue,
                                              realizeType(expr->getType()->getClass())));
}

void CodegenVisitor::visit(const FloatExpr *expr) {
  result = CodegenResult(
      Nx<LiteralOperand>(expr, expr->value, realizeType(expr->getType()->getClass())));
}

void CodegenVisitor::visit(const StringExpr *expr) {
  result = CodegenResult(
      Nx<LiteralOperand>(expr, expr->value, realizeType(expr->getType()->getClass())));
}

void CodegenVisitor::visit(const IdExpr *expr) {
  auto val = ctx->find(expr->value);
  seqassert(val, "cannot find '{}'", expr->value);
  // TODO: this makes no sense: why setAtomic on temporary expr?
  // if (var->isGlobal() && var->getBase() == ctx->getBase() &&
  //     ctx->hasFlag("atomic"))
  //   dynamic_cast<seq::VarExpr *>(i->getExpr())->setAtomic();
  if (auto v = val->getVar())
    result = CodegenResult(Nx<VarOperand>(expr, v));
  else if (auto f = val->getFunc())
    result = CodegenResult(Nx<VarOperand>(expr, f));
  else
    result = CodegenResult(val->getType());
}

void CodegenVisitor::visit(const IfExpr *expr) {
  auto ifExprResVar =
      Nr<ir::Var>(expr, "res", realizeType(expr->getType()->getClass()));
  ctx->getBase()->vars.push_back(wrap(ifExprResVar));

  auto trueSeries = newScope(expr, "tBranch");
  ctx->addSeries(trueSeries.get());
  ctx->getInsertPoint()->instructions.push_back(Nx<AssignInstr>(
      expr, Nx<VarLvalue>(expr, ifExprResVar), toRvalue(transform(expr->eif))));
  ctx->popSeries();

  auto falseSeries = newScope(expr, "fBranch");
  ctx->addSeries(falseSeries.get());
  ctx->getInsertPoint()->instructions.push_back(Nx<AssignInstr>(
      expr, Nx<VarLvalue>(expr, ifExprResVar), toRvalue(transform(expr->eelse))));
  ctx->popSeries();

  auto check = newScope(expr, "check");
  ctx->addSeries(check.get());
  auto condResult = toOperand(transform(expr->cond));
  ctx->popSeries();

  ctx->getSeries()->series.push_back(Nx<IfFlow>(expr, "ifexpr", move(condResult),
                                                move(check), move(trueSeries),
                                                move(falseSeries)));
  ctx->getSeries()->series.push_back(Nx<BlockFlow>(expr, "ifexpr_done"));

  result = CodegenResult(Nx<VarOperand>(expr, ifExprResVar));
}

void CodegenVisitor::visit(const CallExpr *expr) {
  auto lhs = transform(expr->expr);
  vector<OperandPtr> items;
  for (auto &&i : expr->args) {
    if (CAST(i.value, EllipsisExpr))
      assert(false);
    else
      items.push_back(toOperand(transform(i.value)));
  }
  result = CodegenResult(
      Nx<CallRvalue>(expr, toOperand(transform(expr->expr)), move(items)));
  result.typeOverride = realizeType(expr->getType()->getClass());
}

void CodegenVisitor::visit(const StackAllocExpr *expr) {
  auto c = expr->typeExpr->getType()->getClass();
  assert(c);
  result = CodegenResult(Nx<StackAllocRvalue>(
      expr,
      dynamic_cast<ir::types::ArrayType *>(realizeType(expr->getType()->getClass())),
      toOperand(transform(expr->expr))));
}

void CodegenVisitor::visit(const DotExpr *expr) {
  result = CodegenResult(
      Nx<MemberRvalue>(expr, toOperand(transform(expr->expr)), expr->member));
}

void CodegenVisitor::visit(const PtrExpr *expr) {
  auto e = CAST(expr->expr, IdExpr);
  assert(e);
  auto v = ctx->find(e->value, true)->getVar();
  assert(v);
  result = CodegenResult(
      Nx<VarPointerOperand>(expr, v,
                            dynamic_cast<seq::ir::types::PointerType *>(
                                realizeType(expr->getType()->getClass()))));
}

void CodegenVisitor::visit(const YieldExpr *expr) {
  result =
      CodegenResult(Nx<YieldInRvalue>(expr, realizeType(expr->getType()->getClass())));
}

void CodegenVisitor::visit(const StmtExpr *expr) {
  ctx->addScope();
  for (auto &s : expr->stmts) {
    transform(s);
  }
  result = transform(expr->expr);
  ctx->popScope();
}

void CodegenVisitor::visit(const SuiteStmt *stmt) {
  for (auto &s : stmt->stmts)
    transform(s);
}

void CodegenVisitor::visit(const PassStmt *stmt) {}

void CodegenVisitor::visit(const BreakStmt *stmt) {
  ctx->getInsertPoint()->instructions.push_back(Nx<BreakInstr>(stmt, nearestLoop()));
}

void CodegenVisitor::visit(const ContinueStmt *stmt) {
  ctx->getInsertPoint()->instructions.push_back(Nx<ContinueInstr>(stmt, nearestLoop()));
}

void CodegenVisitor::visit(const ExprStmt *stmt) {
  ctx->getInsertPoint()->instructions.push_back(
      Nx<RvalueInstr>(stmt, toRvalue(transform(stmt->expr))));
}

void CodegenVisitor::visit(const AssignStmt *stmt) {
  /// TODO: atomic operations & JIT
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;
  if (!stmt->rhs) {
    assert(var == ".__argv__");
    if (!ctx->getModule()->argVar)
      ctx->getModule()->argVar = Nx<ir::Var>(stmt, "argv", ctx->getArgvType());
    ctx->addVar(var, ctx->getModule()->argVar.get());
  } else if (stmt->rhs->isType()) {
    // ctx->addType(var, realizeType(stmt->rhs->getType()->getClass()));
  } else {
    auto *newVar =
        Nr<ir::Var>(stmt, var, realizeType(stmt->rhs->getType()->getClass()));
    if (var[0] == '.') {
      newVar->global = true;
      ctx->getModule()->globals.push_back(wrap(newVar));
    } else {
      ctx->getBase()->vars.push_back(wrap(newVar));
    }
    ctx->addVar(var, newVar, var[0] == '.');
    ctx->getInsertPoint()->instructions.push_back(Nx<AssignInstr>(
        stmt, Nx<VarLvalue>(stmt, newVar), toRvalue(transform(stmt->rhs))));
  }
}

void CodegenVisitor::visit(const AssignMemberStmt *stmt) {
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;
  auto val = ctx->find(var, true);
  assert(val && val->getVar());

  ctx->getInsertPoint()->instructions.push_back(
      Nx<AssignInstr>(stmt, Nx<VarMemberLvalue>(stmt, val->getVar(), stmt->member),
                      toRvalue(transform(stmt->rhs))));
}

void CodegenVisitor::visit(const UpdateStmt *stmt) {
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;
  auto val = ctx->find(var, true);
  assert(val && val->getVar());
  ctx->getInsertPoint()->instructions.push_back(Nx<AssignInstr>(
      stmt, Nx<VarLvalue>(stmt, val->getVar()), toRvalue(transform(stmt->lhs))));
}

void CodegenVisitor::visit(const DelStmt *stmt) {
  auto expr = CAST(stmt->expr, IdExpr);
  assert(expr);
  auto v = ctx->find(expr->value, true)->getVar();
  assert(v);
  ctx->remove(expr->value);
}

void CodegenVisitor::visit(const ReturnStmt *stmt) {
  if (!stmt->expr) {
    ctx->getInsertPoint()->instructions.push_back(Nx<ReturnInstr>(stmt));
  } else {
    ctx->getInsertPoint()->instructions.push_back(
        Nx<ReturnInstr>(stmt, toRvalue(transform(stmt->expr))));
  }
}

void CodegenVisitor::visit(const YieldStmt *stmt) {
  if (!stmt->expr) {
    ctx->getInsertPoint()->instructions.push_back(Nx<YieldInstr>(stmt));
  } else {
    ctx->getInsertPoint()->instructions.push_back(
        Nx<YieldInstr>(stmt, toRvalue(transform(stmt->expr))));
  }
  ctx->getBase()->generator = true;
}

void CodegenVisitor::visit(const AssertStmt *stmt) {
  ctx->getInsertPoint()->instructions.push_back(
      Nx<AssertInstr>(stmt, toRvalue(transform(stmt->expr))));
}

void CodegenVisitor::visit(const WhileStmt *stmt) {
  auto checkSeries = newScope(stmt, "check");
  ctx->addSeries(checkSeries.get());
  auto operand = toOperand(transform(stmt->cond));
  ctx->popSeries();

  ctx->addScope();
  auto bodySeries = newScope(stmt, "body");
  ctx->addSeries(bodySeries.get());
  transform(stmt->suite);
  ctx->popSeries();
  ctx->popScope();

  ctx->getSeries()->series.push_back(
      Nx<WhileFlow>(stmt, "while", move(operand), move(checkSeries), move(bodySeries)));
  ctx->getSeries()->series.push_back(Nx<BlockFlow>(stmt, "while_done"));
}

void CodegenVisitor::visit(const ForStmt *stmt) {
  auto *doneVar =
      Nr<ir::Var>(stmt, "isDone", realizeType(stmt->done->getType()->getClass()));
  ctx->getBase()->vars.push_back(wrap(doneVar));

  auto varId = CAST(stmt->var, IdExpr);
  auto *resVar =
      Nr<ir::Var>(stmt, varId->value, realizeType(varId->getType()->getClass()));
  ctx->getBase()->vars.push_back(wrap(resVar));

  auto setupSeries = newScope(stmt, "setup");
  ctx->addSeries(setupSeries.get());

  ctx->getInsertPoint()->instructions.push_back(Nx<AssignInstr>(
      stmt, Nx<VarLvalue>(stmt, doneVar), toRvalue(transform(clone(stmt->done)))));

  auto setupOkBlock = Nx<BlockFlow>(stmt, "setup_ok");
  setupOkBlock->instructions.push_back(Nx<AssignInstr>(
      stmt, Nx<VarLvalue>(stmt, resVar), toRvalue(transform(clone(stmt->next)))));
  ctx->getSeries()->series.push_back(Nx<IfFlow>(stmt, "setup_if",
                                                Nx<VarOperand>(stmt, doneVar), nullptr,
                                                move(setupOkBlock), nullptr));
  ctx->popSeries();

  auto updateSeries = newScope(stmt, "update");
  ctx->addSeries(updateSeries.get());

  ctx->getInsertPoint()->instructions.push_back(Nx<AssignInstr>(
      stmt, Nx<VarLvalue>(stmt, doneVar), toRvalue(transform(clone(stmt->done)))));

  auto updateOkBlock = Nx<BlockFlow>(stmt, "update_ok");
  updateOkBlock->instructions.push_back(Nx<AssignInstr>(
      stmt, Nx<VarLvalue>(stmt, resVar), toRvalue(transform(clone(stmt->next)))));
  ctx->getSeries()->series.push_back(Nx<IfFlow>(stmt, "update_if",
                                                Nx<VarOperand>(stmt, doneVar), nullptr,
                                                move(updateOkBlock), nullptr));
  ctx->popSeries();

  ctx->addScope();
  ctx->addVar(varId->value, resVar);
  auto bodySeries = newScope(stmt, "body");
  ctx->addSeries(bodySeries.get());
  transform(stmt->suite);
  ctx->popSeries();
  ctx->popScope();

  ctx->getSeries()->series.push_back(Nx<ForFlow>(stmt, "for", move(setupSeries),
                                                 Nx<VarOperand>(stmt, doneVar), nullptr,
                                                 move(bodySeries), move(updateSeries)));
  ctx->getSeries()->series.push_back(Nx<BlockFlow>(stmt, "for_done"));
}

void CodegenVisitor::visit(const IfStmt *stmt) {
  auto trueSeries = newScope(stmt, "ifstmt_true");
  ctx->addScope();
  ctx->addSeries(trueSeries.get());
  transform(stmt->ifs[0].suite);
  ctx->popSeries();
  ctx->popScope();

  unique_ptr<SeriesFlow> falseSeries;
  if (stmt->ifs.size() > 1) {
    falseSeries = newScope(stmt, "ifstmt_false");
    ctx->addScope();
    ctx->addSeries(falseSeries.get());
    transform(stmt->ifs[1].suite);
    ctx->popSeries();
    ctx->popScope();
  }

  auto check = newScope(stmt, "ifstmt_check");
  ctx->addSeries(check.get());
  auto condResult = toOperand(transform(stmt->ifs[0].cond));
  ctx->popSeries();

  ctx->getSeries()->series.push_back(Nx<IfFlow>(stmt, "ifstmt", move(condResult),
                                                move(check), move(trueSeries),
                                                move(falseSeries)));
  ctx->getSeries()->series.push_back(Nx<BlockFlow>(stmt, "ifstmt_done"));
}

void CodegenVisitor::visit(const TryStmt *stmt) {
  auto bodySeries = newScope(stmt, "body");
  ctx->addScope();
  ctx->addSeries(bodySeries.get());
  transform(stmt->suite);
  ctx->popSeries();
  ctx->popScope();

  unique_ptr<SeriesFlow> finallySeries;
  if (stmt->finally) {
    finallySeries = newScope(stmt, "finally");
    ctx->addScope();
    ctx->addSeries(finallySeries.get());
    transform(stmt->finally);
    ctx->popSeries();
    ctx->popScope();
  }

  auto newTc =
      Nx<TryCatchFlow>(stmt, "trycatch", move(bodySeries), move(finallySeries));

  for (auto &c : stmt->catches) {
    auto catchBody = newScope(stmt, "catch");
    auto *excType = c.exc ? realizeType(c.exc->getType()->getClass()) : nullptr;

    ctx->addScope();

    ir::Var *catchVar;

    if (!c.var.empty()) {
      catchVar = Nr<ir::Var>(stmt, c.var, excType);
      ctx->addVar(c.var, catchVar);
      ctx->getBase()->vars.push_back(wrap(catchVar));
    }

    ctx->addSeries(catchBody.get());
    transform(c.suite);
    ctx->popSeries();

    ctx->popScope();

    newTc->catches.push_back(
        make_unique<TryCatchFlow::Catch>(move(catchBody), excType, catchVar));
  }

  ctx->getSeries()->series.push_back(move(newTc));
  ctx->getSeries()->series.push_back(Nx<BlockFlow>(stmt, "trycatch_done"));
}

void CodegenVisitor::visit(const ThrowStmt *stmt) {
  ctx->getInsertPoint()->instructions.push_back(
      Nx<ThrowInstr>(stmt, toRvalue(transform(stmt->expr))));
}

void CodegenVisitor::visit(const FunctionStmt *stmt) {
  for (auto &real : ctx->cache->realizations[stmt->name]) {
    auto &fp = ctx->functions[real.first];
    if (fp.second)
      continue;
    fp.second = true;

    auto ast = (FunctionStmt *)(ctx->cache->realizationAsts[real.first].get());
    assert(ast);

    vector<string> names;
    vector<seq::types::Type *> types;
    auto t = real.second->getFunc();
    for (int i = 1; i < t->args.size(); i++) {
      names.push_back(ast->args[i - 1].name);
    }

    LOG_REALIZE("[codegen] generating fn {}", real.first);
    if (in(stmt->attributes, "llvm")) {
      auto f = dynamic_cast<seq::ir::Func *>(fp.first);
      assert(f);
      f->realize(dynamic_cast<ir::types::FuncType *>(realizeType(t->getClass())),
                 names);
      // auto s = CAST(ast->suite, SuiteStmt);
      // assert(s && s->stmts.size() == 1);
      auto c = CAST(ast->suite, ExprStmt);
      assert(c);
      auto sp = CAST(c->expr, StringExpr);
      assert(sp);

      std::istringstream sin(sp->value);
      string l, declare, code;
      bool isDeclare = true;
      while (std::getline(sin, l)) {
        string lp = l;
        lp.erase(lp.begin(), std::find_if(lp.begin(), lp.end(), [](unsigned char ch) {
                   return !std::isspace(ch);
                 }));
        if (!startswith(lp, "declare "))
          isDeclare = false;
        if (isDeclare)
          declare += lp + "\n";
        else
          code += l + "\n";
      }
      f->llvmDeclares = move(declare);
      f->llvmBody = move(code);
    } else {
      auto f = dynamic_cast<seq::ir::Func *>(fp.first);
      assert(f);
      f->setSrcInfo(getSrcInfo());
      //      if (!ctx->isToplevel())
      //        f->p
      ctx->addScope();
      ctx->addSeries(f->body.get(), f);
      f->realize(dynamic_cast<ir::types::FuncType *>(realizeType(t->getClass())),
                 names);
      f->setAttribute(kFuncAttribute, make_unique<FuncAttribute>(ast->attributes));
      for (auto &a : ast->attributes) {
        if (a.first == "atomic")
          ctx->setFlag("atomic");
      }
      if (in(ast->attributes, ".c")) {
        auto newName = ctx->cache->reverseLookup[stmt->name];
        f->external = true;
      } else if (!in(ast->attributes, "internal")) {
        for (auto &arg : names)
          ctx->addVar(arg, f->getArgVar(arg));
        f->body->series.push_back(Nx<BlockFlow>(stmt, "start"));
        transform(ast->suite);
      }
      ctx->popSeries();
      ctx->popScope();
    }
  }
} // namespace ast

void CodegenVisitor::visit(const ClassStmt *stmt) {
  // visitMethods(ctx->getRealizations()->getCanonicalName(stmt->getSrcInfo()));
}

seq::ir::types::Type *CodegenVisitor::realizeType(types::ClassTypePtr t) {
  auto i = ctx->types.find(t->getClass()->realizeString());
  assert(i != ctx->types.end());
  return i->second;
}
OperandPtr CodegenVisitor::toOperand(CodegenResult r) {
  switch (r.tag) {
  case CodegenResult::OP:
    return move(r.operandResult);
  case CodegenResult::LVALUE:
    seqassert(false, "cannot convert lvalue to operand.");
    return nullptr;
  case CodegenResult::RVALUE: {
    auto srcInfoAttr =
        r.rvalueResult->getAttribute<SrcInfoAttribute>(kSrcInfoAttribute);
    auto srcInfo = srcInfoAttr ? srcInfoAttr->info : seq::SrcInfo();
    auto *t = Nr<ir::Var>(srcInfo,
                          r.typeOverride ? r.typeOverride : r.rvalueResult->getType());
    ctx->getBase()->vars.push_back(wrap(t));
    ctx->getInsertPoint()->instructions.push_back(
        Nx<AssignInstr>(srcInfo, Nx<VarLvalue>(srcInfo, t), move(r.rvalueResult)));
    return Nx<VarOperand>(srcInfo, t);
  }
  case CodegenResult::TYPE:
    seqassert(false, "cannot convert type to operand.");
    return nullptr;
  default:
    seqassert(false, "cannot convert unknown to operand.");
    return nullptr;
  }
}

seq::ir::RvaluePtr CodegenVisitor::toRvalue(CodegenResult r) {
  switch (r.tag) {
  case CodegenResult::OP: {
    auto srcInfoAttr =
        r.operandResult->getAttribute<SrcInfoAttribute>(kSrcInfoAttribute);
    auto srcInfo = srcInfoAttr ? srcInfoAttr->info : seq::SrcInfo();
    return Nx<OperandRvalue>(srcInfo, move(r.operandResult));
  }
  case CodegenResult::LVALUE:
    seqassert(false, "cannot convert lvalue to rvalue.");
    return nullptr;
  case CodegenResult::RVALUE:
    return move(r.rvalueResult);
  case CodegenResult::TYPE:
    seqassert(false, "cannot convert pattern to rvalue.");
    return nullptr;
  default:
    seqassert(false, "cannot convert unknown to rvalue.");
    return nullptr;
  }
}

std::unique_ptr<ir::SeriesFlow> CodegenVisitor::newScope(const seq::SrcObject *s,
                                                         std::string name) {
  auto ret = Nx<SeriesFlow>(s, name);
  ret->series.push_back(Nx<BlockFlow>(s, "begin"));
  return ret;
}
seq::ir::Flow *CodegenVisitor::nearestLoop() {
  seq::ir::Flow *cur = ctx->getInsertPoint();
  for (; cur && (dynamic_cast<seq::ir::ForFlow *>(cur) ||
                 dynamic_cast<seq::ir::WhileFlow *>(cur));
       cur = dynamic_cast<seq::ir::Flow *>(cur->parent)) {
  }
  return cur;
}

} // namespace ast
} // namespace seq
