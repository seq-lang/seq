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

#include "parser/ast/ast.h"
#include "parser/ast/context.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "util/nlohmann/json.hpp"

using nlohmann::json;

namespace seq {
namespace ast {

struct DocContext;
struct DocShared {
  int itemID;
  json j;
  std::unordered_map<std::string, std::shared_ptr<DocContext>> modules;
  std::string argv0;
  DocShared() : itemID(1) {}
};

struct DocContext : public Context<int> {
  std::shared_ptr<DocShared> shared;
  std::string file;
  DocContext(std::shared_ptr<DocShared> s) : Context<int>(""), shared(s) {
    stack.push_front(std::vector<std::string>());
  }
  virtual ~DocContext() {}
  std::shared_ptr<int> find(const std::string &s);
};

struct DocVisitor : public CallbackASTVisitor<json, std::string, std::string> {
  std::shared_ptr<DocContext> ctx;
  json resultExpr;
  std::string resultStmt;

public:
  DocVisitor(std::shared_ptr<DocContext> ctx) : ctx(ctx) {}
  json transform(const ExprPtr &e) override;
  std::string transform(const StmtPtr &e) override;
  std::string transform(const PatternPtr &e) override { return ""; }
  static json apply(const std::string &argv0, const std::vector<std::string> &files);
  void transformModule(StmtPtr stmt);
  json jsonify(const seq::SrcInfo &s);
  std::vector<StmtPtr> flatten(StmtPtr stmt, std::string *docstr = nullptr,
                               bool deep = true);

public:
  virtual void visit(const IdExpr *) override;
  virtual void visit(const IndexExpr *) override;
  virtual void visit(const FunctionStmt *) override;
  virtual void visit(const ClassStmt *) override;
  virtual void visit(const ExtendStmt *) override;
  virtual void visit(const AssignStmt *) override;
  virtual void visit(const ImportStmt *) override;
  virtual void visit(const ExternImportStmt *) override;
};

} // namespace ast
} // namespace seq
