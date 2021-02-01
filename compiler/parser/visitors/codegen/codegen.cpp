#include "util/fmt/format.h"
#include <memory>
#include <sstream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/codegen/codegen.h"
#include "parser/visitors/codegen/codegen_ctx.h"

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

void CodegenVisitor::defaultVisit(Expr *n) {
  seqassert(false, "invalid node {}", n->toString());
}

void CodegenVisitor::defaultVisit(Stmt *n) {
  seqassert(false, "invalid node {}", n->toString());
}

CodegenVisitor::CodegenVisitor(shared_ptr<CodegenContext> ctx)
    : ctx(std::move(ctx)), result() {}

Value *CodegenVisitor::transform(const ExprPtr &expr) {
  CodegenVisitor v(ctx);
  v.setSrcInfo(expr->getSrcInfo());
  expr->accept(v);
  return v.result;
}

seq::ir::types::Type *CodegenVisitor::realizeType(types::ClassType *t) {
  auto i = ctx->find(t->getClass()->realizedTypeName());
  seqassert(i, "type {} not realized", t->toString());
  return i->getType();
}

Value *CodegenVisitor::transform(const StmtPtr &stmt) {
  CodegenVisitor v(ctx);
  v.setSrcInfo(stmt->getSrcInfo());
  stmt->accept(v);
  return v.result;
}

IRModule *CodegenVisitor::apply(shared_ptr<Cache> cache, StmtPtr stmts) {
  auto *module = cache->module;
  auto *main = cast<BodiedFunc>(module->getMainFunc());

  auto *block = module->Nr<SeriesFlow>("body");
  main->setBody(block);

  auto ctx = make_shared<CodegenContext>(cache, block, main);

  for (auto &ff : cache->classes)
    for (auto &f : ff.second.realizations) {
      //      LOG("[codegen] add {} -> {} | {}", f.first,
      //      f.second.type->realizedName(),
      //          f.second.llvm->getName());
      ctx->addType(f.first, f.second.llvm);
    }

  // Now add all realization stubs
  for (auto &ff : cache->functions)
    for (auto &f : ff.second.realizations) {
      auto t = f.second.type;
      assert(t);
      auto *ast = cache->functions[ff.first].ast.get();
      if (in(ast->attributes, ATTR_INTERNAL)) {
        vector<ir::types::Type *> types;
        auto p = t->funcParent;
        assert(in(ast->attributes, ATTR_PARENT_CLASS));
        if (!in(ast->attributes, ATTR_NOT_STATIC)) { // hack for non-generic types
          for (auto &x :
               ctx->cache->classes[ast->attributes[ATTR_PARENT_CLASS]].realizations) {
            if (startswith(t->realizedName(), x.first)) {
              p = x.second.type;
              break;
            }
          }
        }
        seqassert(p && p->getClass(), "parent must be set ({}) for {}; parent={}",
                  p ? p->toString() : "-", t->toString(),
                  ast->attributes[ATTR_PARENT_CLASS]);

        seq::ir::types::Type *typ = ctx->find(p->getClass()->realizedName())->getType();
        int startI = 1;
        if (!ast->args.empty() &&
            ctx->cache->reverseIdentifierLookup[ast->args[0].name] == "self")
          startI = 2;
        for (int i = startI; i < t->args.size(); i++) {
          types.push_back(ctx->find(t->args[i]->realizedName())->getType());
          assert(types.back());
        }

        auto names = split(ast->name, '.');
        auto name = names.back();
        if (std::isdigit(name[0])) // TODO: get rid of this hack
          name = names[names.size() - 2];
        LOG_REALIZE("[codegen] generating internal fn {} -> {}", ast->name, name);
        auto *fn = module->Nr<seq::ir::InternalFunc>(module->getVoidRetAndArgFuncType(),
                                                     ast->name);
        fn->setParentType(typ);
        fn->setGlobal();
        ctx->functions[f.first] = {fn, false};
      } else if (in(ast->attributes, "llvm")) {
        auto *fn = module->Nr<seq::ir::LLVMFunc>(module->getVoidRetAndArgFuncType(),
                                                 ast->name);
        ctx->functions[f.first] = {fn, false};
        fn->setGlobal();
      } else if (in(ast->attributes, ".c")) {
        auto *fn = module->Nr<seq::ir::ExternalFunc>(module->getVoidRetAndArgFuncType(),
                                                     ast->name);
        ctx->functions[f.first] = {fn, false};
        fn->setGlobal();
      } else {
        auto *fn = module->Nr<seq::ir::BodiedFunc>(module->getVoidRetAndArgFuncType(),
                                                   ast->name);
        ctx->functions[f.first] = {fn, false};

        if (in(ast->attributes, "builtin")) {
          fn->setBuiltin();
        }

        fn->setGlobal();
      }
      ctx->addFunc(f.first, ctx->functions[f.first].first);
    }

  CodegenVisitor(ctx).transform(stmts);

  return module;
}

