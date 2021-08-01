/*
 * peg.h --- PEG parser interface.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "parser/ast.h"
#include "util/common.h"

namespace seq {
namespace ast {

/// Parse a Seq code block with the appropriate file and position offsets.
StmtPtr parseCode(const string &file, const string &code, int line_offset = 0);
/// Parse a Seq code expression.
ExprPtr parseExpr(const string &code, const seq::SrcInfo &offset);
/// Parse a Seq file.
StmtPtr parseFile(const string &file);

} // namespace ast
} // namespace seq
