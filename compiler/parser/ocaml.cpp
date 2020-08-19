#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"

#include <caml/alloc.h>
#include <caml/callback.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>

#include "lang/seq.h"
#include "parser/ast/ast.h"
#include "parser/common.h"

using namespace std;

#define OcamlReturn(result)                                                            \
  do {                                                                                 \
    auto caml__temp_result = (result);                                                 \
    caml_local_roots = caml__frame;                                                    \
    return (caml__temp_result);                                                        \
  } while (0)

namespace seq {
namespace ast {

string parse_string(value v) { return string(String_val(v), caml_string_length(v)); }

template <typename TF> auto parse_list(value v, TF f) -> vector<decltype(f(v))> {
  auto helper = [](value v, TF f, vector<decltype(f(v))> &t) {
    CAMLparam1(v);
    while (v != Val_emptylist) {
      t.push_back(f(Field(v, 0)));
      v = Field(v, 1);
    }
    CAMLreturn0;
  };
  vector<decltype(f(v))> t;
  helper(v, f, t);
  return t;
}

template <typename TF> auto parse_optional(value v, TF f) -> decltype(f(v)) {
  if (v == Val_int(0))
    return decltype(f(v))();
  else
    return f(Field(v, 0));
}

seq::SrcInfo parse_pos(value val) {
  CAMLparam1(val);
  CAMLlocal2(p0, p1);
  p0 = Field(val, 0);
  string file = parse_string(Field(p0, 0));
  int line1 = Int_val(Field(p0, 1));
  int col1 = Int_val(Field(p0, 3)) - Int_val(Field(p0, 2));
  p1 = Field(val, 1);
  int line2 = Int_val(Field(p1, 1));
  int col2 = Int_val(Field(p1, 3)) - Int_val(Field(p1, 2));
  return seq::SrcInfo(file, line1 + 1, line2 + 1, col1 + 1, col2 + 1);
}

unique_ptr<Expr> parse_expr(value val) {
#define Return(x, ...)                                                                 \
  do {                                                                                 \
    auto _ret = make_unique<x##Expr>(__VA_ARGS__);                                     \
    _ret->setSrcInfo(pos);                                                             \
    OcamlReturn(move(_ret));                                                           \
  } while (0)

  CAMLparam1(val);
  CAMLlocal3(p, v, t);
  CAMLlocal3(f0, f1, f2);

  auto pos = parse_pos(Field(val, 0));
  v = Field(val, 1);
  if (Is_long(v))
    seq::compilationError("[internal] long variant mismatch");
  int tv = Tag_val(v);
  t = Field(v, 0);
  switch (tv) {
  case 0:
    Return(None, );
  case 1:
    Return(Bool, Bool_val(t));
  case 2:
    Return(Int, parse_string(Field(t, 0)), parse_string(Field(t, 1)));
  case 3:
    Return(Float, Double_val(Field(t, 0)), parse_string(Field(t, 1)));
  case 4:
    Return(String, parse_string(t));
  case 5:
    Return(FString, parse_string(t));
  case 6:
    Return(Kmer, parse_string(t));
  case 7:
    Return(Seq, parse_string(Field(t, 1)), parse_string(Field(t, 0)));
  case 8:
    Return(Id, parse_string(t));
  case 9:
    Return(Unpack, parse_expr(t));
  case 10:
    Return(Tuple, parse_list(t, parse_expr));
  case 11:
    Return(List, parse_list(t, parse_expr));
  case 12:
    Return(Set, parse_list(t, parse_expr));
  case 13:
    Return(Dict, parse_list(t, [](value in) {
             CAMLparam1(in);
             OcamlReturn((DictExpr::KeyValue{parse_expr(Field(in, 0)),
                                             parse_expr(Field(in, 1))}));
           }));
  case 14:
  case 15:
  case 16:
  case 17: {
    f0 = Field(t, 0);
    f1 = Field(t, 1);
    vector<GeneratorExpr::Body> loops;
    while (true) {
      f1 = Field(f1, 1); // TODO: ignore position here for now
      loops.push_back({parse_expr(Field(f1, 0)), parse_expr(Field(f1, 1)),
                       parse_list(Field(f1, 2), parse_expr)});
      if (Field(f1, 3) == Val_int(0))
        break;
      f1 = Field(Field(f1, 3), 0);
    }
    if (tv < 17)
      Return(Generator, static_cast<GeneratorExpr::Kind>(tv - 14), parse_expr(f0),
             move(loops));
    else
      Return(DictGenerator, parse_expr(Field(f0, 0)), parse_expr(Field(f0, 1)),
             move(loops));
  }
  case 18:
    Return(If, parse_expr(Field(t, 0)), parse_expr(Field(t, 1)),
           parse_expr(Field(t, 2)));
  case 19:
    Return(Unary, parse_string(Field(t, 0)), parse_expr(Field(t, 1)));
  case 20:
    Return(Binary, parse_expr(Field(t, 0)), parse_string(Field(t, 1)),
           parse_expr(Field(t, 2)));
  case 21:
    Return(Pipe, parse_list(t, [](value in) {
             CAMLparam1(in);
             OcamlReturn((
                 PipeExpr::Pipe{parse_string(Field(in, 0)), parse_expr(Field(in, 1))}));
           }));
  case 22:
    Return(Index, parse_expr(Field(t, 0)), parse_expr(Field(t, 1)));
  case 23:
    Return(Call, parse_expr(Field(t, 0)), parse_list(Field(t, 1), [](value i) {
             CAMLparam1(i);
             OcamlReturn((CallExpr::Arg{parse_optional(Field(i, 0), parse_string),
                                        parse_expr(Field(i, 1))}));
           }));
  case 24:
    Return(Slice, parse_optional(Field(t, 0), parse_expr),
           parse_optional(Field(t, 1), parse_expr),
           parse_optional(Field(t, 2), parse_expr));
  case 25:
    Return(Dot, parse_expr(Field(t, 0)), parse_string(Field(t, 1)));
  case 26:
    Return(Ellipsis, );
  case 27:
    Return(TypeOf, parse_expr(t));
  case 28:
    Return(Ptr, parse_expr(t));
  case 29:
    Return(Lambda, parse_list(Field(t, 0), parse_string), parse_expr(Field(t, 1)));
  case 30:
    Return(Yield, );
  default:
    seq::compilationError("[internal] tag variant mismatch ...");
    return nullptr;
  }
#undef Return
}

unique_ptr<Pattern> parse_pattern(value val) {
#define Return(x, ...)                                                                 \
  do {                                                                                 \
    auto _ret = make_unique<x##Pattern>(__VA_ARGS__);                                  \
    _ret->setSrcInfo(pos);                                                             \
    OcamlReturn(move(_ret));                                                           \
  } while (0)

  CAMLparam1(val);
  CAMLlocal3(p, v, t);
  CAMLlocal3(f0, f1, f2);

  auto pos = parse_pos(Field(val, 0));
  v = Field(val, 1);
  if (Is_long(v))
    seq::compilationError("[internal] long variant mismatch ...");
  int tv = Tag_val(v);
  t = Field(v, 0);
  switch (tv) {
  case 0:
    Return(Star, );
  case 1:
    Return(Int, Int64_val(t));
  case 2:
    Return(Bool, Bool_val(t));
  case 3:
    Return(Str, parse_string(t));
  case 4:
    Return(Seq, parse_string(t));
  case 5:
    Return(Range, Int64_val(Field(t, 0)), Int64_val(Field(t, 1)));
  case 6:
    Return(Tuple, parse_list(t, parse_pattern));
  case 7:
    Return(List, parse_list(t, parse_pattern));
  case 8:
    Return(Or, parse_list(t, parse_pattern));
  case 9:
    Return(Wildcard, parse_optional(t, parse_string));
  case 10:
    Return(Guarded, parse_pattern(Field(t, 0)), parse_expr(Field(t, 1)));
  case 11:
    Return(Bound, parse_string(Field(t, 0)), parse_pattern(Field(t, 1)));
  default:
    seq::compilationError("[internal] tag variant mismatch ...");
    return nullptr;
  }
#undef Return
}

unique_ptr<Stmt> parse_stmt(value val);
unique_ptr<Stmt> parse_stmt_list(value val) {
  return StmtPtr(new SuiteStmt(parse_list(val, parse_stmt)));
}

unique_ptr<Stmt> parse_stmt(value val) {
#define Return(x, ...)                                                                 \
  do {                                                                                 \
    auto _ret = make_unique<x##Stmt>(__VA_ARGS__);                                     \
    _ret->setSrcInfo(pos);                                                             \
    OcamlReturn(move(_ret));                                                           \
  } while (0)

  CAMLparam1(val);
  CAMLlocal3(p, v, t);
  CAMLlocal3(f0, f1, f2);

  auto parse_param = [](value p) {
    CAMLparam1(p);
    CAMLlocal1(v);
    v = Field(p, 1); // ignore position
    OcamlReturn(
        (Param{parse_string(Field(v, 0)), parse_optional(Field(v, 1), parse_expr),
               parse_optional(Field(v, 2), parse_expr)}));
  };

  auto pos = parse_pos(Field(val, 0));
  v = Field(val, 1);
  if (Is_long(v))
    seq::compilationError("[internal] long variant mismatch ...");
  int tv = Tag_val(v);
  t = Field(v, 0);
  switch (tv) {
  case 0:
    Return(Pass, );
  case 1:
    Return(Break, );
  case 2:
    Return(Continue, );
  case 3:
    Return(Expr, parse_expr(t));
  case 4:
    f0 = Field(t, 3);
    Return(Assign, parse_expr(Field(t, 0)), parse_expr(Field(t, 1)),
           parse_optional(Field(t, 2), parse_expr));
  case 5:
    Return(Del, parse_expr(t));
  case 6:
    Return(Print, parse_expr(t));
  case 7:
    Return(Return, parse_optional(t, parse_expr));
  case 8:
    Return(Yield, parse_optional(t, parse_expr));
  case 9:
    Return(Assert, parse_expr(t));
  case 10:
    assert(false);
    // Return(TypeAlias, parse_string(Field(t, 0)), parse_expr(Field(t, 1)));
  case 11:
    Return(While, parse_expr(Field(t, 0)), parse_stmt_list(Field(t, 1)));
  case 12:
    Return(For, parse_expr(Field(t, 0)), parse_expr(Field(t, 1)),
           parse_stmt_list(Field(t, 2)));
  case 13:
    Return(If, parse_list(t, [](value i) {
             return IfStmt::If{parse_optional(Field(i, 0), parse_expr),
                               parse_stmt_list(Field(i, 1))};
           }));
  case 14:
    Return(Match, parse_expr(Field(t, 0)), parse_list(Field(t, 1), [](value i) {
             return make_pair(parse_pattern(Field(i, 0)), parse_stmt_list(Field(i, 1)));
           }));
  case 15:
    Return(Extend, parse_expr(Field(t, 0)), parse_stmt_list(Field(t, 1)));
  case 16:
    f0 = Field(t, 0);
    Return(Import,
           make_pair(parse_string(Field(f0, 0)),
                     parse_optional(Field(f0, 1), parse_string)),
           parse_list(Field(t, 1), [](value j) {
             return make_pair(parse_string(Field(j, 0)),
                              parse_optional(Field(j, 1), parse_string));
           }));
  case 17:
    Return(
        ExternImport,
        make_pair(parse_string(Field(t, 2)), parse_optional(Field(t, 5), parse_string)),
        parse_optional(Field(t, 1), parse_expr), parse_expr(Field(t, 3)),
        parse_list(Field(t, 4), parse_param), parse_string(Field(t, 0)));
  case 18:
    Return(Try, parse_stmt_list(Field(t, 0)),
           parse_list(Field(t, 1),
                      [](value t) {
                        CAMLparam1(t);
                        CAMLlocal1(v);
                        v = Field(t, 1); // ignore position
                        OcamlReturn(
                            (TryStmt::Catch{parse_optional(Field(v, 1), parse_string),
                                            parse_optional(Field(v, 0), parse_expr),
                                            parse_stmt_list(Field(v, 2))}));
                      }),
           parse_stmt_list(Field(t, 2)));
  case 19:
    Return(Global, parse_string(t));
  case 20:
    Return(Throw, parse_expr(t));
  case 23:
    Return(Function, parse_string(Field(t, 0)), parse_optional(Field(t, 1), parse_expr),
           parse_list(Field(t, 2), parse_param), parse_list(Field(t, 3), parse_param),
           std::shared_ptr<Stmt>(parse_stmt_list(Field(t, 4))),
           parse_list(Field(t, 5), [](value i) {
             return parse_string(Field(i, 1)); // ignore position for now
           }));
  case 24:
  case 25:
    Return(Class, tv == 25, parse_string(Field(t, 0)),
           parse_list(Field(t, 1), parse_param), parse_list(Field(t, 2), parse_param),
           parse_stmt_list(Field(t, 3)), parse_list(Field(t, 4), [](value i) {
             return parse_string(Field(i, 1)); // ignore position for now
           }));
  case 27:
    Return(AssignEq, parse_expr(Field(t, 0)), parse_expr(Field(t, 1)),
           parse_string(Field(t, 2)));
  case 28:
    Return(YieldFrom, parse_expr(t));
  case 29:
    Return(With,
           parse_list(Field(t, 0),
                      [](value j) {
                        return make_pair(parse_expr(Field(j, 0)),
                                         parse_optional(Field(j, 1), parse_string));
                      }),
           parse_stmt_list(Field(t, 1)));
  case 30:
    Return(PyDef, parse_string(Field(t, 0)), parse_optional(Field(t, 1), parse_expr),
           parse_list(Field(t, 2), parse_param), parse_string(Field(t, 3)));
  default:
    seq::compilationError("[internal] tag variant mismatch ...");
    return nullptr;
  }
#undef Return
}

unique_ptr<SuiteStmt> ocamlParse(string file, string code, int line_offset,
                                 int col_offset) {
  CAMLparam0();
  CAMLlocal3(p1, f, c);
  static value *closure_f = nullptr;
  if (!closure_f)
    closure_f = (value *)caml_named_value("menhir_parse");
  f = caml_copy_string(file.c_str());
  c = caml_copy_string(code.c_str());
  value args[] = {f, c, Val_int(line_offset), Val_int(col_offset)};
  p1 = caml_callbackN(*closure_f, 4, args);
  OcamlReturn(make_unique<SuiteStmt>(parse_optional(p1, [](value v) {
    CAMLparam1(v);
    return parse_list(v, parse_stmt);
  })));
}

void initOcaml() {
  const char *argv[] = {"parser", 0};
  caml_main((char **)argv);
}

SEQ_FUNC CAMLprim value seq_ocaml_exception(value msg, value file, value line,
                                            value col) {
  CAMLparam4(msg, file, line, col);
  error(seq::SrcInfo(parse_string(file), Int_val(line), Int_val(line), Int_val(col),
                     Int_val(col)),
        parse_string(msg).c_str());
  CAMLreturn(Val_unit);
}

unique_ptr<SuiteStmt> parseCode(string file, string code, int line_offset,
                                int col_offset) {
  static bool initialized(false);
  if (!initialized) {
    initOcaml();
    initialized = true;
  }
  return ocamlParse(file, code, line_offset, col_offset);
}

unique_ptr<Expr> parseExpr(string code, const seq::SrcInfo &offset) {
  auto result = parseCode(offset.file, code, offset.line, offset.col);
  assert(result->stmts.size() == 1);
  auto s = CAST(result->stmts[0], ExprStmt);
  assert(s);
  return move(s->expr);
}

unique_ptr<SuiteStmt> parseFile(string file) {
  string result, line;
  if (file == "-") {
    while (getline(cin, line))
      result += line + "\n";
  } else {
    ifstream fin(file);
    if (!fin)
      error(fmt::format("cannot open {}", file).c_str());
    while (getline(fin, line))
      result += line + "\n";
    fin.close();
  }
  return parseCode(file, result, 0, 0);
}

} // namespace ast
} // namespace seq
