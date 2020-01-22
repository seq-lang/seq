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
#include "parser/ast/expr.h"
#include "parser/ast/stmt.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "parser/context.h"
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
  this->result = new T(__VA_ARGS__);                                           \
  return
#define ERROR(...) error(expr->getSrcInfo(), __VA_ARGS__)

CodegenExprVisitor::CodegenExprVisitor(Context &ctx,
                                       CodegenStmtVisitor &stmtVisitor,
                                       vector<seq::Var *> *captures)
    : ctx(ctx), stmtVisitor(stmtVisitor), result(nullptr), captures(captures) {}

seq::Expr *CodegenExprVisitor::transform(const ExprPtr &expr,
                                         vector<seq::Var *> *captures) {
  // fmt::print("<e> {} :pos {}\n", expr, expr->getSrcInfo());
  CodegenExprVisitor v(ctx, stmtVisitor, captures);
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
  CodegenExprVisitor v(ctx, this->stmtVisitor);
  expr->accept(v);
  if (auto t = dynamic_cast<seq::TypeExpr *>(v.result)) {
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
      uint64_t i = std::stoull(expr->value, nullptr, 10);
      RETURN(seq::IntExpr, i);
    } else {
      int64_t i = std::stoll(expr->value, nullptr, 10);
      RETURN(seq::IntExpr, i);
    }
  } catch (std::out_of_range &) {
    ERROR("integer {} out of range", expr->value);
  }
}
void CodegenExprVisitor::visit(const FloatExpr *expr) {
  RETURN(seq::FloatExpr, expr->value);
}
void CodegenExprVisitor::visit(const StringExpr *expr) {
  RETURN(seq::StrExpr, expr->value);
}
void CodegenExprVisitor::visit(const FStringExpr *expr) {
  ERROR("unexpected f-string");
}
void CodegenExprVisitor::visit(const KmerExpr *expr) {
  ERROR("unexpected k-mer");
}
void CodegenExprVisitor::visit(const SeqExpr *expr) {
  if (expr->prefix != "s") {
    ERROR("unexpected custom sequence");
  }
  RETURN(seq::SeqExpr, expr->value);
}
void CodegenExprVisitor::visit(const IdExpr *expr) {
  auto i = ctx.find(expr->value);
  if (!i) {
    ERROR("identifier '{}' not found", expr->value);
  }
  this->result = i->getExpr();
  if (auto var = dynamic_cast<VarContextItem *>(i.get())) {
    if (captures) {
      captures->push_back(var->getVar());
    }
    if (var->isGlobal() && var->getBase() == ctx.getBase() &&
        ctx.hasFlag("atomic")) {
      dynamic_cast<seq::VarExpr *>(i->getExpr())->setAtomic();
    }
  }
}
void CodegenExprVisitor::visit(const UnpackExpr *expr) {
  ERROR("unexpected unpacking operator");
}
void CodegenExprVisitor::visit(const TupleExpr *expr) {
  vector<seq::Expr *> items;
  for (auto &&i : expr->items) {
    items.push_back(transform(i));
  }
  RETURN(seq::RecordExpr, items, vector<string>(items.size(), ""));
}
void CodegenExprVisitor::visit(const ListExpr *expr) {
  ERROR("unexpected list expression");
}
void CodegenExprVisitor::visit(const SetExpr *expr) {
  ERROR("unexpected set expression");
}
void CodegenExprVisitor::visit(const DictExpr *expr) {
  ERROR("unexpected dict expression");
}
seq::For *CodegenExprVisitor::parseComprehension(
    const Expr *expr, const vector<GeneratorExpr::Body> &loops,
    vector<seq::Var *> *captures, int &added) {
  seq::For *topFor = nullptr;
  seq::Block *block = nullptr;
  for (auto &l : loops) {
    auto f = new seq::For(transform(l.gen, captures));
    f->setSrcInfo(l.gen->getSrcInfo());
    if (!topFor) {
      topFor = f;
    }
    ctx.addBlock(f->getBlock());
    added++;
    if (l.vars.size() == 1) {
      ctx.add(l.vars[0], f->getVar());
    } else {
      ERROR("TODO not yet supported");
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
      auto b = i->addCond(transform(l.conds[0], captures));
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
void CodegenExprVisitor::visit(const GeneratorExpr *expr) {
  vector<seq::Var *> captures;
  int added = 0;
  auto topFor = parseComprehension(expr, expr->loops, &captures, added);
  auto e = transform(expr->expr, &captures);
  if (expr->kind == GeneratorExpr::ListGenerator) {
    this->result = new seq::ListCompExpr(
        e, topFor, transformType(make_unique<IdExpr>("list")));
  } else if (expr->kind == GeneratorExpr::SetGenerator) {
    this->result = new seq::SetCompExpr(
        e, topFor, transformType(make_unique<IdExpr>("set")));
  } else if (expr->kind == GeneratorExpr::Generator) {
    this->result = new seq::GenExpr(e, topFor, captures);
  }
  while (added--) {
    ctx.popBlock();
  }
}
void CodegenExprVisitor::visit(const DictGeneratorExpr *expr) {
  int added = 0;
  auto topFor = parseComprehension(expr, expr->loops, nullptr, added);
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
  } else {
    RETURN(seq::BOpExpr, seq::bop(expr->op), transform(expr->lexpr),
           transform(expr->rexpr), expr->inPlace);
  }
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
  auto getInt = [&](const ExprPtr &e) {
    if (auto i = dynamic_cast<IntExpr *>(e.get())) {
      return std::stol(i->value);
    } else {
      ERROR("expected integer");
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
             seq::types::ArrayType::get(transformType(expr->index)));
    } else if (lhs->value == "generator") {
      RETURN(seq::TypeExpr,
             seq::types::GenType::get(transformType(expr->index)));
    } else if (lhs->value == "Kmer") {
      RETURN(seq::TypeExpr, seq::types::KMer::get(getInt(expr->index)));
    } else if (lhs->value == "Int") {
      RETURN(seq::TypeExpr,
             seq::types::IntNType::get(getInt(expr->index), true));
    } else if (lhs->value == "UInt") {
      RETURN(seq::TypeExpr,
             seq::types::IntNType::get(getInt(expr->index), false));
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
    ERROR("all arguments must be either types or expressions");
  } else if (types.size()) {
    if (auto e = dynamic_cast<seq::TypeExpr *>(lhs)) {
      if (auto ref = dynamic_cast<seq::types::RefType *>(e->getType())) {
        RETURN(seq::TypeExpr, seq::types::GenericType::get(ref, types));
      } else {
        ERROR("types do not accept type arguments");
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
      ERROR("cannot realize expression (is it generic?)");
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
  vector<seq::Expr *> items;
  bool isPartial = false;
  for (auto &&i : expr->args) {
    items.push_back(transform(i.value));
    isPartial |= !items.back();
  }
  if (auto e = dynamic_cast<seq::TypeExpr *>(lhs)) {
    RETURN(seq::ConstructExpr, e->getType(), items);
  } else if (isPartial) {
    RETURN(seq::PartialCallExpr, lhs, items);
  } else {
    RETURN(seq::CallExpr, lhs, items);
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
      c = c->getCache().importFile(c->getModule(), f->getFile()).get();
    } else {
      isImport = false;
      break;
    }
  }
  if (isImport) {
    if (auto i = c->find(expr->member)) {
      this->result = i->getExpr();
    } else {
      ERROR("cannot locate '{}'", expr->member);
    }
    return;
  }

  // Not an import
  auto lhs = transform(expr->expr);
  if (auto e = dynamic_cast<seq::TypeExpr *>(lhs)) {
    RETURN(seq::GetStaticElemExpr, e->getType(), expr->member);
  } else {
    RETURN(seq::GetElemExpr, lhs, expr->member);
  }
}
void CodegenExprVisitor::visit(const SliceExpr *expr) {
  ERROR("unexpected slice");
}
void CodegenExprVisitor::visit(const EllipsisExpr *expr) {}
void CodegenExprVisitor::visit(const TypeOfExpr *expr) {
  RETURN(seq::TypeOfExpr, transform(expr->expr));
}
void CodegenExprVisitor::visit(const PtrExpr *expr) {
  if (auto e = dynamic_cast<IdExpr *>(expr->expr.get())) {
    if (auto v = dynamic_cast<VarContextItem *>(ctx.find(e->value).get())) {
      RETURN(seq::VarPtrExpr, v->getVar());
    } else {
      ERROR("identifier '{}' not found", e->value);
    }
  } else {
    ERROR("not an identifier");
  }
}
void CodegenExprVisitor::visit(const LambdaExpr *expr) { ERROR("TODO"); }
void CodegenExprVisitor::visit(const YieldExpr *expr) {
  RETURN(seq::YieldExpr, ctx.getBase());
}