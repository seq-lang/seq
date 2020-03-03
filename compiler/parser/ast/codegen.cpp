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

#define RETURN(T, ...)                                                         \
  this->result = new T(__VA_ARGS__);                                           \
  return
#define ERROR(s, ...) error(s->getSrcInfo(), __VA_ARGS__)

namespace seq {
namespace ast {

CodegenExprVisitor::CodegenExprVisitor(Context &ctx,
                                       CodegenStmtVisitor &stmtVisitor)
    : ctx(ctx), stmtVisitor(stmtVisitor), result(nullptr) {}

seq::Expr *CodegenExprVisitor::transform(const ExprPtr &expr) {
  CodegenExprVisitor v(ctx, stmtVisitor);
  expr->accept(v);
  if (v.result) {
    v.result->setSrcInfo(expr->getSrcInfo());
    if (auto t = ctx.getTryCatch()) {
      v.result->setTryCatch(t);
    }
  }
  return v.result;
}

seq::types::Type *CodegenExprVisitor::transformType(const ExprPtr &expr) {
  auto v = CodegenExprVisitor(ctx, this->stmtVisitor).transform(expr);
  if (auto t = dynamic_cast<seq::TypeExpr *>(v)) {
    return t->getType();
  } else {
    error(expr->getSrcInfo(), "expected type: {}", *expr);
    return nullptr;
  }
}

void CodegenExprVisitor::visit(const EmptyExpr *expr) {
  RETURN(seq::NoneExpr, );
}

void CodegenExprVisitor::visit(const BoolExpr *expr) {
  RETURN(seq::BoolExpr, expr->value);
}

void CodegenExprVisitor::visit(const IntExpr *expr) {
  try {
    if (expr->suffix == "u") {
      uint64_t i = std::stoull(expr->value, nullptr, 0);
      RETURN(seq::IntExpr, i);
    } else {
      int64_t i = std::stoull(expr->value, nullptr, 0);
      RETURN(seq::IntExpr, i);
    }
  } catch (std::out_of_range &) {
    ERROR(expr, "integer {} out of range", expr->value);
  }
}

void CodegenExprVisitor::visit(const FloatExpr *expr) {
  RETURN(seq::FloatExpr, expr->value);
}

void CodegenExprVisitor::visit(const StringExpr *expr) {
  RETURN(seq::StrExpr, expr->value);
}

void CodegenExprVisitor::visit(const FStringExpr *expr) {
  ERROR(expr, "unexpected f-string");
}

void CodegenExprVisitor::visit(const KmerExpr *expr) {
  ERROR(expr, "unexpected k-mer");
}

void CodegenExprVisitor::visit(const SeqExpr *expr) {
  if (expr->prefix != "s") {
    ERROR(expr, "unexpected custom sequence");
  }
  RETURN(seq::SeqExpr, expr->value);
}

void CodegenExprVisitor::visit(const IdExpr *expr) {
  auto i = ctx.find(expr->value);
  if (!i) {
    ERROR(expr, "identifier '{}' not found", expr->value);
  }
  this->result = i->getExpr();
  if (auto var = dynamic_cast<VarContextItem *>(i.get())) {
    if (var->isGlobal() && var->getBase() == ctx.getBase() &&
        ctx.hasFlag("atomic")) {
      dynamic_cast<seq::VarExpr *>(i->getExpr())->setAtomic();
    }
  }
}

void CodegenExprVisitor::visit(const UnpackExpr *expr) {
  ERROR(expr, "unexpected unpacking operator");
}

void CodegenExprVisitor::visit(const TupleExpr *expr) {
  vector<seq::Expr *> items;
  for (auto &&i : expr->items) {
    items.push_back(transform(i));
  }
  RETURN(seq::RecordExpr, items, vector<string>(items.size(), ""));
}

void CodegenExprVisitor::visit(const ListExpr *expr) {
  vector<seq::Expr *> items;
  for (auto &i : expr->items) {
    items.push_back(transform(i));
  }
  RETURN(seq::ListExpr, items, ctx.getType("list"));
}

void CodegenExprVisitor::visit(const SetExpr *expr) {
  vector<seq::Expr *> items;
  for (auto &i : expr->items) {
    items.push_back(transform(i));
  }
  RETURN(seq::SetExpr, items, ctx.getType("set"));
}

void CodegenExprVisitor::visit(const DictExpr *expr) {
  vector<seq::Expr *> items;
  for (auto &i : expr->items) {
    items.push_back(transform(i.key));
    items.push_back(transform(i.value));
  }
  RETURN(seq::DictExpr, items, ctx.getType("dict"));
}

seq::For *CodegenExprVisitor::parseComprehension(
    const Expr *expr, const vector<GeneratorExpr::Body> &loops, int &added) {
  seq::For *topFor = nullptr;
  seq::Block *block = nullptr;
  for (auto &l : loops) {
    auto f = new seq::For(transform(l.gen));
    f->setSrcInfo(l.gen->getSrcInfo());
    if (!topFor) {
      topFor = f;
    }
    ctx.addBlock(f->getBlock());
    added++;
    if (l.vars.size() == 1) {
      ctx.add(l.vars[0], f->getVar());
    } else {
      string varName = getTemporaryVar("for");
      ctx.add(varName, f->getVar());
      for (int i = 0; i < l.vars.size(); i++) {
        auto varStmt = new seq::VarStmt(new seq::ArrayLookupExpr(
            ctx.find(varName)->getExpr(), new seq::IntExpr(i)));
        varStmt->setBase(ctx.getBase());
        ctx.getBlock()->add(varStmt);
        ctx.add(l.vars[i], varStmt->getVar());
      }
    }
    f->setSrcInfo(expr->getSrcInfo());
    f->setBase(ctx.getBase());
    if (block) {
      block->add(f);
    }
    block = ctx.getBlock();
    if (l.conds.size()) {
      auto i = new seq::If();
      i->setSrcInfo(l.conds[0]->getSrcInfo());
      auto b = i->addCond(transform(l.conds[0]));
      i->setSrcInfo(expr->getSrcInfo());
      i->setBase(ctx.getBase());
      block->add(i);
      block = b;
      ctx.addBlock(b);
      added++;
    }
  }
  return topFor;
}

CaptureExprVisitor::CaptureExprVisitor(Context &ctx) : ctx(ctx) {}

void CaptureExprVisitor::visit(const IdExpr *expr) {
  if (auto var = dynamic_cast<VarContextItem *>(ctx.find(expr->value).get())) {
    captures[expr->value] = var->getVar();
  }
}

void CodegenExprVisitor::visit(const GeneratorExpr *expr) {
  int added = 0;

  CaptureExprVisitor captures(ctx);
  expr->accept(captures);

  auto oldTryCatch = ctx.getTryCatch();
  if (expr->kind == GeneratorExpr::Generator) {
    ctx.setTryCatch(nullptr);
  }
  auto topFor = parseComprehension(expr, expr->loops, added);
  auto e = transform(expr->expr);
  if (expr->kind == GeneratorExpr::Generator) {
    ctx.setTryCatch(oldTryCatch);
  }

  if (expr->kind == GeneratorExpr::ListGenerator) {
    this->result = new seq::ListCompExpr(
        e, topFor, transformType(make_unique<IdExpr>("list")));
  } else if (expr->kind == GeneratorExpr::SetGenerator) {
    this->result = new seq::SetCompExpr(
        e, topFor, transformType(make_unique<IdExpr>("set")));
  } else if (expr->kind == GeneratorExpr::Generator) {
    vector<seq::Var *> v;
    // DBG("gen {} getting {} captures", expr->to_string(),
    // captures.captures.size());
    for (auto &kv : captures.captures) {
      // DBG("cap {} ", kv.first);
      v.push_back(kv.second);
    }
    this->result = new seq::GenExpr(e, topFor, v);
  }
  while (added--) {
    ctx.popBlock();
  }
}

void CodegenExprVisitor::visit(const DictGeneratorExpr *expr) {
  int added = 0;
  auto topFor = parseComprehension(expr, expr->loops, added);
  this->result =
      new seq::DictCompExpr(transform(expr->key), transform(expr->expr), topFor,
                            transformType(make_unique<IdExpr>("dict")));
  while (added--) {
    ctx.popBlock();
  }
}

void CodegenExprVisitor::visit(const IfExpr *expr) {
  RETURN(seq::CondExpr, transform(expr->cond), transform(expr->eif),
         transform(expr->eelse));
}

void CodegenExprVisitor::visit(const UnaryExpr *expr) {
  RETURN(seq::UOpExpr, seq::uop(expr->op), transform(expr->expr));
}

void CodegenExprVisitor::visit(const BinaryExpr *expr) {
  if (expr->op == "is") {
    RETURN(seq::IsExpr, transform(expr->lexpr), transform(expr->rexpr));
  } else if (expr->op == "is not") {
    RETURN(seq::UOpExpr, seq::uop("!"),
           new seq::IsExpr(transform(expr->lexpr), transform(expr->rexpr)));
  } else if (expr->op == "in") {
    RETURN(seq::ArrayContainsExpr, transform(expr->lexpr),
           transform(expr->rexpr));
  } else if (expr->op == "not in") {
    RETURN(seq::UOpExpr, seq::uop("!"),
           new seq::ArrayContainsExpr(transform(expr->lexpr),
                                      transform(expr->rexpr)));
  }
  RETURN(seq::BOpExpr, seq::bop(expr->op), transform(expr->lexpr),
         transform(expr->rexpr), expr->inPlace);
}

void CodegenExprVisitor::visit(const PipeExpr *expr) {
  vector<seq::Expr *> items;
  for (auto &&i : expr->items) {
    items.push_back(transform(i.expr));
  }
  auto pexpr = new seq::PipeExpr(items);
  for (int i = 0; i < expr->items.size(); i++) {
    if (expr->items[i].op == "||>") {
      pexpr->setParallel(i);
    }
  }
  this->result = pexpr;
}

void CodegenExprVisitor::visit(const IndexExpr *expr) {
  auto getInt = [&](const ExprPtr &e, int limit) {
    if (auto i = dynamic_cast<IntExpr *>(e.get())) {
      auto r = std::stol(i->value, nullptr, 0);
      if (r <= 0 || r > limit) {
        ERROR(expr, "invalid integer parameter (maximum allowed is {})", limit);
      }
      return r;
    } else {
      ERROR(expr, "expected integer");
      return long(0);
    }
  };
  auto getTypeArr = [&](const ExprPtr &e) {
    vector<seq::types::Type *> v;
    if (auto i = dynamic_cast<TupleExpr *>(e.get())) {
      for (auto &t : i->items) {
        v.push_back(transformType(t));
      }
    } else {
      v.push_back(transformType(e));
    }
    return v;
  };
  if (auto lhs = dynamic_cast<IdExpr *>(expr->expr.get())) {
    if (lhs->value == "array") {
      RETURN(seq::TypeExpr,
             seq::types::ArrayType::get(transformType(expr->index)));
    } else if (lhs->value == "ptr") {
      RETURN(seq::TypeExpr,
             seq::types::PtrType::get(transformType(expr->index)));
    } else if (lhs->value == "generator") {
      RETURN(seq::TypeExpr,
             seq::types::GenType::get(transformType(expr->index)));
    } else if (lhs->value == "Kmer") {
      RETURN(seq::TypeExpr, seq::types::KMer::get(getInt(expr->index, 1024)));
    } else if (lhs->value == "Int") {
      RETURN(seq::TypeExpr,
             seq::types::IntNType::get(getInt(expr->index, 2048), true));
    } else if (lhs->value == "UInt") {
      RETURN(seq::TypeExpr,
             seq::types::IntNType::get(getInt(expr->index, 2048), false));
    } else if (lhs->value == "optional") {
      RETURN(seq::TypeExpr,
             seq::types::OptionalType::get(transformType(expr->index)));
    } else if (lhs->value == "function") {
      auto ty = getTypeArr(expr->index);
      RETURN(seq::TypeExpr,
             seq::types::FuncType::get(
                 vector<seq::types::Type *>(ty.begin() + 1, ty.end()), ty[0]));
    } else if (lhs->value == "tuple") {
      auto ty = getTypeArr(expr->index);
      RETURN(seq::TypeExpr, seq::types::RecordType::get(
                                ty, vector<string>(ty.size(), ""), ""));
    }
  }

  auto lhs = transform(expr->expr);
  vector<seq::Expr *> indices;
  if (auto t = dynamic_cast<TupleExpr *>(expr->index.get())) {
    for (auto &e : t->items) {
      indices.push_back(transform(e));
    }
  } else {
    indices.push_back(transform(expr->index));
  }
  vector<seq::types::Type *> types;
  for (auto &e : indices) {
    if (auto t = dynamic_cast<seq::TypeExpr *>(e)) {
      types.push_back(e->getType());
    }
  }
  if (types.size() && types.size() != indices.size()) {
    ERROR(expr, "all arguments must be either types or expressions");
  } else if (types.size()) {
    if (auto e = dynamic_cast<seq::TypeExpr *>(lhs)) {
      if (auto ref = dynamic_cast<seq::types::RefType *>(e->getType())) {
        RETURN(seq::TypeExpr, seq::types::GenericType::get(ref, types));
      } else {
        ERROR(expr, "types do not accept type arguments");
      }
      /* TODO: all these below are for the functions: should be done better
       * later on ... */
    } else if (auto e = dynamic_cast<seq::FuncExpr *>(lhs)) {
      e->setRealizeTypes(types);
      this->result = lhs;
    } else if (auto e = dynamic_cast<seq::GetElemExpr *>(lhs)) {
      e->setRealizeTypes(types);
      this->result = lhs;
    } else if (auto e = dynamic_cast<seq::GetStaticElemExpr *>(lhs)) {
      e->setRealizeTypes(types);
      this->result = lhs;
    } else {
      ERROR(expr, "cannot realize expression (is it generic?)");
    }
  } else if (indices.size() == 1) {
    RETURN(seq::ArrayLookupExpr, lhs, indices[0]);
  } else {
    RETURN(seq::ArrayLookupExpr, lhs,
           new seq::RecordExpr(indices, vector<string>(indices.size(), "")));
  }
}

void CodegenExprVisitor::visit(const CallExpr *expr) {
  // Special case: __array__ transformation
  if (auto lhs = dynamic_cast<IndexExpr *>(expr->expr.get())) {
    if (auto e = dynamic_cast<IdExpr *>(lhs->expr.get())) {
      if (e->value == "__array__" && expr->args.size() == 1) {
        RETURN(seq::ArrayExpr, transformType(lhs->index),
               transform(expr->args[0].value), true);
      }
    }
  }

  auto lhs = transform(expr->expr);
  bool isTuple = false;
  if (auto fn = dynamic_cast<seq::FuncExpr *>(lhs)) {
    if (auto f = dynamic_cast<seq::Func *>(fn->getFunc())) {
      for (auto &a : f->getAttributes()) {
        if (a == "pyhandle")
          isTuple = true;
      }
    }
  }

  vector<seq::Expr *> items;
  vector<string> names;
  bool isPartial = false;
  if (isTuple) {
    for (auto &&i : expr->args) {
      items.push_back(transform(i.value));
      names.push_back("");
    }
    auto i = new seq::RecordExpr(items, names);
    items = {i};
    names = {""};
  } else {
    bool namesStarted = false;
    for (auto &&i : expr->args) {
      if (i.name == "" && namesStarted) {
        ERROR(expr, "unexpected unnamed argument after a named argument");
      }
      namesStarted |= i.name != "";
      names.push_back(i.name);
      items.push_back(transform(i.value));
      isPartial |= !items.back();
    }
  }

  if (auto e = dynamic_cast<seq::TypeExpr *>(lhs)) {
    RETURN(seq::ConstructExpr, e->getType(), items, names);
  } else if (isPartial) {
    RETURN(seq::PartialCallExpr, lhs, items, names);
  } else {
    RETURN(seq::CallExpr, lhs, items, names);
  }
}

void CodegenExprVisitor::visit(const DotExpr *expr) {
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
      this->result = i->getExpr();
    } else {
      ERROR(expr, "cannot locate '{}'", expr->member);
    }
    return;
  }

