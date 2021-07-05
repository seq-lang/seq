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

  rules["program"] = COPY;
  rules["statements"] = SUITE;
  rules["statement"] = COPY;
  rules["simple_stmt"] = SUITE;
  RULE("small_stmt",
    switch (CHOICE) {
      case 0:  return NS(Pass);
      case 1:  return NS(Break);
      case 2:  return NS(Continue);
      case 10: return NS(Expr, unique_ptr<Expr>(singleOrTuple(V0)));
      default: return V0;
    }
  );

  rules["global_stmt"] = [](SV &vs) {
    vector<StmtPtr> stmts;
    for (auto &i : vs)
      stmts.push_back(N(GlobalStmt, any_cast<string>(i)));
    return NS(Suite, move(stmts));
  };
  rules["yield_stmt"] = [](SV &vs) {
    if (vs.choice() == 0)
      return NS(YieldFrom, UE(vs[0]));
    if (!vs.size())
      return NS(Yield, nullptr);
    return NS(Yield, unique_ptr<Expr>(singleOrTuple(vs[0])));
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
    return NS(Return, unique_ptr<Expr>(singleOrTuple(vs[0])));
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
  rules["comparison"] = [](SV &vs) {
    Expr *b = any_cast<Expr *>(vs[0]);
    for (int i = 1; i < vs.size(); i++) {
      auto p = any_cast<pair<string, any>>(vs[i]);
      b = new BinaryExpr(unique_ptr<Expr>(b), p.first, UE(p.second));
    }
    return b;
  };
  rules["compare_op_bitwise_or"] = [](SV &vs) {
    if (vs.choice() == 0)
      return make_pair(string("not in"), vs[0]);
    else if (vs.choice() == 1)
      return make_pair(string("is not"), vs[0]);
    return make_pair(vs.token_to_string(), vs[0]);
  };
  auto chain = [](SV &vs) {
    if (vs.choice() == 0) {
      Expr *b = any_cast<Expr *>(vs[0]);
      for (int i = 1; i < vs.size(); i++)
        b = new BinaryExpr(unique_ptr<Expr>(b), vs.token_to_string(i - 1), UE(vs[i]));
      return make_any<Expr *>(b);
    }
    return vs[0];
  };
  rules["bitwise_or"] = chain;
  rules["bitwise_xor"] = chain;
  rules["bitwise_and"] = chain;
  rules["shift_expr"] = chain;
  rules["sum"] = chain;
  rules["term"] = chain;
  rules["factor"] = [](SV &vs) {
    if (vs.choice() == 0)
      return NE(Unary, "+", UE(vs[0]));
    else if (vs.choice() == 1)
      return NE(Unary, "-", UE(vs[0]));
    else if (vs.choice() == 0)
      return NE(Unary, "~", UE(vs[0]));
    return vs[0];
  };
  rules["power"] = [](SV &vs) {
    if (vs.choice() == 0)
      return NE(Binary, UE(vs[0]), "**", UE(vs[1]));
    return vs[0];
  };
  auto primary = [](SV &vs) {
    Expr *e = any_cast<Expr *>(vs[0]);
    for (int i = 1; i < vs.size(); i++) {
      auto p = any_cast<pair<size_t, any>>(vs[i]);
      if (p.first == 0) {
        e = new DotExpr(unique_ptr<Expr>(e), any_cast<string>(p.second));
      } else if (p.first == 1) {
        e = new CallExpr(unique_ptr<Expr>(e), UE(p.second));
      } else if (p.first == 2) {
        vector<CallExpr::Arg> a;
        for (auto &i : any_cast<vector<pair<string, any>>>(p.second))
          a.push_back(CallExpr::Arg{i.first, UE(i.second)});
        e = new CallExpr(unique_ptr<Expr>(e), move(a));
      } else {
        e = new IndexExpr(unique_ptr<Expr>(e), UE(p.second));
      }
    }
    return e;
  };
  rules["primary"] = primary;
  rules["t_primary"] = primary;
  auto primary_tail = [](SV &vs) {
    if (vs.choice() == 2)
      return make_pair(vs.choice(),
                       vs.size() ? vs[0] : make_any<vector<pair<string, any>>>());
    return make_pair(vs.choice(), vs[0]);
  };
  rules["primary_tail"] = primary_tail;
  rules["t_primary_tail"] = primary_tail;
  rules["slices"] = [](SV &vs) {
    if (vs.size() == 1)
      return vs[0];
    vector<ExprPtr> e;
    for (auto &i : vs)
      e.push_back(UE(i));
    return NE(Tuple, move(e));
  };
  rules["slice"] = [](SV &vs) {
    if (vs.choice() == 0)
      return NE(Slice, UE(vs[0]), UE(vs[1]), vs.size() > 2 ? UE(vs[2]) : nullptr);
    return vs[0];
  };
  rules["slice_part"] = [](SV &vs) {
    return vs.size() ? vs[0] : make_any<Expr *>(nullptr);
  };
  rules["atom"] = [](SV &vs) {
    if (vs.choice() == 0)
      return NE(Bool, true);
    if (vs.choice() == 1)
      return NE(Bool, false);
    if (vs.choice() == 2)
      return NE(None);
    if (vs.choice() == 3)
      return NE(Id, any_cast<string>(vs[0]));
    if (vs.choice() == 4)
      return NE(Int, any_cast<string>(vs[0]),
                vs.size() > 1 ? any_cast<string>(vs[1]) : "");
    if (vs.choice() == 5)
      return vs[0];
    return NE(Ellipsis);
  };
  rules["parentheses"] = COPY;
  rules["tuple"] = [](SV &vs) {
    vector<ExprPtr> e;
    for (auto &i : vs)
      e.push_back(UE(i));
    return NE(Tuple, move(e));
  };
  rules["yield"] = [](SV &vs) { return NE(Yield); };
  rules["named"] = COPY;
  auto genBody = [](const any &a) {
    vector<GeneratorBody> v;
    auto vx = any_cast<vector<any>>(a);
    for (auto &i : vx) {
      auto ti = any_cast<tuple<any, any, vector<any>>>(i);
      vector<ExprPtr> conds;
      for (auto &c : get<2>(ti))
        conds.push_back(UE(c));
      v.push_back({UE(get<0>(ti)), UE(get<1>(ti)), move(conds)});
    }
    return v;
  };
  rules["genexp"] = [genBody](SV &vs) {
    return NE(Generator, GeneratorExpr::Generator, UE(vs[0]), genBody(vs[1]));
  };
  rules["listexpr"] = [](SV &vs) {
    vector<ExprPtr> e;
    for (auto &i : vs)
      e.push_back(UE(i));
    return NE(List, move(e));
  };
  rules["listcomp"] = [genBody](SV &vs) {
    return NE(Generator, GeneratorExpr::ListGenerator, UE(vs[0]), genBody(vs[1]));
  };
  rules["set"] = [](SV &vs) {
    vector<ExprPtr> e;
    for (auto &i : vs)
      e.push_back(UE(i));
    return NE(Set, move(e));
  };
  rules["setcomp"] = [genBody](SV &vs) {
    return NE(Generator, GeneratorExpr::SetGenerator, UE(vs[0]), genBody(vs[1]));
  };
  rules["dict"] = [](SV &vs) {
    vector<DictExpr::DictItem> e;
    for (auto &i : vs) {
      auto p = any_cast<pair<any, any>>(i);
      e.push_back({UE(p.first), UE(p.second)});
    }
    return NE(Dict, move(e));
  };
  rules["dictcomp"] = [genBody](SV &vs) {
    auto p = any_cast<pair<any, any>>(vs[0]);
    return NE(DictGenerator, UE(p.first), UE(p.second), genBody(vs[1]));
  };
  rules["double_starred_kvpair"] = [](SV &vs) {
    if (vs.choice() == 0)
      return make_any<pair<any, any>>(make_any<Expr *>(nullptr),
                                      NE(KeywordStar, UE(vs[0])));
    return vs[0];
  };
  rules["kvpair"] = [](SV &vs) { return make_pair(vs[0], vs[1]); };
  rules["for_if_clauses"] = [](SV &vs) { return vector<any>(vs.begin(), vs.end()); };
  rules["for_if_clause"] = [](SV &vs) {
    return make_tuple(vs[0], vs[1], vector<any>(vs.begin() + 2, vs.end()));
  };


  rules["star_targets"] = [](SV &vs) {
    return NE(Id, "--");
    // return vector<any>(vs.begin(), vs.end());
  };
  rules["star_target"] = COPY;
  rules["target_with_star_atom"] = [](SV &vs) {
    if (vs.choice() == 0)
      return NE(Dot, UE(vs[0]), any_cast<string>(vs[1]));
    if (vs.choice() == 1)
      return NE(Index, UE(vs[0]), UE(vs[1]));
    return vs[0];
  };
  rules["star_atom"] = [](SV &vs) {
    if (vs.choice() == 2 || vs.choice() == 3) {
      vector<ExprPtr> e;
      for (auto &i : vs)
        e.push_back(UE(i));
      return NE(Tuple, move(e));
    }
    return vs[0];
  };

  rules["star_named_expression"] = [](SV &vs) {
    if (vs.choice() == 0)
      return NE(Star, UE(vs[0]));
    return vs[0];
  };
  rules["named_expression"] = [](SV &vs) {
    if (vs.choice() == 0)
      return NE(Assign, make_unique<IdExpr>(any_cast<string>(vs[0])), UE(vs[1]));
    return vs[0];
  };
  rules["arguments"] = [](SV &vs) {
    vector<pair<string, any>> vr;
    for (auto &v: vs)
      for (auto &i: any_cast<vector<pair<string, any>>>(v))
        vr.push_back(i);
    return vr;
  };
  rules["args"] = [](SV &vs) {
    auto vx = any_cast<vector<pair<string, any>>>(vs[0]);
    vector<pair<string, any>> v(vx.begin(), vx.end());
    if (vs.size() > 1) {
      vx = any_cast<vector<pair<string, any>>>(vs[1]);
      v.insert(v.end(), vx.begin(), vx.end());
    }
    return v;
  };
  rules["simple_args"] = [](SV &vs) {
    vector<pair<string, any>> v;
    for (auto &i : vs)
      v.push_back(make_pair(string(), i));
    return v;
  };
  rules["starred_expression"] = [](SV &vs) { return NE(Star, UE(vs[0])); };
  rules["kwargs"] = [](SV &vs) {
    vector<pair<string, any>> v;
    for (auto &i : vs)
      v.push_back(any_cast<pair<string, any>>(i));
    return v;
  };
  rules["kwarg_or_starred"] = [](SV &vs) {
    if (vs.choice() == 0)
      return make_pair(any_cast<string>(vs[0]), vs[1]);
    return make_pair(string(), vs[0]);
  };
  rules["kwarg_or_double_starred"] = [](SV &vs) {
    if (vs.choice() == 0)
      return make_pair(any_cast<string>(vs[0]), vs[1]);
    return make_pair(string(), NE(KeywordStar, UE(vs[0])));
  };
}

