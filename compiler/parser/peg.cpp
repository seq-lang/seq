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
#include "parser/visitors/format/format.h"
#include "util/peglib.h"

using namespace std;

#define RULE(name, block) \
  rules[name] = [](SV &vs) { \
    block ; \
  }
#define V_ALL  vs
#define V0     vs[0]
#define V1     vs[1]
#define V2     vs[2]
#define CHOICE vs.choice()

namespace seq {
namespace ast {

using namespace peg;
typedef const SemanticValues SV;

class PEGVisitor : public Ope::Visitor {
  vector<string> v;

public:
  static string parse(const shared_ptr<Ope> &op) {
    PEGVisitor v;
    op->accept(v);
    if (v.v.size()) {
      if (v.v[0].empty())
        return fmt::format("g[\"{}\"]", v.v[1]);
      else
        return fmt::format("{}({})", v.v[0], join(v.v, ", ", 1));
    }
    return "-";
  };

private:
  void visit(Sequence &s) override {
    v = { "seq" };
    for (auto &o: s.opes_)
      v.push_back(parse(o));
  }
  void visit(PrioritizedChoice &s) override {
    v = { "cho" };
    for (auto &o: s.opes_)
      v.push_back(parse(o));
  }
  void visit(Repetition &s) override {
    if (s.is_zom())
      v = { "zom", parse(s.ope_) };
    else if (s.min_ == 1 && s.max_ == std::numeric_limits<size_t>::max())
      v = { "oom", parse(s.ope_) };
    else if (s.min_ == 0 && s.max_ == 1)
      v = { "opt", parse(s.ope_) };
    else
      v = { "rep", parse(s.ope_), to_string(s.min_), to_string(s.max_) };
  }
  void visit(AndPredicate &s) override {
    v = { "apd", parse(s.ope_) };
  }
  void visit(NotPredicate &s) override {
    v = { "npd", parse(s.ope_) };
  }
  void visit(LiteralString &s) override {
    v = { s.ignore_case_ ? "liti" : "lit", fmt::format("\"{}\"", escape(s.lit_)) };
  }
  void visit(CharacterClass &s) override {
    vector<string> sv;
    for (auto &c: s.ranges_)
      sv.push_back(fmt::format("{{0x{:x}, 0x{:x}}}", (int)c.first, (int)c.second));
    v = { s.negated_ ? "ncls" : "cls", "{" + join(sv, ",") + "}" };
  }
  void visit(Character &s) override {
    v = { "chr", fmt::format("'{}'", s.ch_) };
  }
  void visit(AnyCharacter &s) override {
    v = { "dot" };
  }
  void visit(Cut &s) override {
    v = { "cut" };
  }
  void visit(Reference &s) override {
    if (s.is_macro_) {
      vector<string> vs;
      for (auto &o: s.args_)
        vs.push_back(parse(o));
      v = { "ref", "g", fmt::format("\"{}\"", s.name_), "\"\"", "true", join(vs, ", ") };
    }
    else
      v = { "", s.name_ };
  }
  void visit(TokenBoundary &s) override {
    v = { "tok", parse(s.ope_) };
  }
  void visit(Ignore &s) override {
    v = { "ign", parse(s.ope_) };
  }
  void visit(Recovery &s) override {
    v = { "rec", parse(s.ope_) };
  }
  // infix TODO
};


struct ParseContext {
  stack<int> indent;
  int parens;

