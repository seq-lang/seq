/**
 * doc.h
 * Documentation AST walker.
 *
 * Reads docstrings and generates documentation of a given AST node.
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
#include "parser/context.h"
#include "parser/visitors/visitor.h"
#include "util/nlohmann/json.hpp"

using nlohmann::json;

namespace seq {
namespace ast {

struct DocContext;
struct DocShared {
  int itemID;
  json j;
  unordered_map<string, shared_ptr<DocContext>> modules;
  string argv0;
  DocShared() : itemID(1) {}
};

struct DocContext : public Context<int> {
  shared_ptr<DocShared> shared;
  string file;
  DocContext(shared_ptr<DocShared> s) : Context<int>(""), shared(s) {
    stack.push_front(vector<string>());
  }
  virtual ~DocContext() {}
  shared_ptr<int> find(const string &s);
};

struct DocVisitor : public CallbackASTVisitor<json, string, string> {
  shared_ptr<DocContext> ctx;
  json resultExpr;
  string resultStmt;

public:
  DocVisitor(shared_ptr<DocContext> ctx) : ctx(ctx) {}
  json transform(const ExprPtr &e) override;
  string transform(const StmtPtr &e) override;
  string transform(const PatternPtr &e) override { return ""; }
  static json apply(const string &argv0, const vector<string> &files);
  void transformModule(StmtPtr stmt);
  json jsonify(const seq::SrcInfo &s);
  vector<StmtPtr> flatten(StmtPtr stmt, string *docstr = nullptr, bool deep = true);

public:
  virtual void visit(const IdExpr *) override;
  virtual void visit(const IndexExpr *) override;
  virtual void visit(const FunctionStmt *) override;
  virtual void visit(const ClassStmt *) override;
  virtual void visit(const AssignStmt *) override;
  virtual void visit(const ImportStmt *) override;
};

} // namespace ast
} // namespace seq
