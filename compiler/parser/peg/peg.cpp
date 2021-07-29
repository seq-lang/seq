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

// struct SetUpPackrat : public peg::Ope::Visitor {
//   bool packrat;

//   void fix(shared_ptr<peg::Ope> &ope) {
//     if (ope.)
//   }
//   void visit(Sequence &ope) override {
//     for (auto op : ope.opes_)
//       op->accept(*this);
//   }
//   void visit(PrioritizedChoice &ope) override {
//     for (auto op : ope.opes_)
//       op->accept(*this);
//   }
//   void visit(Repetition &ope) override { ope.ope_->accept(*this); }
//   void visit(AndPredicate &ope) override { ope.ope_->accept(*this); }
//   void visit(NotPredicate &ope) override { ope.ope_->accept(*this); }
//   void visit(CaptureScope &ope) override { ope.ope_->accept(*this); }
//   void visit(Capture &ope) override { ope.ope_->accept(*this); }
//   void visit(TokenBoundary &ope) override { ope.ope_->accept(*this); }
//   void visit(Ignore &ope) override { ope.ope_->accept(*this); }
//   void visit(WeakHolder &ope) override { ope.weak_.lock()->accept(*this); }
//   void visit(Holder &ope) override { ope.ope_->accept(*this); }
//   void visit(Reference &ope) override {
//     if ()
//     void visit(Sequence & ope) override {
//       for (auto op : ope.opes_)
//         op->accept(*this);
//     }
//   }
//   void visit(Whitespace &ope) override { ope.ope_->accept(*this); }
//   void visit(PrecedenceClimbing &ope) override { ope.atom_->accept(*this); }
//   void visit(Recovery &ope) override { ope.ope_->accept(*this); }
// };

shared_ptr<peg::Grammar> initParser() {
  auto g = make_shared<peg::Grammar>();
  init_rules(*g);
  init_actions(*g);
  ~(*g)["NLP"] <= peg::usr([](const char *, size_t, peg::SemanticValues &, any &dt) {
    return any_cast<ParseContext &>(dt).parens ? 0 : -1;
  });
  for (auto &x : *g) {
    auto v = peg::LinkReferences(*g, x.second.params);
    x.second.accept(v);
  }

  (*g)["program"].enablePackratParsing = true;
  (*g)["INDENT"].enablePackratParsing = false;
  (*g)["DEDENT"].enablePackratParsing = false;
  (*g)["SAMEDENT"].enablePackratParsing = false;
  // RUN_PACKRAT_VISITOR;
  for (auto &rule : vector<string>{"arguments", "slices", "genexp", "parentheses",
                                   "star_parens", "with_parens_item", "params"}) {
    (*g)[rule].enter = [](const char *, size_t, any &dt) {
      any_cast<ParseContext &>(dt).parens++;
    };
    (*g)[rule.c_str()].leave = [](const char *, size_t, size_t, any &, any &dt) {
      any_cast<ParseContext &>(dt).parens--;
    };
  }
  for (auto &rule :
       vector<string>{"INDENT", "DEDENT", "SAMEDENT", "suite", "statement",
                      "statements", "compound_stmt", "if_stmt", "with_stmt", "for_stmt",
                      "while_stmt", "try_stmt", "except_block", "excepts", "gens"})
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

  auto log = [](size_t line, size_t col, const string &msg) {
    LOG("line {}, col {}, msg {}", line, col, msg);
  };
  Stmt *result = nullptr;
  auto ctx = make_any<ParseContext>(0, line_offset, col_offset);
  auto r = (*g)[rule].parse_and_get_value(code.c_str(), code.size(), ctx, result,
                                          file.c_str(), log);
  if (!r.ret) {
    if (r.error_info.message_pos) {
      auto line = peg::line_info(code.c_str(), r.error_info.message_pos);
      log(line.first + line_offset, line.second + col_offset, r.error_info.message);
    } else {
      auto line = peg::line_info(code.c_str(), r.error_info.error_pos);
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
  // LOG("fmt := {}", FormatVisitor::apply(result));

  LOG("[T] ocaml = {:.1f}", _ocaml_time / 1000.0);
  seqassert(false, "not yet implemented");
  return nullptr;
}

} // namespace ast
} // namespace seq