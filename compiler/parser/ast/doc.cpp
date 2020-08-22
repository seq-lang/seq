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

#include "parser/ast/ast.h"
#include "parser/ast/doc.h"
#include "parser/ast/format.h"
#include "parser/common.h"
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

namespace seq {
namespace ast {

#ifdef HAHA2

void DocStmtVisitor::visit(const SuiteStmt *stmt) {
  for (auto &i : stmt->stmts) {
    i->accept(*this);
  }
}

void DocStmtVisitor::visit(const FunctionStmt *stmt) {
  if (stmt->name.size() && stmt->name[0] == '_') {
    return;
  }
  auto s = stmt->suite->getStatements();
  string docstr;
  if (s.size()) {
    if (auto se = dynamic_cast<ExprStmt *>(s[0])) {
      if (auto e = dynamic_cast<StringExpr *>(se->expr.get())) {
        docstr = e->value;
      }
    }
  }
  string as;
  for (auto &a : stmt->args)
    as += " " + a.toString();
  LOG("(#fun {}{}{}{}{}{})", stmt->name,
      stmt->generics.size() ? format(" :generics {}", fmt::join(stmt->generics, " "))
                            : "",
      stmt->ret ? " :return " + stmt->ret->toString() : "",
      stmt->args.size() ? " :args" + as : "",
      stmt->attributes.size() ? format(" :attrs ({})", fmt::join(stmt->attributes, " "))
                              : "",
      docstr.size() ? format(" :docstr \"{}\"", escape(docstr)) : "");
}

void DocStmtVisitor::visit(const ClassStmt *stmt) {
  if (stmt->name.size() && stmt->name[0] == '_') {
    return;
  }
  auto s = stmt->suite->getStatements();
  string docstr;
  if (s.size()) {
    if (auto se = dynamic_cast<ExprStmt *>(s[0])) {
      if (auto e = dynamic_cast<StringExpr *>(se->expr.get())) {
        docstr = e->value;
      }
    }
  }
  string as;
  for (auto &a : stmt->args)
    as += " " + a.toString();
  LOG("(#{} {}{}{} {})", (stmt->isRecord ? "type" : "class"), stmt->name,
      stmt->generics.size() ? format(" :gen {}", fmt::join(stmt->generics, " ")) : "",
      stmt->args.size() ? " :args" + as : "",
      docstr.size() ? format(" :docstr \"{}\"", escape(docstr)) : "");
}
#endif

} // namespace ast
} // namespace seq
