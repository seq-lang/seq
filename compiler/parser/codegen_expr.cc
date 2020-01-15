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

CodegenExprVisitor::CodegenExprVisitor(Context &ctx,
                                       CodegenStmtVisitor &stmtVisitor)
    : ctx(ctx), stmtVisitor(stmtVisitor), result(nullptr) {}

void CodegenExprVisitor::Set(seq::Expr *stmt) { result = stmt; }
seq::Expr *CodegenExprVisitor::Visit(Expr &expr) {
  fmt::print("<e> {} :pos {}\n", expr, expr.getSrcInfo());
  CodegenExprVisitor v(ctx, stmtVisitor);
  expr.accept(v);
  if (v.result) {
    v.result->setSrcInfo(expr.getSrcInfo());
    if (auto t = ctx.getTryCatch()) {
      v.result->setTryCatch(t);
    }
  }
  return v.result;
}

void CodegenExprVisitor::visit(EmptyExpr &_) { RETURN(seq::NoneExpr, ); }
void CodegenExprVisitor::visit(BoolExpr &expr) {
  RETURN(seq::BoolExpr, expr.value);
}
void CodegenExprVisitor::visit(IntExpr &expr) {
  try {
    if (expr.suffix == "u") {
      uint64_t i = std::stoull(expr.value, nullptr, 10);
      RETURN(seq::IntExpr, i);
    } else {
      int64_t i = std::stoll(expr.value, nullptr, 10);
      RETURN(seq::IntExpr, i);
    }
  } catch (std::out_of_range &) {
    error(expr.getSrcInfo(), "integer {} out of range", expr.value);
  }
}
void CodegenExprVisitor::visit(FloatExpr &expr) {
  RETURN(seq::FloatExpr, expr.value);
}
void CodegenExprVisitor::visit(StringExpr &expr) {
  RETURN(seq::StrExpr, expr.value);
}
void CodegenExprVisitor::visit(FStringExpr &expr) {
  error(expr.getSrcInfo(), "unexpected f-string");
}
void CodegenExprVisitor::visit(KmerExpr &expr) {
  error(expr.getSrcInfo(), "unexpected k-mer");
}
void CodegenExprVisitor::visit(SeqExpr &expr) {
  if (expr.prefix != "s") {
    error(expr.getSrcInfo(), "unexpected custom sequence");
  }
  RETURN(seq::SeqExpr, expr.value);
}
void CodegenExprVisitor::visit(IdExpr &expr) {
  auto i = ctx.find(expr.value);
  if (!i) {
    error(expr.getSrcInfo(), "identifier '{}' not found", expr.value);
  }
  auto e = i->getExpr();
  if (auto vi = dynamic_cast<VarContextItem *>(i.get())) {
    // if (vi->isAtomic(ctx->getBase())) {
    //   e->setAtomic();
    // }
  }
  Set(e);
}
void CodegenExprVisitor::visit(UnpackExpr &expr) {
  error(expr.getSrcInfo(), "unexpected unpacking operator");
}
void CodegenExprVisitor::visit(TupleExpr &expr) {
  vector<seq::Expr *> items;
  for (auto &&i : expr.items)
    items.push_back(Visit(*i));
  RETURN(seq::RecordExpr, items, vector<string>(items.size(), ""));
}
void CodegenExprVisitor::visit(ListExpr &expr) {
  vector<seq::Expr *> items;
  for (auto &&i : expr.items)
    items.push_back(Visit(*i));
  auto typ = ctx.getType("list");
  RETURN(seq::ListExpr, items, typ);
}
void CodegenExprVisitor::visit(SetExpr &expr) {
  vector<seq::Expr *> items;
  for (auto &&i : expr.items)
    items.push_back(Visit(*i));
  auto typ = ctx.getType("set");
  RETURN(seq::SetExpr, items, typ);
}
void CodegenExprVisitor::visit(DictExpr &expr) {
  vector<seq::Expr *> items;
  for (auto &&i : expr.items) {
    items.push_back(Visit(*i.key));
    items.push_back(Visit(*i.value));
  }
  auto typ = ctx.getType("dict");
  RETURN(seq::DictExpr, items, typ);
}
void CodegenExprVisitor::visit(GeneratorExpr &expr) {
  error(expr.getSrcInfo(), "TODO");
}
void CodegenExprVisitor::visit(DictGeneratorExpr &expr) {
  error(expr.getSrcInfo(), "TODO");
}
void CodegenExprVisitor::visit(IfExpr &expr) {
  RETURN(seq::CondExpr, Visit(*expr.cond), Visit(*expr.eif),
         Visit(*expr.eelse));
}
void CodegenExprVisitor::visit(UnaryExpr &expr) {
  RETURN(seq::UOpExpr, seq::uop(expr.op), Visit(*expr.expr));
}
void CodegenExprVisitor::visit(BinaryExpr &expr) {
  RETURN(seq::BOpExpr, seq::bop(expr.op), Visit(*expr.lexpr),
         Visit(*expr.rexpr), true);
}
void CodegenExprVisitor::visit(PipeExpr &expr) {
  vector<seq::Expr *> items;
  for (auto &&i : expr.items)
    items.push_back(Visit(*i.expr));
  auto p = new seq::PipeExpr(items);
  for (int i = 0; i < expr.items.size(); i++) {
    if (expr.items[i].op == "||>") {
      p->setParallel(i);
    }
  }
  Set(p);
}
void CodegenExprVisitor::visit(IndexExpr &expr) {
  // if (IdExpr *i = dynamic_cast<expr.expr>) {
  //   if (i->value == "array") {
  //     auto type = parseType(expr.what);
  //   } else if (i->value == "ptr") {

  //   } else if (i->)
  // }
  RETURN(seq::ArrayLookupExpr, Visit(*expr.expr), Visit(*expr.index));
}
void CodegenExprVisitor::visit(CallExpr &expr) {
  vector<seq::Expr *> items;
  for (auto &&i : expr.args) {
    items.push_back(Visit(*i.value));
  }
  RETURN(seq::CallExpr, Visit(*expr.expr), items);
}
void CodegenExprVisitor::visit(DotExpr &expr) {
  RETURN(seq::GetElemExpr, Visit(*expr.expr), expr.member);
}
void CodegenExprVisitor::visit(SliceExpr &expr) {
  error(expr.getSrcInfo(), "unexpected slice");
}
void CodegenExprVisitor::visit(EllipsisExpr &expr) {
  error(expr.getSrcInfo(), "unexpected ellipsis");
}
void CodegenExprVisitor::visit(TypeOfExpr &expr) {
  RETURN(seq::TypeOfExpr, Visit(*expr.expr));
}
void CodegenExprVisitor::visit(PtrExpr &expr) {
  error(expr.getSrcInfo(), "TODO");
}
void CodegenExprVisitor::visit(LambdaExpr &expr) {
  error(expr.getSrcInfo(), "TODO");
}
void CodegenExprVisitor::visit(YieldExpr &expr) {
  error(expr.getSrcInfo(), "TODO");
}