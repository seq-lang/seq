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

using namespace std;
namespace seq {
namespace ast {

using namespace peg;

// struct ParseContext {
//   stack<int> indent;
//   int parens;

//   ParseContext(int parens = 0) : parens(parens) {}
// };

// void setLexingRules(parser &rules) {
//   rules["INDENT"].enablePackratParsing = false;
//   rules["DEDENT"].enablePackratParsing = false;
//   rules["SAMEDENT"].enablePackratParsing = false;
//   RUN_PACKRAT_VISITOR;

//   for (auto &rule :
//        vector<string>{"arguments", "slices", "genexp", "parentheses", "star_parens"})
//        {
//     rules[rule.c_str()].enter = [](const char *c, size_t n, any &dt) {
//       any_cast<ParseContext &>(dt).parens++;
//     };
//   };
//   rules[rule.c_str()].leave = [](const char *c, size_t n, size_t, any &, any &dt) {
//     any_cast<ParseContext &>(dt).parens--;
//   };
// };
//   }
//   for (auto &rule: vector<string>{"INDENT", "DEDENT", "SAMEDENT", "suite",
//   "statement", "statements", "compound_stmt", "if_stmt"})
//     rules[rule.c_str()].enable_memoize = false;
// }

StmtPtr parseCode(const string &file, const string &code, int line_offset,
                  int col_offset) {
  seqassert(false, "not yet implemented");
  return nullptr;
}

ExprPtr parseExpr(const string &code, const seq::SrcInfo &offset) {
  seqassert(false, "not yet implemented");
  return nullptr;
}

StmtPtr parseFile(const string &file) {
  // peg::parser parser;
  // parser.log = [](size_t line, size_t col, const string &msg) {
  //   cerr << line << ":" << col << ": " << msg << "\n";
  // };

  // ifstream ifs("/Users/inumanag/Projekti/seq/peg/seq.gram");
  // string grammar((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
  // ifs.close();

  // peg::Rules additional_rules = {
  //     {"NLP", peg::usr([](const char *s, size_t n, peg::SemanticValues &vs,
  //                         any &dt) -> size_t {
  //        return any_cast<ParseContext &>(dt).parens ? 0 : -1;
  //      })}};
  // auto ok = parser.load_grammar(grammar, additional_rules);
  // assert(ok);

  // for (auto &name: parser.get_rule_names()) {
  //   auto op = parser[name.c_str()].get_core_operator();
  //   LOG("g[\"{}\"] <= {};", name, PEGVisitor::parse(op));
  //   auto code = op->code;
  //   if (auto ope = dynamic_cast<PrioritizedChoice *>(op.get())) {
  //     for (int i = 0; i < ope->opes_.size(); i++)
  //       if (!ope->opes_[i]->code.empty())
  //         code += fmt::format(" <!> {} -> {}\n", i, ope->opes_[i]->code);
  //   }
  //   if (!code.empty())
  //     LOG("g[\"{}\"] = [](SemanticValue &vs, any &dt) {};", name, code);
  // };
  // LOG("preamble -> {}", parser.preamble_);

  // setLexingRules(parser);
  // setStmtRules(parser);
  // setExprRules(parser);

  // string source;
  // {
  //   ifstream ifs(file);
  //   source = string((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
  //   ifs.close();
  // }

  // Stmt *stmt = nullptr;
  // auto ctx = make_any<ParseContext>(0);
  // parser.enable_packrat_parsing();
  // auto ret = parser.parse_n(source.data(), source.size(), ctx, stmt, file.c_str());
  // assert(ret);
  // assert(stmt);

  // auto result = StmtPtr(stmt);
  // LOG("peg := {}", result ? result->toString(0) : "<nullptr>");
  // LOG("fmt := {}", FormatVisitor::apply(result));

  seqassert(false, "not yet implemented");
  return nullptr;
}

} // namespace ast
} // namespace seq