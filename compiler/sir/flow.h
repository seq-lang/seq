#pragma once

#include <list>
#include <vector>

#include "base.h"
#include "value.h"
#include "var.h"

namespace seq {
namespace ir {

/// Base for flows, which represent control flow.
class Flow : public AcceptorExtend<Flow, Value> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  /// @return a clone of the value
  Flow *clone() const { return cast<Flow>(Value::clone()); }

private:
  const types::Type *doGetType() const final;
};

/// Flow that contains a series of flows or instructions.
class SeriesFlow : public AcceptorExtend<SeriesFlow, Flow> {
private:
  std::list<Value *> series;

public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  /// @return an iterator to the first instruction/flow
  auto begin() { return series.begin(); }
  /// @return an iterator beyond the last instruction/flow
  auto end() { return series.end(); }
  /// @return an iterator to the first instruction/flow
  auto begin() const { return series.begin(); }
  /// @return an iterator beyond the last instruction/flow
  auto end() const { return series.end(); }

  /// @return a pointer to the first instruction/flow
  Value *front() { return series.front(); }
  /// @return a pointer to the last instruction/flow
  Value *back() { return series.back(); }
  /// @return a pointer to the first instruction/flow
  const Value *front() const { return series.front(); }
  /// @return a pointer to the last instruction/flow
  const Value *back() const { return series.back(); }

  /// Inserts an instruction/flow at the given position.
  /// @param pos the position
  /// @param v the flow or instruction
  /// @return an iterator to the newly added instruction/flow
  template <typename It> auto insert(It pos, Value *v) { return series.insert(pos, v); }
  /// Appends an instruction/flow.
  /// @param f the flow or instruction
  void push_back(Value *f) { series.push_back(f); }

  /// Erases the item at the supplied position.
  /// @param pos the position
  /// @return the iterator beyond the removed flow or instruction
  template <typename It> auto erase(It pos) { return series.erase(pos); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  std::vector<Value *> doGetUsedValues() const override {
    return std::vector<Value *>(series.begin(), series.end());
  }
  int doReplaceUsedValue(int id, Value *newValue) override;

  Value *doClone() const override;
};

/// Flow representing a while loop.
class WhileFlow : public AcceptorExtend<WhileFlow, Flow> {
private:
  /// the condition
  Value *cond;
  /// the body
  Value *body;

public:
  static const char NodeId;

  /// Constructs a while loop.
  /// @param cond the condition
  /// @param body the body
  /// @param name the flow's name
  WhileFlow(Value *cond, Flow *body, std::string name = "")
      : AcceptorExtend(std::move(name)), cond(cond), body(body) {}

  /// @return the condition
  Value *getCond() { return cond; }
  /// @return the condition
  const Value *getCond() const { return cond; }
  /// Sets the condition.
  /// @param c the new condition
  void setCond(Value *c) { cond = c; }

  /// @return the body
  Flow *getBody() { return cast<Flow>(body); }
  /// @return the body
  const Flow *getBody() const { return cast<Flow>(body); }
  /// Sets the body.
  /// @param f the new value
  void setBody(Flow *f) { body = f; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  std::vector<Value *> doGetUsedValues() const override { return {cond, body}; }
  int doReplaceUsedValue(int id, Value *newValue) override;

  Value *doClone() const override;
};

/// Flow representing a for loop.
class ForFlow : public AcceptorExtend<ForFlow, Flow> {
private:
  /// the iterator
  Value *iter;
  /// the body
  Value *body;

  /// the variable
  Var *var;

public:
  static const char NodeId;

  /// Constructs a for loop.
  /// @param setup the setup
  /// @param iter the iterator
  /// @param body the body
  /// @param update the update
  /// @param name the flow's name
  ForFlow(Value *iter, Flow *body, Var *var, std::string name = "")
      : AcceptorExtend(std::move(name)), iter(iter), body(body), var(var) {}

