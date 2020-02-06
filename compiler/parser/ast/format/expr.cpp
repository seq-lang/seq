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

#include "parser/ast/expr.h"
#include "parser/ast/format/expr.h"
#include "parser/ast/stmt.h"
#include "parser/ast/transform/expr.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "parser/context.h"
#include "parser/ocaml.h"

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
  this->result = fmt::format(T, __VA_ARGS__);                                  \
  return

string FormatExprVisitor::transform(const Expr *expr) {
  FormatExprVisitor v;
  expr->accept(v);
  return v.result;
}

string FormatExprVisitor::transform(const vector<ExprPtr> &exprs) {
  string r;
  for (auto &e : exprs) {
    r += transform(e) + ", ";
  }
  return r;
}

void FormatExprVisitor::visit(const EmptyExpr *expr) { this->result = "None"; }

void FormatExprVisitor::visit(const BoolExpr *expr) {
  RETURN("{}", expr->value ? "True" : "False");
}

void FormatExprVisitor::visit(const IntExpr *expr) {
  RETURN("{}{}", expr->value, expr->suffix);
}

void FormatExprVisitor::visit(const FloatExpr *expr) {
  RETURN("{}{}", expr->value, expr->suffix);
}

void FormatExprVisitor::visit(const StringExpr *expr) {
  RETURN("\"{}\"", escape(expr->value));
}

void FormatExprVisitor::visit(const FStringExpr *expr) {
  RETURN("f\"{}\"", escape(expr->value));
}

void FormatExprVisitor::visit(const KmerExpr *expr) {
  RETURN("k\"{}\"", escape(expr->value));
}

void FormatExprVisitor::visit(const SeqExpr *expr) {
  RETURN("{}\"{}\"", expr->prefix, escape(expr->value));
}

void FormatExprVisitor::visit(const IdExpr *expr) { RETURN("{}", expr->value); }

void FormatExprVisitor::visit(const UnpackExpr *expr) {
  RETURN("*{}", transform(expr->what));
}

void FormatExprVisitor::visit(const TupleExpr *expr) {
  RETURN("({})", transform(expr->items));
}

void FormatExprVisitor::visit(const ListExpr *expr) {
  RETURN("[{}]", transform(expr->items));
}

void FormatExprVisitor::visit(const SetExpr *expr) {
  RETURN("{{{}}}", transform(expr->items));
}

void FormatExprVisitor::visit(const DictExpr *expr) {
  vector<string> items;
  for (auto &i : expr->items) {
    items.push_back(format("{}: {}", transform(i.key), transform(i.value)));
  }
  RETURN("{{{}}}", fmt::join(items, ", "));
}

void FormatExprVisitor::visit(const GeneratorExpr *expr) {
  string s;
  for (auto &i : expr->loops) {
    string cond;
    for (auto &k : i.conds) {
      cond += format(" if {}", transform(k));
    }
    s += format("for {} in {}{}", fmt::join(i.vars, ", "), i.gen->to_string(),
                cond);
  }
  if (expr->kind == GeneratorExpr::ListGenerator) {
    RETURN("[{} {}]", transform(expr->expr), s);
  } else if (expr->kind == GeneratorExpr::SetGenerator) {
    RETURN("{{{} {}}}", transform(expr->expr), s);
  } else {
    RETURN("({} {})", transform(expr->expr), s);
  }
}

void FormatExprVisitor::visit(const DictGeneratorExpr *expr) {
  string s;
  for (auto &i : expr->loops) {
    string cond;
    for (auto &k : i.conds) {
      cond += format(" if {}", transform(k));
    }
    s += format("for {} in {}{}", fmt::join(i.vars, ", "), i.gen->to_string(),
                cond);
  }
  RETURN("{{{}: {} {}}}", transform(expr->key), transform(expr->expr), s);
}

void FormatExprVisitor::visit(const IfExpr *expr) {
  RETURN("{} if {} else {}", transform(expr->eif), transform(expr->cond),
         transform(expr->eelse));
}

void FormatExprVisitor::visit(const UnaryExpr *expr) {
  RETURN("{}{}", expr->op, transform(expr->expr));
}

void FormatExprVisitor::visit(const BinaryExpr *expr) {
  RETURN("({} {} {})", transform(expr->lexpr), expr->op,
         transform(expr->rexpr));
}

void FormatExprVisitor::visit(const PipeExpr *expr) {
  vector<string> items;
  for (auto &l : expr->items) {
    if (!items.size()) {
      items.push_back(transform(l.expr));
    } else {
      items.push_back(l.op + " " + transform(l.expr));
    }
  }
  RETURN("({})", fmt::join(items, " "));
}

void FormatExprVisitor::visit(const IndexExpr *expr) {
  RETURN("{}[{}]", transform(expr->expr), transform(expr->index));
}

void FormatExprVisitor::visit(const CallExpr *expr) {
  vector<string> args;
  for (auto &i : expr->args) {
    if (i.name == "") {
      args.push_back(transform(i.value));
    } else {
      args.push_back(format("{}: {}", i.name, transform(i.value)));
    }
  }
  RETURN("{}({})", transform(expr->expr), fmt::join(args, ", "));
}

void FormatExprVisitor::visit(const DotExpr *expr) {
  RETURN("{}.{}", transform(expr->expr), expr->member);
}

void FormatExprVisitor::visit(const SliceExpr *expr) {
  string s;
  if (expr->st) {
    s += transform(expr->st);
  }
  s += ":";
  if (expr->ed) {
    s += transform(expr->ed);
  }
  s += ":";
  if (expr->step) {
    s += transform(expr->step);
  }
  this->result = s;
}

void FormatExprVisitor::visit(const EllipsisExpr *expr) {
  this->result = "...";
}

void FormatExprVisitor::visit(const TypeOfExpr *expr) {
  RETURN("typeof({})", transform(expr->expr));
}

void FormatExprVisitor::visit(const PtrExpr *expr) {
  RETURN("__ptr__({})", transform(expr->expr));
}

void FormatExprVisitor::visit(const LambdaExpr *expr) {
  RETURN("lambda {}: {}", fmt::join(expr->vars, ", "), transform(expr->expr));
}

void FormatExprVisitor::visit(const YieldExpr *expr) {
  this->result = "(yield)";
}