void CodegenVisitor::visit(BoolExpr *expr) {
  result = make<BoolConstant>(expr, expr->value,
                              realizeType(expr->getType()->getClass().get()));
}

void CodegenVisitor::visit(IntExpr *expr) {
  result = make<IntConstant>(expr, expr->intValue,
                             realizeType(expr->getType()->getClass().get()));
}

void CodegenVisitor::visit(FloatExpr *expr) {
  result = make<FloatConstant>(expr, expr->value,
                               realizeType(expr->getType()->getClass().get()));
}

void CodegenVisitor::visit(StringExpr *expr) {
  result = make<StringConstant>(expr, expr->value,
                                realizeType(expr->getType()->getClass().get()));
}

void CodegenVisitor::visit(IdExpr *expr) {
  auto val = ctx->find(expr->value);
  seqassert(val, "cannot find '{}'", expr->value);

  if (auto *v = val->getVar())
    result = make<VarValue>(expr, v);
  else if (auto *f = val->getFunc())
    result = make<VarValue>(expr, f);
  else
    typeResult = val->getType();
}

void CodegenVisitor::visit(IfExpr *expr) {
  result = make<TernaryInstr>(expr, transform(expr->cond), transform(expr->ifexpr),
                              transform(expr->elsexpr));
}

void CodegenVisitor::visit(CallExpr *expr) {
  vector<Value *> items;
  for (auto &&i : expr->args) {
    if (CAST(i.value, EllipsisExpr))
      assert(false);
    else
      items.push_back(transform(i.value));
  }
  result = make<CallInstr>(expr, transform(expr->expr), move(items));
}

void CodegenVisitor::visit(StackAllocExpr *expr) {
  auto c = expr->typeExpr->getType()->getClass();
  assert(c);
  auto val = CAST(expr->expr, IntExpr);
  assert(val);
  result = make<StackAllocInstr>(
      expr, ctx->getModule()->getArrayType(realizeType(c.get())), val->intValue);
}

void CodegenVisitor::visit(DotExpr *expr) {
  if (expr->member == "__atomic__" || expr->member == "__elemsize__") {
    auto *idExpr = dynamic_cast<IdExpr *>(expr->expr.get());
    assert(idExpr);
    auto *type = ctx->find(idExpr->value)->getType();
    assert(type);
    result = make<TypePropertyInstr>(expr, type,
                                     expr->member == "__atomic__"
                                         ? TypePropertyInstr::Property::IS_ATOMIC
                                         : TypePropertyInstr::Property::SIZEOF);
    return;
  }

  result = make<ExtractInstr>(expr, transform(expr->expr), expr->member);
}

void CodegenVisitor::visit(PtrExpr *expr) {
  auto *i = CAST(expr->expr, IdExpr);
  assert(i);
  auto var = i->value;
  auto val = ctx->find(var, true);
  assert(val && val->getVar());

  result = make<PointerValue>(expr, val->getVar());
}

void CodegenVisitor::visit(YieldExpr *expr) {
  result = make<YieldInInstr>(expr, realizeType(expr->getType()->getClass().get()));
}

void CodegenVisitor::visit(StmtExpr *expr) {
  // ctx->addScope();

  auto *bodySeries = newScope(expr, "body");
  ctx->addSeries(bodySeries);
  for (auto &s : expr->stmts)
    transform(s);
  ctx->popSeries();
  result = make<FlowInstr>(expr, bodySeries, transform(expr->expr));

  // ctx->popScope();
}

