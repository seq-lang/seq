#pragma once

#include <list>
#include <vector>

#include "base.h"
#include "value.h"
#include "var.h"

namespace seq {
namespace ir {

/// Base fors, which represent control.
class Flow : public AcceptorExtend<Flow, Value> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  types::Type *getType() const override { return nullptr; }

  virtual ~Flow() noexcept = default;

  /// @return a clone of the value
  std::unique_ptr<Flow> clone() const { return std::unique_ptr<Flow>(static_cast<Flow *>(doClone())); }

};

using FlowPtr = std::unique_ptr<Flow>;

/// Flow that contains a series of flows or instructions.
class SeriesFlow : public AcceptorExtend<SeriesFlow, Flow> {
public:
  using iterator = std::list<ValuePtr>::iterator;
  using const_iterator = std::list<ValuePtr>::const_iterator;
  using reference = std::list<ValuePtr>::reference;
  using const_reference = std::list<ValuePtr>::const_reference;

private:
  std::list<ValuePtr> series;

public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  /// @return an iterator to the first instruction/flow
  iterator begin() { return series.begin(); }
  /// @return an iterator beyond the last instruction/flow
  iterator end() { return series.end(); }
  /// @return an iterator to the first instruction/flow
  const_iterator begin() const { return series.begin(); }
  /// @return an iterator beyond the last instruction/flow
  const_iterator end() const { return series.end(); }

  /// @return a reference to the first instruction/flow
  reference front() { return series.front(); }
  /// @return a reference to the last instruction/flow
  reference back() { return series.back(); }
  /// @return a reference to the first instruction/flow
  const_reference front() const { return series.front(); }
  /// @return a reference to the last instruction/flow
  const_reference back() const { return series.back(); }

  /// Inserts an instruction/flow at the given position.
  /// @param pos the position
  /// @param v the flow or instruction
  /// @return an iterator to the newly added instruction/flow
  iterator insert(iterator pos, ValuePtr v) { return series.insert(pos, std::move(v)); }
  /// Inserts an instruction/flow at the given position.
  /// @param pos the position
  /// @param v the flow or instruction
  /// @return an iterator to the newly added instruction/flow
  iterator insert(const_iterator pos, ValuePtr v) {
    return series.insert(pos, std::move(v));
  }
  /// Appends an instruction/flow.
  /// @param f the flow or instruction
  void push_back(ValuePtr f) { series.push_back(std::move(f)); }

  /// Erases the item at the supplied position.
  /// @param pos the position
  /// @return the iterator beyond the removed flow or instruction
  iterator erase(iterator pos) { return series.erase(pos); }
  /// Erases the item at the supplied position.
  /// @param pos the position
  /// @return the iterator beyond the removed flow or instruction
  iterator erase(const_iterator pos) { return series.erase(pos); }

  /// @return true if the series contains other flows.
  bool containsFlows() const;

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Flow representing a while loop.
class WhileFlow : public AcceptorExtend<WhileFlow, Flow> {
private:
  /// the condition
  ValuePtr cond;
  /// the body
  FlowPtr body;

public:
  static const char NodeId;

  /// Constructs a while loop.
  /// @param cond the condition
  /// @param body the body
  /// @param name the flow's name
  WhileFlow(ValuePtr cond, FlowPtr body, std::string name = "")
      : AcceptorExtend(std::move(name)), cond(std::move(cond)), body(std::move(body)) {}

  /// @return the condition
  const ValuePtr &getCond() const { return cond; }
  /// Sets the condition.
  /// @param c the new condition
  void setCond(ValuePtr c) { cond = std::move(c); }