  // Not an import
  auto lhs = transform(expr->expr);
  if (auto e = dynamic_cast<seq::TypeExpr *>(lhs)) {
    // DBG(">> sta_elem {}", expr->member);
    RETURN(seq::GetStaticElemExpr, e->getType(), expr->member);
  } else {
    // DBG(">> elem {}", expr->member);
    RETURN(seq::GetElemExpr, lhs, expr->member);
  }
}

void CodegenExprVisitor::visit(const SliceExpr *expr) {
  ERROR(expr, "unexpected slice");
}

void CodegenExprVisitor::visit(const EllipsisExpr *expr) {}

void CodegenExprVisitor::visit(const TypeOfExpr *expr) {
  RETURN(seq::TypeExpr, seq::types::GenericType::get(transform(expr->expr)));
}

void CodegenExprVisitor::visit(const PtrExpr *expr) {
  if (auto e = dynamic_cast<IdExpr *>(expr->expr.get())) {
    if (auto v =
            dynamic_cast<VarContextItem *>(ctx.find(e->value, true).get())) {
      RETURN(seq::VarPtrExpr, v->getVar());
    } else {
      ERROR(expr, "identifier '{}' not found", e->value);
    }
  } else {
    ERROR(expr, "not an identifier");
  }
}

void CodegenExprVisitor::visit(const LambdaExpr *expr) { ERROR(expr, "TODO"); }

