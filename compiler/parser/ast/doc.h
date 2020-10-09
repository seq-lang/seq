#pragma once

#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"
#include "parser/context.h"
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

struct DocContext : public VTable<int> {
  std::shared_ptr<DocShared> shared;
  std::string file;
  DocContext(std::shared_ptr<DocShared> s) : shared(s) {
    stack.push(std::vector<std::string>());
  }
  virtual ~DocContext() {}
  std::shared_ptr<int> find(const std::string &s);
};

struct DocExprVisitor : public ExprVisitor {
  std::shared_ptr<DocContext> ctx;
  json result;

public:
  DocExprVisitor(std::shared_ptr<DocContext> ctx) : ctx(ctx) {}
  json transform(const ExprPtr &e);

public:
  virtual void visit(const IdExpr *) override;
  virtual void visit(const IndexExpr *) override;
};

struct DocStmtVisitor : public StmtVisitor {
  std::shared_ptr<DocContext> ctx;
  std::string result;

public:
  DocStmtVisitor(std::shared_ptr<DocContext> ctx) : ctx(ctx), result("") {}
  json transform(const ExprPtr &e);
  std::string transform(const StmtPtr &e);
  void transformModule(StmtPtr stmt);

  static json apply(const std::string &argv0, const std::vector<std::string> &files);

public:
  virtual void visit(const FunctionStmt *) override;
  virtual void visit(const ClassStmt *) override;
  virtual void visit(const ExtendStmt *) override;
  virtual void visit(const AssignStmt *) override;
  virtual void visit(const ImportStmt *) override;
  virtual void visit(const TypeAliasStmt *) override;
  virtual void visit(const ExternImportStmt *) override;

  json jsonify(const seq::SrcInfo &s);
  std::vector<StmtPtr> flatten(StmtPtr stmt, std::string *docstr = nullptr,
                               bool deep = true);
};

} // namespace ast
} // namespace seq