void CodegenVisitor::visit(PipeExpr *expr) {
  auto isGen = [](const Value *v) -> bool {
    auto *type = v->getType();
    if (isA<ir::types::GeneratorType>(type))
      return true;
    else if (auto *fn = cast<ir::types::FuncType>(type)) {
      return isA<ir::types::GeneratorType>(fn->getReturnType());
    }
    return false;
  };

  vector<PipelineFlow::Stage> stages;

  auto *firstStage = transform(expr->items[0].expr);
  auto firstIsGen = isGen(firstStage);
  stages.emplace_back(firstStage, std::vector<Value *>(), firstIsGen, false);

  auto sugar = !firstIsGen;

  for (auto i = 1; i < expr->items.size(); ++i) {
    auto &item = expr->items[i];
    auto *call = CAST(item.expr, CallExpr);
    assert(call);

    auto *fn = transform(call->expr);
    auto genStage = isGen(fn);

    if (i + 1 != expr->items.size())
      sugar = sugar && !genStage;

    vector<Value *> args(call->args.size());
    for (auto j = 0; j < call->args.size(); ++j) {
      args[j] = transform(call->args[j].value);
    }
    stages.emplace_back(fn, args, genStage, false);
  }

  if (sugar) {
    result = stages[0].getFunc()->clone();

    for (auto i = 1; i < stages.size(); ++i) {
      auto &stage = stages[i];
      std::vector<Value *> newArgs;
      for (auto *arg : stage) {
        newArgs.push_back(arg ? arg->clone() : result);
      }
      result = make<CallInstr>(expr, stage.getFunc()->clone(), newArgs);
    }
    return;
  }

  for (int i = 0; i < expr->items.size(); i++)
    if (expr->items[i].op == "||>")
      stages[i].setParallel();

  ctx->getSeries()->push_back(make<PipelineFlow>(expr, stages));
}

void CodegenVisitor::visit(EllipsisExpr *expr) {}

void CodegenVisitor::visit(SuiteStmt *stmt) {
  for (auto &s : stmt->stmts)
    transform(s);
}

void CodegenVisitor::visit(PassStmt *stmt) {}

void CodegenVisitor::visit(BreakStmt *stmt) {
  ctx->getSeries()->push_back(make<BreakInstr>(stmt, ctx->getLoop()));
}

void CodegenVisitor::visit(ContinueStmt *stmt) {
  ctx->getSeries()->push_back(make<ContinueInstr>(stmt, ctx->getLoop()));
}

void CodegenVisitor::visit(ExprStmt *stmt) {
  auto *r = transform(stmt->expr);
  if (r)
    ctx->getSeries()->push_back(r);
}

void CodegenVisitor::visit(AssignStmt *stmt) {
  /// TODO: atomic operations & JIT
  auto *i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;

  auto *module = ctx->getModule();

  if (!stmt->rhs) {
    if (var == ".__argv__") {
      ctx->addVar(var, module->getArgVar());
    } else {
      auto *newVar =
          make<ir::Var>(stmt, realizeType(stmt->lhs->getType()->getClass().get()),
                        in(ctx->cache->globals, var), var);
      if (!in(ctx->cache->globals, var)) {
        ctx->getBase()->push_back(newVar);
      }
      ctx->addVar(var, newVar, in(ctx->cache->globals, var));
    }
  } else if (stmt->rhs->isType()) {
    // ctx->addType(var, realizeType(stmt->rhs->getType()->getClass()));
  } else {
    auto *newVar =
        make<ir::Var>(stmt, realizeType(stmt->rhs->getType()->getClass().get()),
                      in(ctx->cache->globals, var), var);
    if (!in(ctx->cache->globals, var)) {
      ctx->getBase()->push_back(newVar);
    }
    ctx->addVar(var, newVar, var[0] == '.');
    ctx->getSeries()->push_back(make<AssignInstr>(stmt, newVar, transform(stmt->rhs)));
  }
}

void CodegenVisitor::visit(AssignMemberStmt *stmt) {
  ctx->getSeries()->push_back(make<InsertInstr>(stmt, transform(stmt->lhs),
                                                stmt->member, transform(stmt->rhs)));
}

