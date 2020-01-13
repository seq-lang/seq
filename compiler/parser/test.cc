#include <string>
#include <cstdio>
#include <algorithm>
#include <memory>
#include <vector>
#include <variant>
#include <iostream>
#include <tuple>
#include <cassert>
using namespace std;

#include <fmt/format.h>
#include <fmt/ostream.h>
#include "parser/ocaml.h"

int xyzmain(int argc, char **argv) {
  // initialize_parser();

  auto tests = vector<string> {
      "True",
      "1",
      "1u",
      "3.14",
      "'s'",
      "'a' \"b\" 'c\\''",
      "f'a' f\"b\" f'c\\''",
      "k'ACGT'",
      "s\"ACGT\" s'GCTA'",
      "p\"ACGT\" p'GCTA'",
      "x", // 11
      "*y",
      "(1,)",
      "(1, 2)",
      "[]",
      "[1, 2, 3]",
      "{}",
      "{1, 2}",
      "{1: True, 2: 'z'}",
      "(i for j in k)",
      "[i for j in k if l]",
      "{i for j, k in l if m for n in o for p in r if s}",
      "{a: b for c in d if e}",
      "x if a else b",
      "+5", // 21
      "+a",
      "a % b",
      "a + b << c * d - e ** 1.11",
      "a |> b ||> c",
      "x[1]",
      "x[2, 3]",
      "a(5)",
      "b(a, b=1)",
      "c()",
      "d(x=15, e)", // 31
      "a[1:2]",
      "a[:1]",
      "a[::]",
      "a[1::-1]",
      "a[1:2:4]",
      "a.x",
      // "...",
      "typeof(1+2)",
      "__ptr__(call(5.15))",
      "lambda x, y: x + y - f(x)",
      "(yield) + 1",
      "pass",
      "continue",
      "break",
      "x = 1",
      "y := 2",
      "z : T = 3",
      //"z : T",
      "z += 5",
      "del x[1]",
      "print a, b,",
      "print a",
      "print",
      "return",
      "yield 5",
      "assert True",
      "type x = y[T]",
      "while True:\n  f(x)\n  pass\n",
      "for a, b, c in x:\n  x = 1; y = 2",
      "if True:\n  pass",
      "if True:\n  pass\nelif y:\n  x += 1",
      "if True:\n  pass\nelse:\n  x += 1",
      "if True:\n  pass\nelif y:\n  x += 1\nelse:\n  y += 1",
      "extend H[T]:\n  pass\n  pass",
      "try:\n  pass",
      "try:\n  pass\nexcept x.y:\n  pass",
      "try:\n  pass\nexcept x.y:\n  pass\nexcept z.w as q:\n  yield",
      "try:\n  pass\nfinally:\n  pass",
      "global a",
      "raise x(\"ooo\booo!\")",
      "prefetch x[1], y[b]",
      "import a",
      "import b, c as d, e",
      "from x import y",
      "from x import y as z, w",
      "def f(x):\n return x",
      "def f(x) -> int:\n return x",
      "def f[T](x: T, y = 'w') -> z:\n return x",
      "class A:\n x: int",
      "class A[T]:\n x: T",
      "type A(x: int):\n def f(x): pass",
      "x: T",
      "match x: \n"
      " case ...: pass\n"
      " case -1: pass\n"
      " case True: pass\n"
      " case 'str': pass\n"
      " case s'A_GT': pass\n"
      " case -1 ... 3: pass\n"
      " case (1, 2, ...): pass\n"
      " case [1, ..., -1]: pass\n"
      " case 1 or 2: pass\n"
      " case x if y == 2: pass\n"
      " case (1, 2, 3) as b: pass\n"
      " case _: pass"
    };

  // for (int i = 0; i < tests.size(); i++) {
  //   auto stmts = parse(fmt::format("<test_{}>", i), tests[i]);
  //   //assert (stmts.size() == 1);
  //   for (auto &s: stmts)
  //     fmt::print("{:02d}:  {}\n", i, *s);
  // }

  return 0;
}