  ParseContext(int parens = 0) : parens(parens) {}
};

template <typename Tn, typename Tsv, typename... Ts> auto NX(Tsv &s, Ts &&...args) {
  auto t = new Tn(std::forward<Ts>(args)...);
  t->setSrcInfo(seq::SrcInfo(s.path, s.line_info().first, s.line_info().first,
                             s.line_info().second, s.line_info().second));
  return t;
}
#define NE(T, ...) make_any<Expr *>(NX<T##Expr>(vs, ##__VA_ARGS__))
#define NS(T, ...) make_any<Stmt *>(NX<T##Stmt>(vs, ##__VA_ARGS__))
#define N(T, ...) unique_ptr<T>(NX<T>(vs, ##__VA_ARGS__))
#define US(p) unique_ptr<Stmt>(any_cast<Stmt *>(p))
#define UE(p) unique_ptr<Expr>(any_cast<Expr *>(p))

any STRING(const SV &vs) { return make_any<string>(vs.sv()); }
any COPY(SV &vs) { return vs[0]; }

template <typename T = Expr> vector<unique_ptr<T>> vecFromSv(SV &vs) {
  vector<unique_ptr<T>> v;
  for (auto &i : vs)
    v.push_back(unique_ptr<T>(any_cast<T *>(i)));
  return move(v);
}

template <typename T = Expr> unique_ptr<T> getOrNull(SV &vs, size_t idx) {
  return unique_ptr<T>(idx < vs.size() ? any_cast<T *>(vs[idx]) : nullptr);
}

Expr *singleOrTuple(const any &vs) {
  auto v = any_cast<vector<Expr *>>(vs);
  if (v.size() == 1)
    return v[0];
  vector<ExprPtr> it;
  for (auto &i : v)
    it.push_back(unique_ptr<Expr>(i));
  auto r = new TupleExpr(move(it));
  r->setSrcInfo(v[0]->getSrcInfo());
  return r;
}

void setStmtRules(parser &rules) {
  auto SUITE = [](SV &vs) { return NS(Suite, vecFromSv<Stmt>(vs)); };
}

void setExprRules(parser &rules) {


}

void setLexingRules(parser &rules) {
  rules["INDENT"].enablePackratParsing = false;
  rules["DEDENT"].enablePackratParsing = false;
  rules["SAMEDENT"].enablePackratParsing = false;

  // arguments | slices | genexp | parentheses
  auto inc = [](string x) { return [x](const char* c, size_t n, any& dt) {
    any_cast<ParseContext &>(dt).parens++; };
  } ;
  auto dec = [](string x) { return [x](const char* c, size_t n, size_t, any&, any& dt) {
    any_cast<ParseContext &>(dt).parens--; };
  } ;
  for (auto &rule: vector<string>{"arguments", "slices", "genexp", "parentheses", "star_parens"}) {
    rules[rule.c_str()].enter = inc(rule);
    rules[rule.c_str()].leave = dec(rule);
  }
  for (auto &rule: vector<string>{"INDENT", "DEDENT", "SAMEDENT", "suite", "statement", "statements", "compound_stmt", "if_stmt"})
    rules[rule.c_str()].enable_memoize = false;
}

StmtPtr parseCode(const string &file, const string &code, int line_offset = 0,
                  int col_offset = 0) {
  seqassert(false, "not yet implemented");
  return nullptr;
}

ExprPtr parseExpr(const string &code, const seq::SrcInfo &offset) {
  seqassert(false, "not yet implemented");
  return nullptr;
}

StmtPtr parseFile(const string &file) {
  peg::parser parser;
  parser.log = [](size_t line, size_t col, const string &msg) {
    cerr << line << ":" << col << ": " << msg << "\n";
  };

  ifstream ifs("/Users/inumanag/Projekti/seq/peg/seq.gram");
  string grammar((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
  ifs.close();

  peg::Rules additional_rules = {
      {"NLP", peg::usr([](const char *s, size_t n, peg::SemanticValues &vs,
                          any &dt) -> size_t {
         return any_cast<ParseContext &>(dt).parens ? 0 : -1;
       })}};
  auto ok = parser.load_grammar(grammar, additional_rules);
  assert(ok);

  for (auto &name: parser.get_rule_names()) {
    auto op = parser[name.c_str()];
    LOG("g[\"{}\"] <= {};", name, PEGVisitor::parse(op.get_core_operator()));
    if (!op.code.empty())
      LOG("g[\"{}\"] = [](SemanticValue &vs, any &dt) {};", name, op.code);
  };
  LOG("preamble -> {}", parser.preamble_);

  setLexingRules(parser);
  setStmtRules(parser);
  setExprRules(parser);

  string source;
  {
    ifstream ifs(file);
    source = string((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    ifs.close();
  }

  Stmt *stmt = nullptr;
  auto ctx = make_any<ParseContext>(0);
  parser.enable_packrat_parsing();
  auto ret = parser.parse_n(source.data(), source.size(), ctx, stmt, file.c_str());
  assert(ret);
  assert(stmt);

  auto result = StmtPtr(stmt);
  LOG("peg := {}", result ? result->toString(0) : "<nullptr>");
  LOG("fmt := {}", FormatVisitor::apply(result));

  seqassert(false, "not yet implemented");
  return result;
}

#undef N

} // namespace ast
} // namespace seq