  /// @return the iter
  Value *getIter() { return iter; }
  /// @return the iter
  const Value *getIter() const { return iter; }
  /// Sets the iter.
  /// @param f the new iter
  void setIter(Value *f) { iter = f; }

  /// @return the body
  Flow *getBody() { return cast<Flow>(body); }
  /// @return the body
  const Flow *getBody() const { return cast<Flow>(body); }
  /// Sets the body.
  /// @param f the new body
  void setBody(Flow *f) { body = f; }

  /// @return the var
  Var *getVar() { return var; }
  /// @return the var
  const Var *getVar() const { return var; }
  /// Sets the var.
  /// @param c the new var
  void setVar(Var *c) { var = c; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  std::vector<Value *> doGetUsedValues() const override { return {iter, body}; }
  int doReplaceUsedValue(int id, Value *newValue) override;

  std::vector<Var *> doGetUsedVariables() const override { return {var}; }
  int doReplaceUsedVariable(int id, Var *newVar) override;

  Value *doClone() const override;
};

/// Flow representing an if statement.
class IfFlow : public AcceptorExtend<IfFlow, Flow> {
private:
  /// the condition
  Value *cond;
  /// the true branch
  Value *trueBranch;
  /// the false branch
  Value *falseBranch;

public:
  static const char NodeId;

  /// Constructs an if.
  /// @param cond the condition
  /// @param trueBranch the true branch
  /// @param falseBranch the false branch
  /// @param name the flow's name
  IfFlow(Value *cond, Flow *trueBranch, Flow *falseBranch = nullptr,
         std::string name = "")
      : AcceptorExtend(std::move(name)), cond(cond), trueBranch(trueBranch),
        falseBranch(falseBranch) {}

  /// @return the true branch
  Flow *getTrueBranch() { return cast<Flow>(trueBranch); }
  /// @return the true branch
  const Flow *getTrueBranch() const { return cast<Flow>(trueBranch); }
  /// Sets the true branch.
  /// @param f the new true branch
  void setTrueBranch(Flow *f) { trueBranch = f; }

  /// @return the false branch
  Flow *getFalseBranch() { return cast<Flow>(falseBranch); }
  /// @return the false branch
  const Flow *getFalseBranch() const { return cast<Flow>(falseBranch); }
  /// Sets the false.
  /// @param f the new false
  void setFalseBranch(Flow *f) { falseBranch = f; }

  /// @return the condition
  Value *getCond() { return cond; }
  /// @return the condition
  const Value *getCond() const { return cond; }
  /// Sets the condition.
  /// @param c the new condition
  void setCond(Value *c) { cond = c; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  std::vector<Value *> doGetUsedValues() const override;
  int doReplaceUsedValue(int id, Value *newValue) override;

  Value *doClone() const override;
};

/// Flow representing a try-catch statement.
class TryCatchFlow : public AcceptorExtend<TryCatchFlow, Flow> {
public:
  /// Class representing a catch clause.
  class Catch {
  private:
    /// the handler
    Value *handler;
    /// the catch type, may be nullptr
    types::Type *type;
    /// the catch variable, may be nullptr
    Var *catchVar;

  public:
    explicit Catch(Flow *handler, types::Type *type = nullptr, Var *catchVar = nullptr)
        : handler(handler), type(type), catchVar(catchVar) {}

    /// @return the handler
    Flow *getHandler() { return cast<Flow>(handler); }
    /// @return the handler
    const Flow *getHandler() const { return cast<Flow>(handler); }
    /// Sets the handler.
    /// @param h the new value
    void setHandler(Flow *h) { handler = h; }

    /// @return the catch type, may be nullptr
    types::Type *getType() { return type; }
    /// @return the catch type, may be nullptr
    const types::Type *getType() const { return type; }
    /// Sets the catch type.
    /// @param t the new type, nullptr for catch all
    void setType(types::Type *t) { type = t; }