void setLexingRules(parser &rules) {
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
  rules["NAME"] = STRING;
  rules["INT"] = STRING;

  // arguments | slices | genexp | parentheses
  auto inc = [](string x) { return [x](const char* c, size_t n, any& dt) {
    //LOG("INC {} / {}", x, any_cast<ParseContext &>(dt).parens+1);
    any_cast<ParseContext &>(dt).parens++; };
  } ;
  auto dec = [](string x) { return [x](const char* c, size_t n, size_t, any&, any& dt) {
    //LOG("DEC {} / {}", x, any_cast<ParseContext &>(dt).parens-1);
    any_cast<ParseContext &>(dt).parens--; };
  } ;
  for (auto &rule: vector<string>{"arguments", "slices", "genexp", "parentheses"}) {
    rules[rule.c_str()].enter = inc(rule);
    rules[rule.c_str()].leave = dec(rule);
  }
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
  parser.enable_trace([&](const peg::Ope &ope, const char *s, size_t /*n*/,
                          const peg::SemanticValues &sv, const peg::Context &c,
                          const std::any & /*dt*/) {},
                      [&](const peg::Ope &ope, const char *s, size_t /*n*/,
                          const peg::SemanticValues &sv, const peg::Context &c,
                          const std::any & /*dt*/, size_t len) {
                        auto pos = static_cast<size_t>(s - c.s);
                        if (len != static_cast<size_t>(-1)) {
                          pos += len;
                        }
                        string indent;
                        auto level = c.trace_ids.size() - 1;
                        while (level--) {
                          indent += " ";
                        }
                        auto name = peg::TraceOpeName::get(const_cast<peg::Ope &>(ope));
                        if (len != static_cast<size_t>(-1) && name[0] == '[') {
                          std::stringstream choice;
                          if (sv.choice_count() > 0)
                            choice << " " << sv.choice() << "/" << sv.choice_count();
                          std::string matched;
                          if (peg::success(len))
                            matched =
                                ", match '" + peg::escape_characters(s, len) + "'";
                          // std::cout << "L " << sv.line_info().first << "\t" << indent
                          //           << name << " #" << choice.str() << matched
                          //           << std::endl;
                        }
                      });
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