  /// @return the body
  const FlowPtr &getBody() const { return body; }
  /// Sets the body.
  /// @param f the new value
  void setBody(FlowPtr f) { body = std::move(f); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Flow representing a for loop.
class ForFlow : public AcceptorExtend<ForFlow, Flow> {
private:
  /// the iterator
  ValuePtr iter;
  /// the body
  FlowPtr body;

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
  ForFlow(ValuePtr iter, FlowPtr body, Var *var, std::string name = "")
      : AcceptorExtend(std::move(name)), iter(std::move(iter)), body(std::move(body)),
        var(var) {}

  /// @return the iter
  const ValuePtr &getIter() const { return iter; }
  /// Sets the iter.
  /// @param f the new iter
  void setIter(ValuePtr f) { iter = std::move(f); }

  /// @return the body
  const FlowPtr &getBody() const { return body; }
  /// Sets the body.
  /// @param f the new body
  void setBody(FlowPtr f) { body = std::move(f); }

  /// @return the var
  Var *getVar() const { return var; }
  /// Sets the var.
  /// @param c the new var
  void setVar(Var *c) { var = c; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Flow representing an if statement.
class IfFlow : public AcceptorExtend<IfFlow, Flow> {
private:
  /// the condition
  ValuePtr cond;
  /// the true branch
  FlowPtr trueBranch;
  /// the false branch
  FlowPtr falseBranch;

public:
  static const char NodeId;

  /// Constructs an if.
  /// @param cond the condition
  /// @param trueBranch the true branch
  /// @param falseBranch the false branch
  /// @param name the flow's name
  IfFlow(ValuePtr cond, FlowPtr trueBranch, FlowPtr falseBranch = nullptr,
         std::string name = "")
      : AcceptorExtend(std::move(name)), cond(std::move(cond)),
        trueBranch(std::move(trueBranch)), falseBranch(std::move(falseBranch)) {}

  /// @return the true branch
  const FlowPtr &getTrueBranch() const { return trueBranch; }
  /// Sets the true branch.
  /// @param f the new true branch
  void setTrueBranch(FlowPtr f) { trueBranch = std::move(f); }

  /// @return the false branch
  const FlowPtr &getFalseBranch() const { return falseBranch; }
  /// Sets the false.
  /// @param f the new false
  void setFalseBranch(FlowPtr f) { falseBranch = std::move(f); }

  /// @return the condition
  const ValuePtr &getCond() const { return cond; }
  /// Sets the condition.
  /// @param c the new condition
  void setCond(ValuePtr c) { cond = std::move(c); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Flow representing a try-catch statement.
class TryCatchFlow : public AcceptorExtend<TryCatchFlow, Flow> {
public:
  /// Struct representing a catch clause.
  struct Catch {
    /// the handler
    FlowPtr handler;
    /// the catch type, may be nullptr
    types::Type *type;
    /// the catch variable, may be nullptr
    Var *catchVar;

    explicit Catch(FlowPtr handler, types::Type *type = nullptr,
                   Var *catchVar = nullptr)
        : handler(std::move(handler)), type(type), catchVar(catchVar) {}
  };

  using iterator = std::list<Catch>::iterator;
  using const_iterator = std::list<Catch>::const_iterator;
  using reference = std::list<Catch>::reference;
  using const_reference = std::list<Catch>::const_reference;

private:
  /// the catch clauses
  std::list<Catch> catches;

  /// the body
  FlowPtr body;
  /// the finally, may be nullptr
  FlowPtr finally;

public:
  static const char NodeId;

  /// Constructs an try-catch.
  /// @param name the's name
  /// @param body the body
  /// @param finally the finally
  explicit TryCatchFlow(FlowPtr body, FlowPtr finally = nullptr,
                        std::string name = "")
      : AcceptorExtend(std::move(name)), body(std::move(body)),
        finally(std::move(finally)) {}

  /// @return the body
  const FlowPtr &getBody() const { return body; }
  /// Sets the body.
  /// @param f the new
  void setBody(FlowPtr f) { body = std::move(f); }

  /// @return the finally
  const FlowPtr &getFinally() const { return finally; }
  /// Sets the finally.
  /// @param f the new
  void setFinally(FlowPtr f) { finally = std::move(f); }

  /// @return an iterator to the first catch
  iterator begin() { return catches.begin(); }
  /// @return an iterator beyond the last catch
  iterator end() { return catches.end(); }
  /// @return an iterator to the first catch
  const_iterator begin() const { return catches.begin(); }
  /// @return an iterator beyond the last catch
  const_iterator end() const { return catches.end(); }

  /// @return a reference to the first catch
  reference front() { return catches.front(); }
  /// @return a reference to the last catch
  reference back() { return catches.back(); }
  /// @return a reference to the first catch
  const_reference front() const { return catches.front(); }
  /// @return a reference to the last catch
  const_reference back() const { return catches.back(); }

  /// Inserts a catch at the given position.
  /// @param pos the position
  /// @param v the catch
  /// @return an iterator to the newly added catch
  iterator insert(iterator pos, Catch v) { return catches.insert(pos, std::move(v)); }
  /// Inserts a catch at the given position.
  /// @param pos the position
  /// @param v the catch
  /// @return an iterator to the newly added catch
  iterator insert(const_iterator pos, Catch v) {
    return catches.insert(pos, std::move(v));
  }
  /// Appends a catch.
  /// @param v the catch
  void push_back(Catch v) { catches.push_back(std::move(v)); }

  /// Emplaces a catch.
  /// @tparam Args the catch constructor args
  template <typename... Args> void emplace_back(Args&&... args) {
    catches.emplace_back(std::forward<Args>(args)...);
  }

  /// Erases a catch at the given position.
  /// @param pos the position
  /// @return the iterator beyond the erased catch
  iterator erase(iterator pos) { return catches.erase(pos); }
  /// Erases a catch at the given position.
  /// @param pos the position
  /// @return the iterator beyond the erased catch
  iterator erase(const_iterator pos) { return catches.erase(pos); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Flow that contains an unordered list of flows. Execution starts at the first flow.
class UnorderedFlow : public AcceptorExtend<SeriesFlow, Flow> {
public:
  using iterator = std::list<FlowPtr>::iterator;
  using const_iterator = std::list<FlowPtr>::const_iterator;
  using reference = std::list<FlowPtr>::reference;
  using const_reference = std::list<FlowPtr>::const_reference;

private:
  std::list<FlowPtr> series;

public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  /// @return an iterator to the first flow
  iterator begin() { return series.begin(); }
  /// @return an iterator beyond the last flow
  iterator end() { return series.end(); }
  /// @return an iterator to the first flow
  const_iterator begin() const { return series.begin(); }
  /// @return an iterator beyond the last flow
  const_iterator end() const { return series.end(); }

  /// @return a reference to the first flow
  reference front() { return series.front(); }
  /// @return a reference to the last flow
  reference back() { return series.back(); }
  /// @return a reference to the first flow
  const_reference front() const { return series.front(); }
  /// @return a reference to the last flow
  const_reference back() const { return series.back(); }

  /// Inserts a flow at the given position.
  /// @param pos the position
  /// @param v the flow
  /// @return an iterator to the newly added
  iterator insert(iterator pos, FlowPtr v) { return series.insert(pos, std::move(v)); }
  /// Inserts a flow at the given position.
  /// @param pos the position
  /// @param v the flow
  /// @return an iterator to the newly added
  iterator insert(const_iterator pos, FlowPtr v) {
    return series.insert(pos, std::move(v));
  }
  /// Appends a flow.
  /// @param f the flow
  void push_back(FlowPtr f) { series.push_back(std::move(f)); }

  /// Erases the flow at the supplied position.
  /// @param pos the position
  /// @return the iterator beyond the removed flow
  iterator erase(iterator pos) { return series.erase(pos); }
  /// Erases the flow at the supplied position.
  /// @param pos the position
  /// @return the iterator beyond the removed flow
  iterator erase(const_iterator pos) { return series.erase(pos); }

private:
  std::ostream &doFormat(std::ostream &os) const override;

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