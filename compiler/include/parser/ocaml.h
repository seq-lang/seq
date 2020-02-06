#pragma once

#include <memory>
#include <string>
#include <vector>

#include "parser/ast/expr.h"
#include "parser/ast/stmt.h"
#include "seq/seq.h"

std::unique_ptr<SuiteStmt> parse_code(std::string file, std::string code,
                                      int line_offset = 0, int col_offset = 0);
std::unique_ptr<Expr> parse_expr(std::string code, const seq::SrcInfo &offset);
std::unique_ptr<SuiteStmt> parse_file(std::string file);
