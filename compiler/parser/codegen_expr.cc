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

#define RETURN(T, ...) (this->result = new T(__VA_ARGS__))
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

void CodegenExprVisitor::visit(const EmptyExpr *expr) { RETURN(seq::NoneExpr, ); }
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
  if (auto vi = dynamic_cast<VarContextItem *>(i.get())) {
    // if (vi->isAtomic(ctx->getBase())) {
    //   e->setAtomic();
    // }
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
  vector<seq::Expr *> items;
  for (auto &&i : expr->items)
    items.push_back(transform(i));
  auto typ = ctx.getType("list");
  RETURN(seq::ListExpr, items, typ);
}
void CodegenExprVisitor::visit(const SetExpr *expr) {
  vector<seq::Expr *> items;
  for (auto &&i : expr->items)
    items.push_back(transform(i));
  auto typ = ctx.getType("set");
  RETURN(seq::SetExpr, items, typ);
}
void CodegenExprVisitor::visit(const DictExpr *expr) {
  vector<seq::Expr *> items;
  for (auto &&i : expr->items) {
    items.push_back(transform(i.key));
    items.push_back(transform(i.value));
  }
  auto typ = ctx.getType("dict");
  RETURN(seq::DictExpr, items, typ);
}
void CodegenExprVisitor::visit(const GeneratorExpr *expr) {
  ERROR("TODO");
}
void CodegenExprVisitor::visit(const DictGeneratorExpr *expr) {
  ERROR("TODO");
}
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
  for (auto &&i : expr->items)
    items.push_back(transform(i.expr));
  auto pexpr = new seq::PipeExpr(items);
  for (int i = 0; i < expr->items.size(); i++) {
    if (expr->items[i].op == "||>") {
      pexpr->setParallel(i);
    }
  }
  this->result = pexpr;
}
void CodegenExprVisitor::visit(const IndexExpr *expr) {
  // if (IdExpr *i = dynamic_cast<expr->expr>) {
  //   if (i->value == "array") {
  //     auto type = parseType(expr->what);
  //   } else if (i->value == "ptr") {

  //   } else if (i->)
  // }
  RETURN(seq::ArrayLookupExpr, transform(expr->expr), transform(expr->index));
}
void CodegenExprVisitor::visit(const CallExpr *expr) {
  vector<seq::Expr *> items;
  for (auto &&i : expr->args) {
    items.push_back(transform(i.value));
  }
  RETURN(seq::CallExpr, transform(expr->expr), items);
}
void CodegenExprVisitor::visit(const DotExpr *expr) {
  RETURN(seq::GetElemExpr, transform(expr->expr), expr->member);
}
void CodegenExprVisitor::visit(const SliceExpr *expr) {
  ERROR("unexpected slice");
}
void CodegenExprVisitor::visit(const EllipsisExpr *expr) {
  ERROR("unexpected ellipsis");
}
void CodegenExprVisitor::visit(const TypeOfExpr *expr) {
  RETURN(seq::TypeOfExpr, transform(expr->expr));
}
void CodegenExprVisitor::visit(const PtrExpr *expr) {
  ERROR("TODO");
}
void CodegenExprVisitor::visit(const LambdaExpr *expr) {
  ERROR("TODO");
}
void CodegenExprVisitor::visit(const YieldExpr *expr) {
  ERROR("TODO");
}