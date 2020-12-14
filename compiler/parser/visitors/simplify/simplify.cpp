/*
 * simplify.h --- AST simplification transformation.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/ocaml/ocaml.h"
#include "parser/visitors/simplify/simplify.h"
#include "parser/visitors/simplify/simplify_ctx.h"

using fmt::format;
using std::deque;
using std::dynamic_pointer_cast;
using std::function;
using std::get;
using std::move;
using std::ostream;
using std::pair;
using std::stack;
using std::static_pointer_cast;

namespace seq {
namespace ast {

using namespace types;

StmtPtr SimplifyVisitor::apply(shared_ptr<Cache> cache, const StmtPtr &node,
                               const string &file) {
  // A transformed AST node
  auto suite = make_unique<SuiteStmt>();
  suite->stmts.push_back(make_unique<SuiteStmt>());
  // Preamble is a list of nodes that must be evaluated by the subsequent stages prior
  // to anything else
  auto *preamble = (SuiteStmt *)(suite->stmts[0].get());

  // Load standard library if it has not been already loaded
  if (!in(cache->imports, STDLIB_IMPORT)) {
    // Load the internal module
    auto stdlib = make_shared<SimplifyContext>(STDLIB_IMPORT, cache);
    auto stdlibPath = getImportFile(cache->argv0, STDLIB_INTERNAL_MODULE, "", true);
    if (stdlibPath.empty() ||
        stdlibPath.substr(stdlibPath.size() - 12) != "__init__.seq")
      ast::error("cannot load standard library");
    stdlib->setFilename(stdlibPath);
    cache->imports[STDLIB_IMPORT] = {stdlibPath, stdlib};

    // Add simple POD types to the preamble
    // (these types are defined in LLVM and we cannot properly define them in Seq)
    for (auto &name : {"void", "bool", "byte", "int", "float"}) {
      auto canonical = stdlib->generateCanonicalName(name);
      stdlib->add(SimplifyItem::Type, name, canonical, true);
      // Generate an AST for each POD type. All of them are tuples.
      cache->asts[canonical] =
          make_unique<ClassStmt>(canonical, vector<Param>(), vector<Param>(), nullptr,
                                 vector<string>{ATTR_INTERNAL, ATTR_TUPLE});
      preamble->stmts.push_back(clone(cache->asts[canonical]));
    }
    // Add generic POD types to the preamble
    for (auto &name : vector<string>{"Ptr", "Generator", "Optional", "Int", "UInt"}) {
      auto canonical = stdlib->generateCanonicalName(name);
      stdlib->add(SimplifyItem::Type, name, canonical, true);
      vector<Param> generics;
      // Int and UInt have generic N: int; other have generic T
      if (string(name) == "Int" || string(name) == "UInt")
        generics.emplace_back(Param{"N", make_unique<IdExpr>(".int"), nullptr});
      else
        generics.emplace_back(Param{"T", nullptr, nullptr});
      auto c =
          make_unique<ClassStmt>(canonical, move(generics), vector<Param>(), nullptr,
                                 vector<string>{ATTR_INTERNAL, ATTR_TUPLE});
      if (name == "Generator")
        c->attributes[ATTR_TRAIT] = "";
      preamble->stmts.push_back(clone(c));
      cache->asts[canonical] = move(c);
    }

    StmtPtr stmts = nullptr;
    // This code must be placed in a preamble (these are not POD types but are
    // referenced by the various preamble Function.N and Tuple.N stubs)
    auto code = "@internal\n@tuple\nclass pyobj:\n  p: Ptr[byte]\n"
                "@internal\n@tuple\nclass str:\n  len: int\n  ptr: Ptr[byte]\n";
    preamble->stmts.push_back(
        SimplifyVisitor(stdlib).transform(parseCode(stdlibPath, code)));
    // Load the standard library
    stdlib->setFilename(stdlibPath);
    stmts = parseFile(stdlibPath);
    suite->stmts.push_back(SimplifyVisitor(stdlib).transform(stmts));
    // Add __argv__ variable as __argv__: Array[str]
    stmts =
        make_unique<AssignStmt>(make_unique<IdExpr>("__argv__"), nullptr,
                                make_unique<IndexExpr>(make_unique<IdExpr>(".Array"),
                                                       make_unique<IdExpr>(".str")));
    suite->stmts.push_back(SimplifyVisitor(stdlib).transform(stmts));
  }

  auto ctx = static_pointer_cast<SimplifyContext>(cache->imports[STDLIB_IMPORT].ctx);
  // Transform a given node
  ctx->setFilename(file);
  auto stmts = SimplifyVisitor(ctx).transform(node);

  // Move all auto-generated variardic types to the preamble.
  // Ensure that Function.1 is the first (as all others depend on it!)
  preamble->stmts.emplace_back(clone(cache->asts[".Function.1"]));
  for (auto &v : cache->variardics)
    if (v != ".Function.1")
      preamble->stmts.emplace_back(clone(cache->asts["." + v]));
  // Move the transformed node to the end
  suite->stmts.emplace_back(move(stmts));
  return move(suite);
}

SimplifyVisitor::SimplifyVisitor(shared_ptr<SimplifyContext> ctx,
                                 shared_ptr<vector<StmtPtr>> stmts)
    : ctx(move(ctx)) {
  prependStmts = stmts ? move(stmts) : make_shared<vector<StmtPtr>>();
}

StmtPtr SimplifyVisitor::parseCImport(string name, const vector<Param> &args,
                                      const ExprPtr &ret, string altName,
                                      StringExpr *code) {
  auto canonicalName = ctx->generateCanonicalName(name);
  vector<Param> fnArgs;
  vector<TypePtr> argTypes{};
  generateFunctionStub(args.size() + 1);
  for (int ai = 0; ai < args.size(); ai++) {
    if (args[ai].deflt)
      error("default arguments not supported here");
    if (!args[ai].type)
      error("type for '{}' not specified", args[ai].name);
    fnArgs.emplace_back(
        Param{args[ai].name.empty() ? format(".a{}", ai) : args[ai].name,
              transformType(args[ai].type), nullptr});
  }
  ctx->add(SimplifyItem::Func, altName.empty() ? name : altName, canonicalName,
           ctx->isToplevel());
  StmtPtr body = code ? N<ExprStmt>(code->clone()) : nullptr;
  if (code && !ret)
    error("LLVM functions must have a return type");
  auto f = N<FunctionStmt>(
      canonicalName, ret ? transformType(ret) : transformType(N<IdExpr>("void")),
      vector<Param>(), move(fnArgs), move(body), vector<string>{code ? "llvm" : ".c"});
  ctx->cache->asts[canonicalName] = clone(f);
  return f;
}

StmtPtr SimplifyVisitor::parseDylibCImport(const ExprPtr &dylib, string name,
                                           const vector<Param> &args,
                                           const ExprPtr &ret, string altName) {
  vector<StmtPtr> stmts;
  stmts.push_back(
      N<AssignStmt>(N<IdExpr>("fptr"), N<CallExpr>(N<IdExpr>("_dlsym"), clone(dylib),
                                                   N<StringExpr>(name))));
  vector<ExprPtr> fnArgs;
  fnArgs.push_back(ret ? clone(ret) : N<IdExpr>("void"));
  for (auto &a : args)
    fnArgs.push_back(clone(a.type));
  stmts.push_back(N<AssignStmt>(
      N<IdExpr>("f"),
      N<CallExpr>(N<IndexExpr>(N<IdExpr>("Function"), N<TupleExpr>(move(fnArgs))),
                  N<IdExpr>("fptr"))));
  bool isVoid = true;
  if (ret) {
    if (auto f = CAST(ret, IdExpr))
      isVoid = f->value == "void";
    else
      isVoid = false;
  }
  fnArgs.clear();
  for (int i = 0; i < args.size(); i++)
    fnArgs.push_back(N<IdExpr>(args[i].name != "" ? args[i].name : format(".a{}", i)));
  auto call = N<CallExpr>(N<IdExpr>("f"), move(fnArgs));
  if (!isVoid)
    stmts.push_back(N<ReturnStmt>(move(call)));
  else
    stmts.push_back(N<ExprStmt>(move(call)));
  vector<Param> params;
  for (int i = 0; i < args.size(); i++)
    params.emplace_back(Param{args[i].name != "" ? args[i].name : format(".a{}", i),
                              clone(args[i].type)});
  return transform(N<FunctionStmt>(altName.empty() ? name : altName, clone(ret),
                                   vector<Param>(), move(params),
                                   N<SuiteStmt>(move(stmts)), vector<string>()));
}

// from python import X.Y -> import X; from X import Y ... ?
// from python import Y -> get Y? works---good! not: import Y; return import
StmtPtr SimplifyVisitor::parsePythonImport(const ExprPtr &what, string as) {
  vector<StmtPtr> stmts;
  string from = "";

  vector<string> dirs;
  Expr *e = what.get();
  while (auto d = dynamic_cast<DotExpr *>(e)) {
    dirs.push_back(d->member);
    e = d->expr.get();
  }
  if (!e->getId())
    error("invalid import statement");
  dirs.push_back(e->getId()->value);
  string name = dirs[0], lib;
  for (int i = dirs.size() - 1; i > 0; i--)
    lib += dirs[i] + (i > 1 ? "." : "");
  return transform(N<AssignStmt>(
      N<IdExpr>(name), N<CallExpr>(N<DotExpr>(N<IdExpr>("pyobj"), "_py_import"),
                                   N<StringExpr>(name), N<StringExpr>(lib))));
  // imp = pyobj._py_import("foo", "lib")
}

StmtPtr SimplifyVisitor::parseLLVMImport(const Stmt *codeStmt) {
  if (!codeStmt->getExpr() || !codeStmt->getExpr()->expr->getString())
    error("invalid LLVM function");

  auto code = codeStmt->getExpr()->expr->getString()->value;
  vector<StmtPtr> items;
  auto se = N<StringExpr>("");
  string &finalCode = se->value;
  items.push_back(N<ExprStmt>(move(se)));

  auto escape = [](const string &str, int s, int l) {
    string t;
    t.reserve(l);
    for (int i = s; i < s + l; i++)
      if (str[i] == '{')
        t += "{{";
      else if (str[i] == '}')
        t += "}}";
      else
        t += str[i];
    return t;
  };

  int braceCount = 0, braceStart = 0;
  for (int i = 0; i < code.size(); i++) {
    if (i < code.size() - 1 && code[i] == '{' && code[i + 1] == '=') {
      if (braceStart < i)
        finalCode += escape(code, braceStart, i - braceStart) + '{';
      if (!braceCount) {
        braceStart = i + 2;
        braceCount++;
      } else {
        error("invalid LLVM substitution");
      }
    } else if (braceCount && code[i] == '}') {
      braceCount--;
      string exprCode = code.substr(braceStart, i - braceStart);
      auto offset = getSrcInfo();
      offset.col += i;
      auto expr = transformGenericExpr(parseExpr(exprCode, offset));
      if (!expr->isType() && !expr->getStatic())
        error(expr, "expression {} is not a type or static expression",
              expr->toString());
      //        LOG("~~> {} -> {}", exprCode, expr->toString());
      items.push_back(N<ExprStmt>(move(expr)));
      braceStart = i + 1;
      finalCode += '}';
    }
  }
  if (braceCount)
    error("invalid LLVM substitution");
  if (braceStart != code.size())
    finalCode += escape(code, braceStart, code.size() - braceStart);

  return N<SuiteStmt>(move(items));
}

} // namespace ast
} // namespace seq
