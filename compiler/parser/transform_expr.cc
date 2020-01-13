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

#include "parser/common.h"
#include "parser/context.h"
#include "parser/expr.h"
#include "parser/ocaml.h"
#include "parser/stmt.h"
#include "parser/transform.h"
#include "parser/visitor.h"

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

#define Return(T, ...) Set(make_unique<T>(__VA_ARGS__))

void TransformExprVisitor::Set(ExprPtr &&expr) { result = move(expr); }
ExprPtr TransformExprVisitor::Visit(Expr &expr) {
  TransformExprVisitor v;
  if (newSrcInfo)
    v.newSrcInfo = move(newSrcInfo);
  expr.accept(v);
  if (v.result && v.newSrcInfo) {
    v.result->setSrcInfo(*v.newSrcInfo);
  }
  if (v.newSrcInfo)
    newSrcInfo = move(v.newSrcInfo);
  return move(v.result);
}
ExprPtr TransformExprVisitor::Visit(Expr &expr, const seq::SrcInfo &newInfo) {
  TransformExprVisitor v;
  v.newSrcInfo = make_unique<seq::SrcInfo>(newInfo);
  expr.accept(v);
  if (v.result && v.newSrcInfo) {
    v.result->setSrcInfo(*v.newSrcInfo);
  }
  return move(v.result);
}