    /// @return the variable, may be nullptr
    Var *getVar() { return catchVar; }
    /// @return the variable, may be nullptr
    const Var *getVar() const { return catchVar; }
    /// Sets the variable.
    /// @param v the new value, may be nullptr
    void setVar(Var *v) { catchVar = v; }
  };

private:
  /// the catch clauses
  std::list<Catch> catches;

  /// the body
  Value *body;
  /// the finally, may be nullptr
  Value *finally;

public:
  static const char NodeId;

  /// Constructs an try-catch.
  /// @param name the's name
  /// @param body the body
  /// @param finally the finally
  explicit TryCatchFlow(Flow *body, Flow *finally = nullptr, std::string name = "")
      : AcceptorExtend(std::move(name)), body(body), finally(finally) {}

  /// @return the body
  Flow *getBody() { return cast<Flow>(body); }
  /// @return the body
  const Flow *getBody() const { return cast<Flow>(body); }
  /// Sets the body.
  /// @param f the new
  void setBody(Flow *f) { body = f; }

  /// @return the finally
  Flow *getFinally() { return cast<Flow>(finally); }
  /// @return the finally
  const Flow *getFinally() const { return cast<Flow>(finally); }
  /// Sets the finally.
  /// @param f the new
  void setFinally(Flow *f) { finally = f; }

  /// @return an iterator to the first catch
  auto begin() { return catches.begin(); }
  /// @return an iterator beyond the last catch
  auto end() { return catches.end(); }
  /// @return an iterator to the first catch
  auto begin() const { return catches.begin(); }
  /// @return an iterator beyond the last catch
  auto end() const { return catches.end(); }

  /// @return a reference to the first catch
  auto &front() { return catches.front(); }
  /// @return a reference to the last catch
  auto &back() { return catches.back(); }
  /// @return a reference to the first catch
  auto &front() const { return catches.front(); }
  /// @return a reference to the last catch
  auto &back() const { return catches.back(); }

  /// Inserts a catch at the given position.
  /// @param pos the position
  /// @param v the catch
  /// @return an iterator to the newly added catch
  template <typename It> auto insert(It pos, Catch v) { return catches.insert(pos, v); }

  /// Appends a catch.
  /// @param v the catch
  void push_back(Catch v) { catches.push_back(v); }

  /// Emplaces a catch.
  /// @tparam Args the catch constructor args
  template <typename... Args> void emplace_back(Args &&... args) {
    catches.emplace_back(std::forward<Args>(args)...);
  }

  /// Erases a catch at the given position.
  /// @param pos the position
  /// @return the iterator beyond the erased catch
  template <typename It> auto erase(It pos) { return catches.erase(pos); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  std::vector<Value *> doGetUsedValues() const override;
  int doReplaceUsedValue(int id, Value *newValue) override;

  std::vector<types::Type *> doGetUsedTypes() const override;
  int doReplaceUsedType(const std::string &name, types::Type *newType) override;

  std::vector<Var *> doGetUsedVariables() const override;
  int doReplaceUsedVariable(int id, Var *newVar) override;

  Value *doClone() const override;
};

/// Flow that represents a pipeline. Pipelines with only function
/// stages are expressions and have a concrete type. Pipelines with
/// generator stages are not expressions and have no type. This
/// representation allows for stages that output generators but do
/// not get explicitly iterated in the pipeline, since generator
/// stages are denoted by a separate flag.
class PipelineFlow : public AcceptorExtend<PipelineFlow, Flow> {
public:
  /// Represents a single stage in a pipeline.
  class Stage {
  private:
    /// the function being (partially) called in this stage
    Value *func;
    /// the function arguments, where null represents where
    /// previous pipeline output should go
    std::vector<Value *> args;
    /// true if this stage is a generator
    bool generator;
    /// true if this stage is marked parallel
    bool parallel;

