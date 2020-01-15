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
using std::make_unique;

#define RETURN(T, ...) (this->result = new T(__VA_ARGS__))
#define ERROR(...) error(stmt->getSrcInfo(), __VA_ARGS__)

CodegenStmtVisitor::CodegenStmtVisitor(Context &ctx) : ctx(ctx), result(nullptr) {}
void CodegenStmtVisitor::apply(Context &ctx, const StmtPtr &stmts) {
  auto tv = CodegenStmtVisitor(ctx);
  tv.transform(stmts);
}

seq::Stmt *CodegenStmtVisitor::transform(const StmtPtr &stmt) {
  fmt::print("<codegen> {} :pos {}\n", *stmt, stmt->getSrcInfo());
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
  CodegenExprVisitor v(ctx, *this);
  expr->accept(v);
  return v.result;
}

seq::types::Type *CodegenStmtVisitor::transformType(const ExprPtr &expr) {
  CodegenExprVisitor v(ctx, *this);
  expr->accept(v);
  if (v.result->getName() == "type") {
    return v.result->getType();
  } else {
    error(expr->getSrcInfo(), "expected type");
    return nullptr;
  }
}

void CodegenStmtVisitor::visit(const SuiteStmt *stmt) {
  for (auto &s: stmt->stmts) {
    transform(s);
  }
}
void CodegenStmtVisitor::visit(const PassStmt *stmt) {
}
void CodegenStmtVisitor::visit(const BreakStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const ContinueStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const ExprStmt *stmt) {
  RETURN(seq::ExprStmt, transform(stmt->expr));
}
void CodegenStmtVisitor::visit(const AssignStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const DelStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const PrintStmt *stmt) {
  if (stmt->items.size() != 1) {
    ERROR("expected single expression");
  }
  RETURN(seq::Print, transform(stmt->items[0]));
}
void CodegenStmtVisitor::visit(const ReturnStmt *stmt) {
  if (!stmt->expr) {
    RETURN(seq::Return, nullptr);
  } else {
    auto ret = new seq::Return(transform(stmt->expr));
    if (auto f = dynamic_cast<seq::Func*>(ctx.getBase())) {
      f->sawReturn(ret);
    } else {
      ERROR("return outside function");
    }
    this->result = ret;
  }
}
void CodegenStmtVisitor::visit(const YieldStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const AssertStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const TypeAliasStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const WhileStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const ForStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const IfStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const MatchStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const ExtendStmt *stmt) {
  /*
  let name, generics = match snd name with
      | Id _ -> name, []
      | Index (name, ((_, Id _) as generic)) -> name, [generic]
      | Index (name, (_, Tuple generics)) -> name, generics
      | _ -> serr ~pos "cannot extend non-type expression"
    in
    let typ = E.parse_type ~ctx name in
    let generic_types = Llvm.Generics.Type.get_names typ in
    if List.(length generics <> length generic_types) then
      serr ~pos "specified %d generics, but expected %d" (List.length generics) (List.length generic_types);
    let new_ctx = { ctx with map = Hashtbl.copy ctx.map } in
    let generics = List.map2_exn generics generic_types ~f:(fun g t ->
      match snd g with
      | Id n ->
        (* Util.dbg ">> extend %s :: add %s" (Ast.Expr.to_string name) n; *)
        Ctx.add ~ctx:new_ctx n (Ctx_namespace.Type t)
      | _ -> serr ~pos:(fst g) "not a valid generic specifier")
    in
    ignore
    @@ List.map stmts ~f:(function
           | pos, Function f -> parse_function new_ctx pos f ~cls:typ
           | pos, _ -> serr ~pos "type extensions can only specify functions");
    Llvm.Stmt.pass ()
  */
  ctx.setEnclosingType(transformType(stmt->what));
  transform(stmt->suite);
  ctx.setEnclosingType(nullptr);
}
void CodegenStmtVisitor::visit(const ImportStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const ExternImportStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const TryStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const GlobalStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const ThrowStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const PrefetchStmt *stmt) {
  ERROR("TODO");
}
void CodegenStmtVisitor::visit(const FunctionStmt *stmt) {
  auto f = new seq::Func();
  f->setName(stmt->name);
  if (auto c = ctx.getEnclosingType()) {
    c->addMethod(stmt->name, f, false);
  } else {
    if (!ctx.isToplevel()) {
      f->setEnclosingFunc(dynamic_cast<seq::Func*>(ctx.getBase()));
    }
    vector<string> names;
    for (auto &n: stmt->args) {
      names.push_back(n.name);
    }
    ctx.add(stmt->name, f, names);
  }
  ctx.addBlock(f->getBlock(), f);

  unordered_set<string> seen;
  unordered_set<string> generics(stmt->generics.begin(), stmt->generics.end());
  bool hasDefault = false;
  for (auto &arg: stmt->args) {
    if (!arg.type) {
      string typName = format("'{}", arg.name);
      generics.insert(typName);
    }
    if (seen.find(arg.name) != seen.end()) {
      ERROR("argument '{}' already specified", arg.name);
    }
    seen.insert(arg.name);
    if (arg.deflt) {
      hasDefault = true;
    } else if (hasDefault) {
      ERROR("argument '{}' has no default value", arg.name);
    }
  }
  f->addGenerics(generics.size());
  int gc = 0;
  for (auto &g: generics) {
    f->getGeneric(gc)->setName(g);
    ctx.add(g, f->getGeneric(gc++));
  }
  vector<seq::types::Type*> types;
  vector<string> names;
  vector<seq::Expr*> defaults;
  for (auto &arg: stmt->args) {
    if (!arg.type) {
      types.push_back(transformType(make_unique<IdExpr>(format("'{}", arg.name))));
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
  for (auto a: stmt->attributes) {
    f->addAttribute(a);
    if (a == "atomic") {
      ctx.setFlag("atomic");
    }
  }
  for (auto &arg: stmt->args) {
    ctx.add(arg.name, f->getArgVar(arg.name));
  }
  transform(stmt->suite);
  ctx.popBlock();
  RETURN(seq::FuncStmt, f);
}
void CodegenStmtVisitor::visit(const ClassStmt *stmt) {
  ERROR("TODO");
}