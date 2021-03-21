/*
 * doc.h --- Seq documentation generator.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/ctx.h"
#include "parser/visitors/visitor.h"
#include "util/nlohmann/json.hpp"

namespace seq {
namespace ast {

struct DocContext;
struct DocShared {
  int itemID;
  nlohmann::json j;
  unordered_map<string, shared_ptr<DocContext>> modules;
  string argv0;
  DocShared() : itemID(1) {}
};

struct DocContext : public Context<int> {
  shared_ptr<DocShared> shared;
  explicit DocContext(shared_ptr<DocShared> shared)
      : Context<int>(""), shared(move(shared)) {
    stack.push_front(vector<string>());
  }
  shared_ptr<int> find(const string &s) const override;
};

struct DocVisitor : public CallbackASTVisitor<nlohmann::json, string> {
  shared_ptr<DocContext> ctx;
  nlohmann::json resultExpr;
  string resultStmt;

public:
  explicit DocVisitor(shared_ptr<DocContext> ctx) : ctx(move(ctx)) {}
  static nlohmann::json apply(const string &argv0, const vector<string> &files);

  nlohmann::json transform(const ExprPtr &e) override;
  string transform(const StmtPtr &e) override;

  void transformModule(StmtPtr stmt);
  nlohmann::json jsonify(const seq::SrcInfo &s);
  vector<StmtPtr> flatten(StmtPtr stmt, string *docstr = nullptr, bool deep = true);

public:
  void visit(IntExpr *) override;
  void visit(IdExpr *) override;
  void visit(IndexExpr *) override;
  void visit(FunctionStmt *) override;
  void visit(ClassStmt *) override;
  void visit(AssignStmt *) override;
  void visit(ImportStmt *) override;
};

} // namespace ast
} // namespace seq
