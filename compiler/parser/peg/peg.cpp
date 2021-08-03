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

shared_ptr<peg::Grammar> initParser() {
  auto g = make_shared<peg::Grammar>();
  init_rules(*g);
  init_actions(*g);
  ~(*g)["NLP"] <= peg::usr([](const char *s, size_t n, peg::SemanticValues &, any &dt) {
    return any_cast<ParseContext &>(dt).parens ? 0 : (n >= 1 && s[0] == '\\' ? 1 : -1);
  });
  for (auto &x : *g) {
    auto v = peg::LinkReferences(*g, x.second.params);
    x.second.accept(v);
  }
  (*g)["program"].enablePackratParsing = true;
  for (auto &rule : vector<string>{"arguments", "slices", "genexp", "parentheses",
                                   "star_parens", "generics", "with_parens_item",
                                   "params", "from_as_parens", "from_params"}) {
    (*g)[rule].enter = [](const char *, size_t, any &dt) {
      any_cast<ParseContext &>(dt).parens++;
    };
    (*g)[rule.c_str()].leave = [](const char *, size_t, size_t, any &, any &dt) {
      any_cast<ParseContext &>(dt).parens--;
    };
  }
  return g;
}

template <typename T>
T parseCode(const string &file, string code, int line_offset, int col_offset,
            const string &rule) {
  using namespace std::chrono;
  auto t = high_resolution_clock::now();

  // Initialize
  static shared_ptr<peg::Grammar> g(nullptr);
  if (!g)
    g = initParser();

  vector<tuple<size_t, size_t, string>> errors;
  auto log = [&](size_t line, size_t col, const string &msg) {
    errors.push_back({line, col, msg});
  };
  T result = nullptr;
  auto ctx = make_any<ParseContext>(0, line_offset, col_offset);
  auto r = (*g)[rule].parse_and_get_value(code.c_str(), code.size(), ctx, result,
                                          file.c_str(), log);
  auto ret = r.ret && r.len == code.size();
  if (!ret)
    r.error_info.output_log(log, code.c_str(), code.size());
  exc::ParserException ex;
  if (!errors.empty()) {
    for (auto &e : errors)
      ex.track(fmt::format("{}", get<2>(e)),
               SrcInfo(file, get<0>(e), get<0>(e), get<1>(e), get<1>(e)));
    throw ex;
    return nullptr;
  }
  _ocaml_time += duration_cast<milliseconds>(high_resolution_clock::now() - t).count();
  return result;
}

StmtPtr parseCode(const string &file, const string &code, int line_offset) {
  return parseCode<StmtPtr>(file, code + "\n", line_offset, 0, "program");
}

ExprPtr parseExpr(const string &code, const seq::SrcInfo &offset) {
  return parseCode<ExprPtr>(offset.file, code, offset.line, offset.col, "fstring");
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
  // LOG("peg/{} :=  {}", file, result ? result->toString(0) : "<nullptr>");
  // throw;
  // LOG("fmt := {}", FormatVisitor::apply(result));
  return result;
}

} // namespace ast
} // namespace seq