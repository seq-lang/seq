/**
 * cache.h
 */

#pragma once

#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "parser/ast/ast/stmt.h"
#include "parser/ast/context.h"
#include "parser/common.h"

namespace seq {
namespace ast {

struct TransformItem;

struct Cache {
  unordered_map<string, int> moduleNames;
  unordered_map<string, string> reverseLookup;
  int generatedID;
  int unboundCount;

  struct Import {
    string filename;
    shared_ptr<Context<TransformItem>> ctx;
  };
  string argv0;
  /// By convention, stdlib is stored as ""
  unordered_map<string, Import> imports;

  set<string> variardics;
  unordered_map<string, StmtPtr> asts;

  unordered_map<string,
                     unordered_map<string, vector<types::FuncTypePtr>>>
      classMethods;
  unordered_map<string, vector<std::pair<string, types::TypePtr>>>
      classMembers;
  unordered_map<string, unordered_map<string, types::TypePtr>>
      realizations;
  unordered_map<string, vector<std::pair<string, types::TypePtr>>>
      memberRealizations;
  unordered_map<string, StmtPtr> realizationAsts;
  unordered_map<string, types::TypePtr> partials;

public:
  Cache(const string &argv0 = "")
      : generatedID(0), unboundCount(0), argv0(argv0) {}
};

} // namespace ast
} // namespace seq