void CodegenVisitor::visit(UpdateStmt *stmt) {
  auto *i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;
  auto val = ctx->find(var, true);
  assert(val && val->getVar());

  ctx->getSeries()->push_back(
      make<AssignInstr>(stmt, val->getVar(), transform(stmt->rhs)));
}

void CodegenVisitor::visit(ReturnStmt *stmt) {
  Value *value = nullptr;
  if (stmt->expr)
    value = transform(stmt->expr);

  ctx->getSeries()->push_back(make<ReturnInstr>(stmt, value));
}

void CodegenVisitor::visit(YieldStmt *stmt) {
  Value *value = nullptr;
  if (stmt->expr)
    value = transform(stmt->expr);

  ctx->getSeries()->push_back(make<YieldInstr>(stmt, value));

  ctx->getBase()->setGenerator();
}

void CodegenVisitor::visit(WhileStmt *stmt) {
  auto *loop = make<WhileFlow>(stmt, transform(stmt->cond), newScope(stmt, "body"));

  ctx->addLoop(loop);
  // ctx->addScope();
  ctx->addSeries(cast<SeriesFlow>(loop->getBody()));
  transform(stmt->suite);
  ctx->popSeries();
  // ctx->popScope();
  ctx->popLoop();

  ctx->getSeries()->push_back(loop);
}

void CodegenVisitor::visit(ForStmt *stmt) {
  auto *varId = CAST(stmt->var, IdExpr);
  auto *resVar = make<ir::Var>(stmt, realizeType(varId->getType()->getClass().get()),
                               false, varId->value);
  ctx->getBase()->push_back(resVar);

  auto *bodySeries = newScope(stmt, "body");
  auto *loop = make<ForFlow>(stmt, transform(stmt->iter), bodySeries, resVar);
  ctx->addLoop(loop);
  // ctx->addScope();
  ctx->addVar(varId->value, resVar);

  ctx->addSeries(cast<SeriesFlow>(loop->getBody()));
  transform(stmt->suite);
  ctx->popSeries();
  // ctx->popScope();
  ctx->popLoop();

  ctx->getSeries()->push_back(loop);
}

void CodegenVisitor::visit(IfStmt *stmt) {
  if (!stmt->ifs[0].cond) {
    // ctx->addScope();
    transform(stmt->ifs[0].suite);
    // ctx->popScope();
    return;
  }

  auto *cond = transform(stmt->ifs[0].cond);
  auto *trueSeries = newScope(stmt, "ifstmt_true");
  // ctx->addScope();
  ctx->addSeries(trueSeries);
  transform(stmt->ifs[0].suite);
  ctx->popSeries();
  // ctx->popScope();

  SeriesFlow *falseSeries = nullptr;
  if (stmt->ifs.size() > 1) {
    falseSeries = newScope(stmt, "ifstmt_false");
    // ctx->addScope();
    ctx->addSeries(falseSeries);
    transform(stmt->ifs[1].suite);
    ctx->popSeries();
    // ctx->popScope();
  }

  ctx->getSeries()->push_back(make<IfFlow>(stmt, cond, trueSeries, falseSeries));
}

void CodegenVisitor::visit(TryStmt *stmt) {
  auto *bodySeries = newScope(stmt, "body");
  // ctx->addScope();
  ctx->addSeries(bodySeries);
  transform(stmt->suite);
  ctx->popSeries();
  // ctx->popScope();

  SeriesFlow *finallySeries = nullptr;
  if (stmt->finally) {
    finallySeries = newScope(stmt, "finally");
    // ctx->addScope();
    ctx->addSeries(finallySeries);
    transform(stmt->finally);
    ctx->popSeries();
    // ctx->popScope();
  }

  auto *newTc = make<TryCatchFlow>(stmt, bodySeries, finallySeries);

  for (auto &c : stmt->catches) {
    auto *catchBody = newScope(stmt, "catch");
    auto *excType = c.exc ? realizeType(c.exc->getType()->getClass().get()) : nullptr;

    // ctx->addScope();

    ir::Var *catchVar = nullptr;
    if (!c.var.empty()) {
      catchVar = make<ir::Var>(stmt, excType, false, c.var);
      ctx->addVar(c.var, catchVar);
      ctx->getBase()->push_back(catchVar);
    }

    ctx->addSeries(catchBody);
    transform(c.suite);
    ctx->popSeries();

    // ctx->popScope();

    newTc->push_back(TryCatchFlow::Catch(catchBody, excType, catchVar));
  }

  ctx->getSeries()->push_back(newTc);
}

