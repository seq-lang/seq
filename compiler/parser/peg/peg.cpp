/*
 * peg.cpp --- PEG parser interface.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <any>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/peg/peg.h"
#include "parser/peg/rules.h"
#include "parser/visitors/format/format.h"
#include "util/peglib.h"

extern int _ocaml_time;

using namespace std;
namespace seq {
namespace ast {

using namespace peg;

shared_ptr<peg::Grammar> initParser() {
  auto g = make_shared<peg::Grammar>();
  init_rules(*g);
  init_actions(*g);
  ~(*g)["NLP"] <= usr([](const char *, size_t, peg::SemanticValues &, any &dt) {
    return any_cast<ParseContext &>(dt).parens ? 0 : -1;
  });
  for (auto &x : *g) {
    auto v = LinkReferences(*g, x.second.params);
    x.second.accept(v);
  }

  (*g)["INDENT"].enablePackratParsing = false;
  (*g)["DEDENT"].enablePackratParsing = false;
  (*g)["SAMEDENT"].enablePackratParsing = false;
  // RUN_PACKRAT_VISITOR;
  for (auto &rule :
       vector<string>{"arguments", "slices", "genexp", "parentheses", "star_parens"}) {
    (*g)[rule].enter = [](const char *, size_t, any &dt) {
      any_cast<ParseContext &>(dt).parens++;
    };
    (*g)[rule.c_str()].leave = [](const char *, size_t, size_t, any &, any &dt) {
      any_cast<ParseContext &>(dt).parens--;
    };
  }
  for (auto &rule : vector<string>{"INDENT", "DEDENT", "SAMEDENT", "suite", "statement",
                                   "statements", "compound_stmt", "if_stmt",
                                   "with_stmt", "for_stmt", "while_stmt"})
    (*g)[rule].enable_memoize = false;
  return g;
}

StmtPtr parseCode(const string &file, const string &code, int line_offset,
                  int col_offset, const string &rule) {
  using namespace std::chrono;
  auto t = high_resolution_clock::now();

  // Initialize
  static shared_ptr<peg::Grammar> g(nullptr);
  if (!g)
    g = initParser();

  auto log = [](size_t, size_t, const string &) {};
  Stmt *result = nullptr;
  auto ctx = make_any<ParseContext>(0, line_offset, col_offset);
  auto r = (*g)[rule].parse_and_get_value(code.c_str(), code.size(), ctx, result,
                                          file.c_str(), log);
  if (!r.ret) {
    if (r.error_info.message_pos) {
      auto line = line_info(code.c_str(), r.error_info.message_pos);
      log(line.first + line_offset, line.second + col_offset, r.error_info.message);
    } else {
      auto line = line_info(code.c_str(), r.error_info.error_pos);
      log(line.first + line_offset, line.second + col_offset, "syntax error");
    }
    delete result;
    return nullptr;
  }
  _ocaml_time += duration_cast<milliseconds>(high_resolution_clock::now() - t).count();
  return StmtPtr(result);
}

ExprPtr parseExpr(const string &code, const seq::SrcInfo &offset) {
  seqassert(false, "not yet implemented");
  return nullptr;
}

StmtPtr parseFile(const string &file) {
  string code;
  if (file == "-") {
    for (string line; getline(cin, line);)
      code += line + "\n";
  } else {
    ifstream fin(file);
    if (!fin)
      error(fmt::format("cannot open {}", file).c_str());
    for (string line; getline(fin, line);)
      code += line + "\n";
    fin.close();
  }

  auto result = parseCode(file, code);
  LOG("peg := {}", result ? result->toString(0) : "<nullptr>");
  LOG("fmt := {}", FormatVisitor::apply(result));

  seqassert(false, "not yet implemented");
  return nullptr;
}

} // namespace ast
} // namespace seq