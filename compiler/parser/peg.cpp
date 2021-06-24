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
#include "util/peglib.h"

using namespace std;

namespace seq {
namespace ast {

using namespace peg;
typedef const SemanticValues SV;

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

ExprPtr singleOrTuple(const any &vs) {
  auto v = any_cast<vector<Expr *>>(vs);
  if (v.size() == 1)
    return unique_ptr<Expr>(v[0]);
  vector<ExprPtr> it;
  for (auto &i : v)
    it.push_back(unique_ptr<Expr>(i));
  auto r = make_unique<TupleExpr>(move(it));
  r->setSrcInfo(v[0]->getSrcInfo());
  return r;
}

void setStmtRules(parser &rules) {
  auto SUITE = [](SV &vs) { return NS(Suite, vecFromSv<Stmt>(vs)); };

  rules["program"] = COPY;
  rules["statements"] = SUITE;
  rules["statement"] = COPY;
  rules["simple_stmt"] = SUITE;
  rules["small_stmt"] = [](SV &vs) {
    if (vs.choice() == 10) { // expressions
      return NS(Expr, singleOrTuple(vs[0]));
    } else if (vs.choice() == 0) {
      return NS(Pass);
    } else if (vs.choice() == 1) {
      return NS(Break);
    } else if (vs.choice() == 2) {
      return NS(Continue);
    } else {
      return vs[0];
    }
  };

  rules["global_stmt"] = [](SV &vs) {
    vector<StmtPtr> stmts;
    for (auto &i : vs)
      stmts.push_back(N(GlobalStmt, UE(i)->getId()->value));
    return NS(Suite, move(stmts));
  };
  rules["yield_stmt"] = [](SV &vs) {
    if (vs.choice() == 0)
      return NS(YieldFrom, UE(vs[0]));
    if (!vs.size())
      return NS(Yield, nullptr);
    return NS(Yield, singleOrTuple(vs[0]));
  };
  rules["assert_stmt"] = [](SV &vs) { return NS(Assert, UE(vs[0]), getOrNull(vs, 1)); };
  rules["del_stmt"] = [](SV &vs) {
    vector<StmtPtr> stmts;
    for (auto &i : vs)
      stmts.push_back(N(DelStmt, UE(i)));
    return NS(Suite, move(stmts));
  };
  rules["return_stmt"] = [](SV &vs) {
    if (!vs.size())
      return NS(Return, nullptr);
    return NS(Return, singleOrTuple(vs[0]));
  };
  rules["raise_stmt"] = [](SV &vs) { return NS(Throw, getOrNull(vs, 0)); };
  rules["print_stmt"] = [](SV &vs) {
    if (vs.choice() == 2)
      return NS(Print, vector<ExprPtr>(), false);
    vector<ExprPtr> v;
    for (int i = 0; i < vs.size(); i++)
      v.push_back(UE(vs[i]));
    return NS(Print, move(v), vs.choice() == 0);
  };

  // rules["if_stmt"] = [](const peg::SemanticValues &vs) {
  //   vector<IfStmt::If> e;
  //   for (size_t i = 0; i < vs.size(); i += 2) {
  //     ExprPtr expr = nullptr;
  //     StmtPtr suite = nullptr;
  //     if (i + 1 == vs.size())
  //       suite = UP(Stmt, vs[i]);
  //     else
  //       expr = UP(Expr, vs[i]), suite = UP(Stmt, vs[i + 1]);
  //     e.push_back(IfStmt::If{move(expr), move(suite)});
  //   }
  //   return NS(If, move(e));
  // };
}

void setExprRules(parser &rules) {
  rules["expressions"] = [](SV &vs) {
    vector<Expr *> v;
    for (auto &i : vs)
      v.push_back(any_cast<Expr *>(i));
    return v;
  };
  rules["expression"] = [](SV &vs) {
    if (vs.choice() == 0)
      return NE(If, UE(vs[1]), UE(vs[0]), UE(vs[2]));
    return vs[0];
  };
  rules["disjunction"] = [](SV &vs) {
    if (vs.choice() == 0) {
      seqassert(vs.size() >= 2, "bad disjunction parse");
      Expr *b = NX<BinaryExpr>(vs, UE(vs[0]), "||", UE(vs[1]));
      for (int i = 2; i < vs.size(); i++)
        b = NX<BinaryExpr>(vs, unique_ptr<Expr>(b), "||", UE(vs[i]));
      return make_any<Expr *>(b);
    }
    return vs[0];
  };
  rules["conjunction"] = [](SV &vs) {
    if (vs.choice() == 0) {
      seqassert(vs.size() >= 2, "bad conjunction parse");
      Expr *b = NX<BinaryExpr>(vs, UE(vs[0]), "&&", UE(vs[1]));
      for (int i = 2; i < vs.size(); i++)
        b = NX<BinaryExpr>(vs, unique_ptr<Expr>(b), "&&", UE(vs[i]));
      return make_any<Expr *>(b);
    }
    return vs[0];
  };
  rules["inversion"] = [](SV &vs) {
    if (vs.choice() == 0)
      return NE(Unary, "!", UE(vs[0]));
    return vs[0];
  };
  rules["comparison"] = COPY;

  rules["id"] = [](SV &vs) { return NE(Id, any_cast<string>(vs[0])); };
  rules["int"] = [](SV &vs) {
    return NE(Int, any_cast<string>(vs[0]),
              vs.size() > 1 ? any_cast<string>(vs[1]) : "");
  };
}

void setLexingRules(parser &rules) {
  rules["LP"] = [](SV &vs, any &dt) { any_cast<ParseContext &>(dt).parens++; };
  rules["RP"] = [](SV &vs, any &dt) { any_cast<ParseContext &>(dt).parens--; };
  rules["INDENT"] = [](SV &vs, any &dt) {
    if ((!any_cast<ParseContext &>(dt).indent.size() && vs.sv().size()) ||
        (vs.sv().size() > any_cast<ParseContext &>(dt).indent.top()))
      any_cast<ParseContext &>(dt).indent.push(vs.sv().size());
    else
      throw peg::parse_error("bad indent");
  };
  rules["DEDENT"] = [](SV &vs, any &dt) {
    if (any_cast<ParseContext &>(dt).indent.size() &&
        vs.sv().size() < any_cast<ParseContext &>(dt).indent.top())
      any_cast<ParseContext &>(dt).indent.pop();
    else
      throw peg::parse_error("bad dedent");
  };
  rules["SAMEDENT"] = [](SV &vs, any &dt) {
    if ((!any_cast<ParseContext &>(dt).indent.size() && vs.sv().size()) ||
        (any_cast<ParseContext &>(dt).indent.size() &&
         vs.sv().size() != any_cast<ParseContext &>(dt).indent.top())) {
      throw peg::parse_error("bad eqdent");
    }
  };

  rules["IDENT"] = STRING;
  rules["INT"] = STRING;
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
  auto ret = parser.parse_n(source.data(), source.size(), ctx, stmt, file.c_str());
  assert(ret);
  assert(stmt);

  auto result = StmtPtr(stmt);
  LOG("peg := {}", result ? result->toString(0) : "<nullptr>");

  seqassert(false, "not yet implemented");
  return result;
}

#undef N

} // namespace ast
} // namespace seq