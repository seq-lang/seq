#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/ast/stmt.h"

namespace seq {
namespace ast {

unique_ptr<SuiteStmt> parseCode(string file, string code, int line_offset = 0,
                                int col_offset = 0);
unique_ptr<Expr> parseExpr(string code, const seq::SrcInfo &offset);
unique_ptr<SuiteStmt> parseFile(string file);

} // namespace ast
} // namespace seq