void CodegenExprVisitor::visit(const YieldExpr *expr) {
  RETURN(seq::YieldExpr, ctx.getBase());
}

CodegenStmtVisitor::CodegenStmtVisitor(Context &ctx)
    : ctx(ctx), result(nullptr) {}

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
  return CodegenExprVisitor(ctx, *this).transform(expr);
}

seq::Pattern *CodegenStmtVisitor::transform(const PatternPtr &expr) {
  return CodegenPatternVisitor(*this).transform(expr);
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
              if (b->args[0].value->to_string() == expected) {
                expr = transform(b->args[1].value);
              } else if (b->args[1].value->to_string() == expected) {
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
        RETURN(seq::ExprStmt, new seq::AtomicExpr(op, v->getVar(), expr));
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
    RETURN(seq::AssignMember, transform(i->expr), i->member,
           transform(stmt->rhs));
  } else if (auto i = dynamic_cast<IndexExpr *>(stmt->lhs.get())) {
    RETURN(seq::AssignIndex, transform(i->expr), transform(i->index),
           transform(stmt->rhs));
  }
  ERROR(stmt, "invalid assignment");
}

void CodegenStmtVisitor::visit(const AssignEqStmt *stmt) {
  ERROR(stmt, "unexpected assignEq statement");
}

void CodegenStmtVisitor::visit(const DelStmt *stmt) {
  if (auto expr = dynamic_cast<IdExpr *>(stmt->expr.get())) {
    if (auto v =
            dynamic_cast<VarContextItem *>(ctx.find(expr->value, true).get())) {
      ctx.remove(expr->value);
      RETURN(seq::Del, v->getVar());
    }
  } else if (auto i = dynamic_cast<IndexExpr *>(stmt->expr.get())) {
    RETURN(seq::DelIndex, transform(i->expr), transform(i->index));
  }
  ERROR(stmt, "cannot delete non-variable");
}

