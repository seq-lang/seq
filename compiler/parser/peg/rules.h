/*
 * peg.h --- PEG parser interface.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

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

namespace seq {
namespace ast {

struct ParseContext {
  std::stack<int> indent;
  int parens;
  int line_offset, col_offset;
  ParseContext(int parens = 0, int line_offset = 0, int col_offset = 0)
      : parens(parens), line_offset(line_offset), col_offset(col_offset) {}
};

} // namespace ast
} // namespace seq

void init_rules(peg::Grammar &);
void init_actions(peg::Grammar &);
