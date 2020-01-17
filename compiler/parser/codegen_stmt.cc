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
using std::make_pair;
using std::make_unique;
using std::move;
using std::ostream;
using std::stack;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

#define RETURN(T, ...) (this->result = new T(__VA_ARGS__))
#define ERROR(...) error(stmt->getSrcInfo(), __VA_ARGS__)

CodegenStmtVisitor::CodegenStmtVisitor(Context &ctx)
    : ctx(ctx), result(nullptr) {}
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
  // TODO: JIT
  if (auto i = dynamic_cast<IdExpr*>(stmt->lhs.get())) {
    auto var = i->value;
    if (auto v = dynamic_cast<VarContextItem*>(ctx.find(var).get())) { // assignment
      RETURN(seq::Assign, v->getVar(), transform(stmt->rhs));
    } else { // TODO: types &
      auto varStmt = new seq::VarStmt(transform(stmt->rhs), transformType(stmt->type));
      ctx.add(var, varStmt->getVar());
      this->result = varStmt;
    }
  } if (auto i = dynamic_cast<DotExpr*>(stmt->lhs.get())) {
    RETURN(seq::AssignMember, transform(i->expr), i->member, transform(stmt->rhs));
  } else {
    ERROR("invalid assignment");
  }
}
void CodegenStmtVisitor::visit(const DelStmt *stmt) {
  assert(stmt->var.size() && !stmt->expr);
  auto item = ctx.find(stmt->var);
  if (auto v = dynamic_cast<VarContextItem *>(item.get())) {
    ctx.remove(stmt->var);
    RETURN(seq::Del, v->getVar());
  } else {
    ERROR("cannot delete non-variable");
  }
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
  } else if (auto f = dynamic_cast<seq::Func *>(ctx.getBase())) {
    auto ret = new seq::Return(transform(stmt->expr));
    f->sawReturn(ret);
    this->result = ret;
  } else {
    ERROR("return outside function");
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
    ERROR("yield outside function");
  }
}
void CodegenStmtVisitor::visit(const AssertStmt *stmt) {
  RETURN(seq::Assert, transform(stmt->expr));
}
void CodegenStmtVisitor::visit(const TypeAliasStmt *stmt) { ERROR("TODO"); }
void CodegenStmtVisitor::visit(const WhileStmt *stmt) { ERROR("TODO"); }
void CodegenStmtVisitor::visit(const ForStmt *stmt) { ERROR("TODO"); }
void CodegenStmtVisitor::visit(const IfStmt *stmt) { ERROR("TODO"); }
void CodegenStmtVisitor::visit(const MatchStmt *stmt) { ERROR("TODO"); }
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
      serr ~pos "specified %d generics, but expected %d" (List.length generics)
  (List.length generic_types); let new_ctx = { ctx with map = Hashtbl.copy
  ctx.map } in let generics = List.map2_exn generics generic_types ~f:(fun g t
  -> match snd g with | Id n ->
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
void CodegenStmtVisitor::visit(const ImportStmt *stmt) { ERROR("TODO"); }
void CodegenStmtVisitor::visit(const ExternImportStmt *stmt) { ERROR("TODO"); }
void CodegenStmtVisitor::visit(const TryStmt *stmt) { ERROR("TODO"); }
void CodegenStmtVisitor::visit(const GlobalStmt *stmt) { ERROR("TODO"); }
void CodegenStmtVisitor::visit(const ThrowStmt *stmt) {
  RETURN(seq::Throw, transform(stmt->expr));
}
void CodegenStmtVisitor::visit(const PrefetchStmt *stmt) { ERROR("TODO"); }
void CodegenStmtVisitor::visit(const FunctionStmt *stmt) {
  auto f = new seq::Func();
  f->setName(stmt->name);
  if (auto c = ctx.getEnclosingType()) {
    c->addMethod(stmt->name, f, false);
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
  unordered_set<string> generics(stmt->generics.begin(), stmt->generics.end());
  bool hasDefault = false;
  for (auto &arg : stmt->args) {
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
  for (auto &g : generics) {
    f->getGeneric(gc)->setName(g);
    ctx.add(g, f->getGeneric(gc++));
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
  transform(stmt->suite);
  ctx.popBlock();
  RETURN(seq::FuncStmt, f);
}
void CodegenStmtVisitor::visit(const ClassStmt *stmt) {
  auto getMembers = [&]() {
    vector<seq::types::Type *> types;
    vector<string> names;
    if (stmt->isType && !stmt->args.size()) {
      ERROR("types need at least one member");
    } else
      for (auto &arg : stmt->args) {
        if (!arg.type) {
          ERROR("type information needed for '{}'", arg.name);
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
      ERROR("types cannot be generic");
    }
    auto tn = getMembers();
    t->setContents(tn.first, tn.second);
  } else {
    auto t = seq::types::RefType::get(stmt->name);
    ctx.add(stmt->name, t);
    ctx.setEnclosingType(t);
    ctx.addBlock();
    unordered_set<string> generics(stmt->generics.begin(),
                                   stmt->generics.end());
    t->addGenerics(generics.size());
    int gc = 0;
    for (auto &g : generics) {
      t->getGeneric(gc)->setName(g);
      ctx.add(g, t->getGeneric(gc++));
    }
    auto tn = getMembers();
    t->setContents(seq::types::RecordType::get(tn.first, tn.second, ""));
  }
  transform(stmt->suite);
  ctx.popBlock();
  ctx.setEnclosingType(nullptr);
}