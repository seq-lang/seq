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
  ParseContext(int parens = 0) : parens(parens) {}
};

void init_rules(peg::parser &);
void init_actions(peg::parser &);

} // namespace ast
} // namespace seq