void CodegenVisitor::visit(ThrowStmt *stmt) {
  ctx->getSeries()->push_back(make<ThrowInstr>(stmt, transform(stmt->expr)));
}

void CodegenVisitor::visit(FunctionStmt *stmt) {
  for (auto &real : ctx->cache->functions[stmt->name].realizations) {
    auto &fp = ctx->functions[real.first];
    if (fp.second)
      continue;
    fp.second = true;

    const auto &ast = real.second.ast;
    assert(ast);

    vector<string> names;
    vector<seq::ir::types::Type *> types;
    auto t = real.second.type;
    for (int i = 1; i < t->args.size(); i++) {
      types.push_back(realizeType(t->args[i]->getClass().get()));
      names.push_back(ctx->cache->reverseIdentifierLookup[ast->args[i - 1].name]);
    }

    auto *funcType =
        ctx->getModule()->getFuncType(realizeType(t->args[0]->getClass().get()), types);

    cast<ir::Func>(fp.first)->setSrcInfo(getSrcInfo());

    LOG_REALIZE("[codegen] generating fn {}", real.first);
    if (in(stmt->attributes, "llvm")) {
      auto *f = cast<ir::LLVMFunc>(fp.first);
      assert(f);
      f->realize(cast<ir::types::FuncType>(funcType), names);

      // auto *s = CAST(tmp->suite, SuiteStmt);
      // assert(s && s->stmts.size() == 1)
      auto *c = ast->suite->firstInBlock();
      assert(c);
      auto *e = c->getExpr();
      assert(e);
      auto *sp = CAST(e->expr, StringExpr);
      assert(sp);

      std::vector<ir::LLVMFunc::LLVMLiteral> literals;
      auto &ss = ast->suite->getSuite()->stmts;
      for (int i = 1; i < ss.size(); i++) {
        auto &ex = ss[i]->getExpr()->expr;
        if (auto *ei = ex->getInt()) { // static expr
          literals.emplace_back(ei->intValue);
        } else {
          seqassert(ex->isType() && ex->getType(), "invalid LLVM type argument");
          literals.emplace_back(realizeType(ex->getType()->getClass().get()));
        }
      }

      std::istringstream sin(sp->value);
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
      f->setLLVMBody(join(lines, "\n"));
      f->setLLVMDeclarations(declare);
      f->setLLVMLiterals(literals);
    } else {
      auto *f = cast<ir::Func>(fp.first);
      assert(f);

      f->realize(cast<ir::types::FuncType>(funcType), names);
      f->setAttribute(make_unique<FuncAttribute>(ast->attributes));
      for (auto &a : ast->attributes) {
        if (a.first == "atomic")
          ctx->setFlag("atomic");
      }
      if (in(ast->attributes, ATTR_EXTERN_C)) {
        auto *external = cast<ir::ExternalFunc>(f);
        assert(external);
        external->setUnmangledName(ctx->cache->reverseIdentifierLookup[stmt->name]);
      } else if (!in(ast->attributes, "internal")) {
        ctx->addScope();
        for (auto i = 0; i < names.size(); ++i) {
          auto *var = f->getArgVar(names[i]);
          assert(var);
          ctx->addVar(ast->args[i].name, var);
        }

        auto *body = newScope(stmt, "body");
        ctx->addSeries(body, f);
        transform(ast->suite);
        ctx->popSeries();

        auto *bodied = cast<ir::BodiedFunc>(f);
        assert(bodied);

        bodied->setBody(body);
        ctx->popScope();
      }
    }
    for (auto i = 0; i < names.size(); ++i) {
      auto *var = fp.first->getArgVar(names[i]);
      assert(var);
      var->setSrcInfo(ast->args[i].getSrcInfo());
    }
  }
}

ir::SeriesFlow *CodegenVisitor::newScope(const seq::SrcObject *s, std::string name) {
  return make<SeriesFlow>(s, std::move(name));
}

void CodegenVisitor::visit(ClassStmt *stmt) {}

} // namespace ast
} // namespace seq
