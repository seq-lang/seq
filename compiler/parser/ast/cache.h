/**
 * cache.h
 */

#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/common.h"

namespace seq {
namespace ast {

struct TransformItem;

struct Cache {
  std::unordered_map<std::string, int> moduleNames;
  std::unordered_map<std::string, std::string> reverseLookup;
  int generatedID;

  struct Import {
    std::string filename;
    std::shared_ptr<Context<TransformItem>> ctx;
  };
  std::string argv0;
  /// By convention, stdlib is stored as ""
  std::unordered_map<std::string, Import> imports;

  std::unordered_set<std::string> variardics;
  std::unordered_map<std::string, StmtPtr> asts;
  std::unordered_map<std::string, types::TypePtr> astTypes;

public:
  Cache(const std::string &argv0 = "") : argv0(argv0), generatedID(0) {}
};

} // namespace ast
} // namespace seq