void CodegenStmtVisitor::visit(const PrintStmt *stmt) {
  RETURN(seq::Print, transform(stmt->expr), ctx.getJIT() != nullptr);
}

void CodegenStmtVisitor::visit(const ReturnStmt *stmt) {
  if (!stmt->expr) {
    RETURN(seq::Return, nullptr);
  } else if (auto f = dynamic_cast<seq::Func *>(ctx.getBase())) {
    auto ret = new seq::Return(transform(stmt->expr));
    f->sawReturn(ret);
    this->result = ret;
  } else {
    ERROR(stmt, "return outside function");
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
    ERROR(stmt, "yield outside function");
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
  auto file =
      ctx.getCache()->getImportFile(stmt->from.first, ctx.getFilename());
  if (file == "") {
    ERROR(stmt, "cannot locate import '{}'", stmt->from.first);
  }
  auto table = ctx.importFile(file);
  if (!stmt->what.size()) {
    ctx.add(stmt->from.second == "" ? stmt->from.first : stmt->from.second,
            file);
  } else if (stmt->what.size() == 1 && stmt->what[0].first == "*") {
    if (stmt->what[0].second != "") {
      ERROR(stmt, "cannot rename star-import");
    }
    for (auto &i : *table) {
      ctx.add(i.first, i.second.top());
    }
  } else
    for (auto &w : stmt->what) {
      if (auto c = table->find(w.first)) {
        ctx.add(w.second == "" ? w.first : w.second, c);
      } else {
        ERROR(stmt, "symbol '{}' not found in {}", w.first, file);
      }
    }
}

void CodegenStmtVisitor::visit(const ExternImportStmt *stmt) {
  vector<string> names;
  vector<seq::types::Type *> types;
  for (auto &arg : stmt->args) {
    if (!arg.type) {
      ERROR(stmt, "C imports need a type for each argument");
    }
    if (arg.name != "" &&
        std::find(names.begin(), names.end(), arg.name) != names.end()) {
      ERROR(stmt, "argument '{}' already specified", arg.name);
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
    ERROR(stmt, "C imports need a return type");
  }
  f->setOut(transformType(stmt->ret));
  if (ctx.getJIT() && ctx.isToplevel() && !ctx.getEnclosingType()) {
    // DBG("adding jit fn {}", stmt->name.first);
    auto fs = new seq::FuncStmt(f);
    fs->setSrcInfo(stmt->getSrcInfo());
    fs->setBase(ctx.getBase());
  } else {
    RETURN(seq::FuncStmt, f);
  }
}

void CodegenStmtVisitor::visit(const TryStmt *stmt) {
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

void CodegenStmtVisitor::visit(const GlobalStmt *stmt) {
  if (ctx.isToplevel()) {
    ERROR(stmt, "can only use global within function blocks");
  }
  if (auto var = dynamic_cast<VarContextItem *>(ctx.find(stmt->var).get())) {
    if (!var->isGlobal()) { // must be toplevel!
      ERROR(stmt, "can only mark toplevel variables as global");
    }
    if (var->getBase() == ctx.getBase()) {
      ERROR(stmt, "can only mark outer variables as global");
    }
    ctx.add(stmt->var, var->getVar(), true);
  } else {
    ERROR(stmt, "identifier '{}' not found", stmt->var);
  }
}

void CodegenStmtVisitor::visit(const ThrowStmt *stmt) {
  RETURN(seq::Throw, transform(stmt->expr));
}

void CodegenStmtVisitor::visit(const FunctionStmt *stmt) {
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
      ERROR(stmt, "argument '{}' already specified", arg.name);
    }
    seen.insert(arg.name);
    if (arg.deflt) {
      hasDefault = true;
    } else if (hasDefault) {
      ERROR(stmt, "argument '{}' has no default value", arg.name);
    }
  }
  f->addGenerics(generics.size());
  seen.clear();
  for (int g = 0; g < generics.size(); g++) {
    if (seen.find(generics[g]) != seen.end()) {
      ERROR(stmt, "repeated generic identifier '{}'", generics[g]);
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
  transform(stmt->suite);
  ctx.setEnclosingType(oldEnclosing);
  ctx.popBlock();
  if (ctx.getJIT() && ctx.isToplevel() && !ctx.getEnclosingType()) {
    auto fs = new seq::FuncStmt(f);
    fs->setSrcInfo(stmt->getSrcInfo());
    fs->setBase(ctx.getBase());
  } else {
    RETURN(seq::FuncStmt, f);
  }
}

void CodegenStmtVisitor::visit(const ClassStmt *stmt) {
  auto getMembers = [&]() {
    vector<seq::types::Type *> types;
    vector<string> names;
    if (stmt->isType && !stmt->args.size()) {
      ERROR(stmt, "types need at least one member");
    } else
      for (auto &arg : stmt->args) {
        if (!arg.type) {
          ERROR(stmt, "type information needed for '{}'", arg.name);
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
      ERROR(stmt, "types cannot be generic");
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
        ERROR(stmt, "repeated generic identifier '{}'", stmt->generics[g]);
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
          ERROR(stmt, "invalid generic variable");
        }
      }
    } else if (auto l = dynamic_cast<IdExpr *>(w->index.get())) {
      generics.push_back(l->value);
    } else {
      ERROR(stmt, "invalid generic variable");
    }
  } else {
    ERROR(stmt, "cannot extend non-type");
  }
  ctx.setEnclosingType(type);
  ctx.addBlock();
  int count = 0;
  if (auto g = dynamic_cast<seq::types::RefType *>(type)) {
    if (g->numGenerics() != generics.size()) {
      ERROR(stmt, "generic count mismatch");
    }
    for (int i = 0; i < g->numGenerics(); i++) {
      ctx.add(generics[i], g->getGeneric(i));
    }
  } else if (count) {
    ERROR(stmt, "unexpected generics");
  }
  transform(stmt->suite);
  ctx.popBlock();
  ctx.setEnclosingType(nullptr);
}

void CodegenStmtVisitor::visit(const YieldFromStmt *stmt) {
  ERROR(stmt, "unexpected yieldFrom statement");
}

void CodegenStmtVisitor::visit(const WithStmt *stmt) {
  ERROR(stmt, "unexpected with statement");
}

void CodegenStmtVisitor::visit(const PyDefStmt *stmt) {
  ERROR(stmt, "unexpected pyDef statement");
}

void CodegenStmtVisitor::visit(const DeclareStmt *stmt) {
  ERROR(stmt, "unexpected declare statement");
}

CodegenPatternVisitor::CodegenPatternVisitor(CodegenStmtVisitor &stmtVisitor)
    : stmtVisitor(stmtVisitor), result(nullptr) {}

seq::Pattern *CodegenPatternVisitor::transform(const PatternPtr &ptr) {
  CodegenPatternVisitor v(stmtVisitor);
  ptr->accept(v);
  if (v.result) {
    v.result->setSrcInfo(ptr->getSrcInfo());
    if (auto t = stmtVisitor.getContext().getTryCatch()) {
      v.result->setTryCatch(t);
    }
  }
  return v.result;
}

void CodegenPatternVisitor::visit(const StarPattern *pat) {
  RETURN(seq::StarPattern, );
}

void CodegenPatternVisitor::visit(const IntPattern *pat) {
  RETURN(seq::IntPattern, pat->value);
}

void CodegenPatternVisitor::visit(const BoolPattern *pat) {
  RETURN(seq::BoolPattern, pat->value);
}

void CodegenPatternVisitor::visit(const StrPattern *pat) {
  RETURN(seq::StrPattern, pat->value);
}

void CodegenPatternVisitor::visit(const SeqPattern *pat) {
  RETURN(seq::SeqPattern, pat->value);
}

void CodegenPatternVisitor::visit(const RangePattern *pat) {
  RETURN(seq::RangePattern, pat->start, pat->end);
}

void CodegenPatternVisitor::visit(const TuplePattern *pat) {
  vector<seq::Pattern *> result;
  for (auto &p : pat->patterns) {
    result.push_back(transform(p));
  }
  RETURN(seq::RecordPattern, result);
}

void CodegenPatternVisitor::visit(const ListPattern *pat) {
  vector<seq::Pattern *> result;
  for (auto &p : pat->patterns) {
    result.push_back(transform(p));
  }
  RETURN(seq::ArrayPattern, result);
}

void CodegenPatternVisitor::visit(const OrPattern *pat) {
  vector<seq::Pattern *> result;
  for (auto &p : pat->patterns) {
    result.push_back(transform(p));
  }
  RETURN(seq::OrPattern, result);
}

void CodegenPatternVisitor::visit(const WildcardPattern *pat) {
  auto p = new seq::Wildcard();
  if (pat->var.size()) {
    stmtVisitor.getContext().add(pat->var, p->getVar());
  }
  this->result = p;
}

void CodegenPatternVisitor::visit(const GuardedPattern *pat) {
  RETURN(seq::GuardedPattern, transform(pat->pattern),
         stmtVisitor.transform(pat->cond));
}

void CodegenPatternVisitor::visit(const BoundPattern *pat) {
  error(pat->getSrcInfo(), "unexpected bound pattern");
}

} // namespace ast
} // namespace seq
