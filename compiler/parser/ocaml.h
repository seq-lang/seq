#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/ast.h"

namespace seq {
namespace ast {

std::unique_ptr<SuiteStmt> parseCode(std::string file, std::string code,
                                     int line_offset = 0, int col_offset = 0);
std::unique_ptr<Expr> parseExpr(std::string code, const seq::SrcInfo &offset);
std::unique_ptr<SuiteStmt> parseFile(std::string file);

} // namespace ast
} // namespace seq
