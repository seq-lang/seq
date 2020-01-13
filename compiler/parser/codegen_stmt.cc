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

CodegenStmtVisitor::CodegenStmtVisitor(Context &ctx) : ctx(ctx), result(nullptr) {}

seq::types::Type *CodegenStmtVisitor::VisitType(Expr &expr) {
  CodegenExprVisitor v(ctx, *this);
  expr.accept(v);
  if (v.result->getName() == "type") {
    return v.result->getType();
  } else {
    error(expr.getSrcInfo(), "expected type");
    return nullptr;
  }
}

void CodegenStmtVisitor::apply(Context &ctx, unique_ptr<SuiteStmt> &stmts) {
  auto tv = CodegenStmtVisitor(ctx);
  tv.Visit(*stmts);
}

void CodegenStmtVisitor::Set(seq::Stmt *stmt) { result = stmt; }
seq::Stmt *CodegenStmtVisitor::Visit(Stmt &stmt) {
  CodegenStmtVisitor v(ctx);
  stmt.accept(v);
  if (v.result) {
    v.result->setSrcInfo(stmt.getSrcInfo());
    v.result->setBase(ctx.getBase());
    ctx.getBlock()->add(v.result);
  }
  return move(v.result);
}
seq::Expr *CodegenStmtVisitor::Visit(Expr &expr) {
  CodegenExprVisitor v(ctx, *this);
  expr.accept(v);
  return move(v.result);
}

void CodegenStmtVisitor::visit(SuiteStmt &stmt) {
  for (auto &s: stmt.stmts) {
    Visit(*s);
  }
  Set(nullptr);
}
void CodegenStmtVisitor::visit(PassStmt &stmt) {
  Set(nullptr);
}
void CodegenStmtVisitor::visit(BreakStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ContinueStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ExprStmt &stmt) {
  RETURN(seq::ExprStmt, Visit(*stmt.expr));
}
void CodegenStmtVisitor::visit(AssignStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(DelStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(PrintStmt &stmt) {
  if (stmt.items.size() != 1) {
    error(stmt.getSrcInfo(), "expected single expression");
  }
  RETURN(seq::Print, Visit(*stmt.items[0]));
}
void CodegenStmtVisitor::visit(ReturnStmt &stmt) {
  if (!stmt.expr) {
    RETURN(seq::Return, nullptr);
  } else {
    auto ret = new seq::Return(Visit(*stmt.expr));
    if (auto f = dynamic_cast<seq::Func*>(ctx.getBase())) {
      f->sawReturn(ret);
    } else {
      error(stmt.getSrcInfo(), "return outside function");
    }
    Set(ret);
  }
}
void CodegenStmtVisitor::visit(YieldStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(AssertStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(TypeAliasStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(WhileStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ForStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(IfStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(MatchStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ExtendStmt &stmt) {
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
  auto typ = VisitType(*stmt.what);
  ctx.setEnclosingType(typ);
  Visit(*stmt.suite);
  ctx.setEnclosingType(nullptr);
}
void CodegenStmtVisitor::visit(ImportStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ExternImportStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(TryStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(GlobalStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(ThrowStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(PrefetchStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}
void CodegenStmtVisitor::visit(FunctionStmt &stmt) {
  auto f = new seq::Func();
  f->setName(stmt.name);
  if (auto c = ctx.getEnclosingType()) {
    c->addMethod(stmt.name, f, false);
  } else {
    if (!ctx.isToplevel()) {
      f->setEnclosingFunc(dynamic_cast<seq::Func*>(ctx.getBase()));
    }
    vector<string> names;
    for (auto &n: stmt.args) {
      names.push_back(n.name);
    }
    ctx.add(stmt.name, f, names);
  }
  ctx.addBlock(f->getBlock(), f);

  unordered_set<string> seen;
  unordered_set<string> generics(stmt.generics.begin(), stmt.generics.end());
  bool hasDefault = false;
  for (auto &arg: stmt.args) {
    if (!arg.type) {
      string typName = format("'{}", arg.name);
      arg.type = make_unique<IdExpr>(typName);
      arg.type->setSrcInfo(stmt.getSrcInfo());
      generics.insert(typName);
    }
    if (seen.find(arg.name) != seen.end()) {
      error(stmt.getSrcInfo(), "argument '{}' already specified", arg.name);
    }
    seen.insert(arg.name);
    if (arg.deflt) {
      hasDefault = true;
    } else if (hasDefault) {
      error(stmt.getSrcInfo(), "argument '{}' has no default value", arg.name);
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
  for (auto &arg: stmt.args) {
    types.push_back(VisitType(*arg.type));
    names.push_back(arg.name);
    defaults.push_back(arg.deflt ? Visit(*arg.deflt) : nullptr);
  }
  f->setIns(types);
  f->setArgNames(names);
  f->setDefaults(defaults);

  if (stmt.ret) {
    f->setOut(VisitType(*stmt.ret));
  }
  for (auto a: stmt.attributes) {
    f->addAttribute(a);
    if (a == "atomic") {
      ctx.setFlag("atomic");
    }
  }
  for (auto &arg: stmt.args) {
    ctx.add(arg.name, f->getArgVar(arg.name));
  }
  Visit(*stmt.suite);
  ctx.popBlock();
  RETURN(seq::FuncStmt, f);
}
void CodegenStmtVisitor::visit(ClassStmt &stmt) {
  error(stmt.getSrcInfo(), "TODO");
}