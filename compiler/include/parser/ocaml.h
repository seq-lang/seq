#pragma once

#include <memory>
#include <string>
#include <vector>

#include "parser/expr.h"
#include "parser/stmt.h"

std::vector<std::unique_ptr<Stmt>> parse(std::string file, std::string code);
std::unique_ptr<Expr> parse_expr(std::string code);
std::vector<std::unique_ptr<Stmt>> parse_file(std::string file);
