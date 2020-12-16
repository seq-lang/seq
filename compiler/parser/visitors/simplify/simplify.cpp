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
                               const string &file, bool barebones) {
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
    if (barebones)
      stdlibPath = stdlibPath.substr(0, stdlibPath.size() - 5) + "test__.seq";
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

} // namespace ast
} // namespace seq
