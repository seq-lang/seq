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

#include "parser/codegen.h"
#include "parser/common.h"
#include "parser/context.h"
#include "parser/expr.h"
#include "parser/stmt.h"
#include "parser/visitor.h"
#include "seq/seq.h"

using fmt::format;
using std::get;
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
                                       CodegenStmtVisitor &stmtVisitor)
    : ctx(ctx), stmtVisitor(stmtVisitor), result(nullptr) {}

seq::Expr *CodegenExprVisitor::transform(const ExprPtr &expr) {
  // fmt::print("<e> {} :pos {}\n", expr, expr->getSrcInfo());
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
  CodegenExprVisitor v(ctx, this->stmtVisitor);
  expr->accept(v);
  if (auto t = dynamic_cast<seq::TypeExpr *>(v.result)) {
    return t->getType();
  } else {
    error(expr->getSrcInfo(), "expected type");
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
void CodegenExprVisitor::visit(const GeneratorExpr *expr) { ERROR("TODO"); }
void CodegenExprVisitor::visit(const DictGeneratorExpr *expr) { ERROR("TODO"); }
void CodegenExprVisitor::visit(const IfExpr *expr) {
  RETURN(seq::CondExpr, transform(expr->cond), transform(expr->eif),
         transform(expr->eelse));
}
void CodegenExprVisitor::visit(const UnaryExpr *expr) {
  RETURN(seq::UOpExpr, seq::uop(expr->op), transform(expr->expr));
}
void CodegenExprVisitor::visit(const BinaryExpr *expr) {
  RETURN(seq::BOpExpr, seq::bop(expr->op), transform(expr->lexpr),
         transform(expr->rexpr), true);
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
  if (auto e = dynamic_cast<seq::TypeExpr *>(lhs)) {
    if (auto ref = dynamic_cast<seq::types::RefType *>(e->getType())) {
      RETURN(seq::TypeExpr,
             seq::types::GenericType::get(ref, getTypeArr(expr->index)));
    } else {
      ERROR("types do not accept type arguments");
    }
  } else if (auto e = dynamic_cast<seq::FuncExpr *>(lhs)) {
    e->setRealizeTypes(getTypeArr(expr->index));
  } else if (auto e = dynamic_cast<seq::GetElemExpr *>(lhs)) {
    e->setRealizeTypes(getTypeArr(expr->index));
  } else if (auto e = dynamic_cast<seq::GetStaticElemExpr *>(lhs)) {
    e->setRealizeTypes(getTypeArr(expr->index));
  } else {
    RETURN(seq::ArrayLookupExpr, lhs, transform(expr->index));
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
  vector<seq::Expr *> items;
  bool isPartial = false;
  for (auto &&i : expr->args) {
    items.push_back(transform(i.value));
    isPartial |= !items.back();
  }
  auto lhs = transform(expr->expr);
  if (auto e = dynamic_cast<seq::TypeExpr *>(lhs)) {
    RETURN(seq::ConstructExpr, e->getType(), items);
  } else if (isPartial) {
    RETURN(seq::PartialCallExpr, lhs, items);
  } else {
    RETURN(seq::CallExpr, lhs, items);
  }
}
void CodegenExprVisitor::visit(const DotExpr *expr) {
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