  public:
    /// Constructs a pipeline stage.
    /// @param func the function being called
    /// @param args call arguments, with exactly one null entry
    /// @param generator whether this stage is a generator stage
    /// @param parallel whether this stage is parallel
    Stage(Value *func, std::vector<Value *> args, bool generator, bool parallel)
        : func(func), args(std::move(args)), generator(generator), parallel(parallel) {}

    /// @return an iterator to the first argument
    auto begin() { return args.begin(); }
    /// @return an iterator beyond the last argument
    auto end() { return args.end(); }
    /// @return an iterator to the first argument
    auto begin() const { return args.begin(); }
    /// @return an iterator beyond the last argument
    auto end() const { return args.end(); }

    /// @return a pointer to the first argument
    Value *front() { return args.front(); }
    /// @return a pointer to the last argument
    Value *back() { return args.back(); }
    /// @return a pointer to the first argument
    const Value *front() const { return args.front(); }
    /// @return a pointer to the last argument
    const Value *back() const { return args.back(); }

    /// @return the called function
    Value *getFunc() { return func; }
    /// @return the called function
    const Value *getFunc() const { return func; }

    /// Sets the stage's generator flag.
    /// @param v the new value
    void setGenerator(bool v = true) { generator = v; }
    /// @return whether this stage is a generator stage
    bool isGenerator() const { return generator; }
    /// Sets the stage's parallel flag.
    /// @param v the new value
    void setParallel(bool v = true) { parallel = v; }
    /// @return whether this stage is parallel
    bool isParallel() const { return parallel; }
    /// @return the output type of this stage
    const types::Type *getOutputType() const;
    /// @return deep copy of this stage; used to clone pipelines
    Stage clone() const;

    friend class PipelineFlow;
  };

private:
  /// pipeline stages
  std::vector<Stage> stages;

public:
  static const char NodeId;

  /// Constructs a pipeline flow.
  /// @param stages vector of pipeline stages
  /// @param name the name
  explicit PipelineFlow(std::vector<Stage> stages = {}, std::string name = "")
      : AcceptorExtend(std::move(name)), stages(std::move(stages)) {}

  /// @return an iterator to the first stage
  auto begin() { return stages.begin(); }
  /// @return an iterator beyond the last stage
  auto end() { return stages.end(); }
  /// @return an iterator to the first stage
  auto begin() const { return stages.begin(); }
  /// @return an iterator beyond the last stage
  auto end() const { return stages.end(); }

  /// @return a pointer to the first stage
  Stage &front() { return stages.front(); }
  /// @return a pointer to the last stage
  Stage &back() { return stages.back(); }
  /// @return a pointer to the first stage
  const Stage &front() const { return stages.front(); }
  /// @return a pointer to the last stage
  const Stage &back() const { return stages.back(); }

  /// Inserts a stage
  /// @param pos the position
  /// @param v the stage
  /// @return an iterator to the newly added stage
  template <typename It> auto insert(It pos, Stage v) { return stages.insert(pos, v); }
  /// Appends an stage.
  /// @param v the stage
  void push_back(Stage v) { stages.push_back(std::move(v)); }

  /// Erases the item at the supplied position.
  /// @param pos the position
  /// @return the iterator beyond the removed stage
  template <typename It> auto erase(It pos) { return stages.erase(pos); }

  /// Emplaces a stage.
  /// @param args the args
  template <typename... Args> void emplace_back(Args &&... args) {
    stages.emplace_back(std::forward<Args>(args)...);
  }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  std::vector<Value *> doGetUsedValues() const override;
  int doReplaceUsedValue(int id, Value *newValue) override;

  Value *doClone() const override;
};

} // namespace ir
} // namespace seq

// See https://github.com/fmtlib/fmt/issues/1283.
namespace fmt {
using seq::ir::Flow;

template <typename Char>
struct formatter<Flow, Char> : fmt::v6::internal::fallback_formatter<Flow, Char> {};
} // namespace fmt