void TransformExprVisitor::visit(EmptyExpr &expr) { Return(EmptyExpr, ); }
void TransformExprVisitor::visit(BoolExpr &expr) { Return(BoolExpr, expr); }
void TransformExprVisitor::visit(IntExpr &expr) { Return(IntExpr, expr); }
void TransformExprVisitor::visit(FloatExpr &expr) { Return(FloatExpr, expr); }
void TransformExprVisitor::visit(StringExpr &expr) { Return(StringExpr, expr); }
void TransformExprVisitor::visit(FStringExpr &expr) {
  int braces_count = 0, brace_start = 0;
  vector<ExprPtr> items;
  for (int i = 0; i < expr.value.size(); i++) {
    if (expr.value[i] == '{') {
      if (brace_start < i) {
        ExprPtr p = make_unique<StringExpr>(
            expr.value.substr(brace_start, i - brace_start));
        items.push_back(Visit(*p, expr.getSrcInfo()));
      }
      if (!braces_count) {
        brace_start = i + 1;
      }
      braces_count++;
    } else if (expr.value[i] == '}') {
      braces_count--;
      if (!braces_count) {
        string code = expr.value.substr(brace_start, i - brace_start);
        auto offset = expr.getSrcInfo();
        offset.col += i;
        auto newExpr = parse_expr(code, offset);
        items.push_back(Visit(*newExpr, expr.getSrcInfo()));
      }
      brace_start = i + 1;
    }
  }
  if (braces_count) {
    error(expr.getSrcInfo(), "f-string braces not balanced");
  }
  if (brace_start != expr.value.size()) {
    ExprPtr p = make_unique<StringExpr>(
        expr.value.substr(brace_start, expr.value.size() - brace_start));
    items.push_back(Visit(*p, expr.getSrcInfo()));
  }
  ExprPtr p = make_unique<CallExpr>(
      make_unique<DotExpr>(make_unique<IdExpr>("str"), "cat"), move(items));
  Set(Visit(*p, expr.getSrcInfo()));
}
void TransformExprVisitor::visit(KmerExpr &expr) {
  ExprPtr p = make_unique<CallExpr>(
      make_unique<IndexExpr>(
          make_unique<IdExpr>("Kmer"),
          make_unique<IntExpr>(std::to_string(expr.value.size()), "")),
      make_unique<SeqExpr>(expr.value));
  Set(Visit(*p, expr.getSrcInfo()));
}
void TransformExprVisitor::visit(SeqExpr &expr) {
  if (expr.prefix == "p") {
    ExprPtr p = make_unique<CallExpr>(make_unique<IdExpr>("pseq"),
                                      make_unique<StringExpr>(expr.value));
    Set(Visit(*p, expr.getSrcInfo()));
  } else if (expr.prefix == "s") {
    Return(SeqExpr, expr.value, expr.prefix);
  } else {
    error(expr.getSrcInfo(), "invalid seq prefix '{}'", expr.prefix);
  }
}
void TransformExprVisitor::visit(IdExpr &expr) { Return(IdExpr, expr); }
void TransformExprVisitor::visit(UnpackExpr &expr) {
  error(expr.getSrcInfo(), "unexpected unpacking operator");
}
void TransformExprVisitor::visit(TupleExpr &expr) {
  for (auto &i : expr.items) {
    i = Visit(*i);
  }
  Return(TupleExpr, move(expr.items));
}
void TransformExprVisitor::visit(ListExpr &expr) {
  for (auto &i : expr.items) {
    i = Visit(*i);
  }
  Return(ListExpr, move(expr.items));
}
void TransformExprVisitor::visit(SetExpr &expr) {
  for (auto &i : expr.items) {
    i = Visit(*i);
  }
  Return(SetExpr, move(expr.items));
}
void TransformExprVisitor::visit(DictExpr &expr) {
  for (auto &i : expr.items) {
    i.key = Visit(*i.key);
    i.value = Visit(*i.value);
  }
  Return(DictExpr, move(expr.items));
}
void TransformExprVisitor::visit(GeneratorExpr &expr) {
  for (auto &l : expr.loops) {
    l.gen = Visit(*l.gen);
    for (auto &c: l.conds) {
      c = Visit(*c);
    }
  }
  Return(GeneratorExpr, expr.kind, Visit(*expr.expr), move(expr.loops));
  /* TODO transform:
   T = list[T]()
   for_1: cond_1: for_2: cond_2: expr
   */
}
void TransformExprVisitor::visit(DictGeneratorExpr &expr) {
  for (auto &l : expr.loops) {
    l.gen = Visit(*l.gen);
    for (auto &c: l.conds) {
      c = Visit(*c);
    }
  }
  Return(DictGeneratorExpr, Visit(*expr.key), Visit(*expr.expr),
         move(expr.loops));
}
void TransformExprVisitor::visit(IfExpr &expr) {
  Return(IfExpr, Visit(*expr.cond), Visit(*expr.eif), Visit(*expr.eelse));
}
void TransformExprVisitor::visit(UnaryExpr &expr) {
  Return(UnaryExpr, expr.op, Visit(*expr.expr));
}
void TransformExprVisitor::visit(BinaryExpr &expr) {
  Return(BinaryExpr, Visit(*expr.lexpr), expr.op, Visit(*expr.rexpr));
}
void TransformExprVisitor::visit(PipeExpr &expr) {
  for (auto &i : expr.items) {
    i.expr = Visit(*i.expr);
  }
  Return(PipeExpr, move(expr.items));
}
void TransformExprVisitor::visit(IndexExpr &expr) {
  Return(IndexExpr, Visit(*expr.expr), Visit(*expr.index));
}
void TransformExprVisitor::visit(CallExpr &expr) {
  // TODO: name resolution should come here!
  for (auto &i : expr.args) {
    i.value = Visit(*i.value);
  }
  Return(CallExpr, Visit(*expr.expr), move(expr.args));
}
void TransformExprVisitor::visit(DotExpr &expr) {
  Return(DotExpr, Visit(*expr.expr), expr.member);
}
void TransformExprVisitor::visit(SliceExpr &expr) {
  string prefix;
  if (!expr.st && expr.ed) {
    prefix = "l";
  } else if (expr.st && !expr.ed) {
    prefix = "r";
  } else if (!expr.st && expr.ed) {
    prefix = "e";
  }
  if (expr.step) {
    prefix += "s";
  }
  vector<ExprPtr> args;
  if (expr.st) {
    args.push_back(move(expr.st));
  }
  if (expr.ed) {
    args.push_back(move(expr.ed));
  }
  if (expr.step) {
    args.push_back(move(expr.step));
  }
  if (!args.size()) {
    ExprPtr p = make_unique<IntExpr>("0");
    args.push_back(Visit(*p, expr.getSrcInfo()));
  }
  ExprPtr p =
      make_unique<CallExpr>(make_unique<IdExpr>(prefix + "slice"), move(args));
  Set(Visit(*p, expr.getSrcInfo()));
}
void TransformExprVisitor::visit(EllipsisExpr &expr) { Return(EllipsisExpr, ); }
void TransformExprVisitor::visit(TypeOfExpr &expr) {
  Return(TypeOfExpr, Visit(*expr.expr));
}
void TransformExprVisitor::visit(PtrExpr &expr) {
  Return(PtrExpr, Visit(*expr.expr));
}
void TransformExprVisitor::visit(LambdaExpr &expr) {
  error(expr.getSrcInfo(), "TODO");
}
void TransformExprVisitor::visit(YieldExpr &expr) { Return(YieldExpr, ); }