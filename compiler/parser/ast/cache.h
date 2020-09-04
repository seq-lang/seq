/**
 * cache.h
 */

#pragma once

#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/ast/context.h"
#include "parser/common.h"

namespace seq {
namespace ast {

struct TransformItem;

struct Cache {
  std::unordered_map<std::string, int> moduleNames;
  std::unordered_map<std::string, std::string> reverseLookup;
  int generatedID;
  int unboundCount;

  struct Import {
    std::string filename;
    std::shared_ptr<Context<TransformItem>> ctx;
  };
  std::string argv0;
  /// By convention, stdlib is stored as ""
  std::unordered_map<std::string, Import> imports;

  std::set<std::string> variardics;
  std::unordered_map<std::string, StmtPtr> asts;

  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::vector<types::FuncTypePtr>>>
      classMethods;
  std::unordered_map<std::string, std::vector<std::pair<std::string, types::TypePtr>>>
      classMembers;
  std::unordered_map<std::string, std::unordered_map<std::string, types::TypePtr>>
      realizations;
  std::unordered_map<std::string, std::vector<std::pair<std::string, types::TypePtr>>>
      memberRealizations;
  std::unordered_map<std::string, StmtPtr> realizationAsts;
  std::unordered_map<std::string, types::TypePtr> partials;

public:
  Cache(const std::string &argv0 = "")
      : generatedID(0), unboundCount(0), argv0(argv0) {}
};

} // namespace ast
} // namespace seq
