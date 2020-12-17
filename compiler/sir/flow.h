#pragma once

#include <list>
#include <vector>

#include "base.h"
#include "value.h"

namespace seq {
namespace ir {

/// Base fors, which represent control.
class Flow : public Value {
public:
  /// Constructs a flow.
  /// @param name the name
  explicit Flow(std::string name = "") : Value(std::move(name)) {}

  types::Type *getType() const override { return nullptr; }

  virtual ~Flow() noexcept = default;
};

/// CRTP base fors that provides visitor functionality.
template <typename FlowType> class FlowBase : public Flow {
public:
  /// Constructs a flow.
  /// @param name the name
  explicit FlowBase(std::string name = "") : Flow(std::move(name)) {}

  void accept(util::SIRVisitor &v) override { v.visit(static_cast<FlowType *>(this)); }
};

/// Flow that contains a series of flows or instructions.
class SeriesFlow : public FlowBase<SeriesFlow> {
public:
  using iterator = std::list<ValuePtr>::iterator;
  using const_iterator = std::list<ValuePtr>::const_iterator;
  using reference = std::list<ValuePtr>::reference;
  using const_reference = std::list<ValuePtr>::const_reference;

private:
  std::list<ValuePtr> series;

public:
  /// Constructs a series flow.
  /// @param name the flow's name
  explicit SeriesFlow(std::string name = "") : FlowBase(std::move(name)) {}

  /// @return an iterator to the first
  iterator begin() { return series.begin(); }
  /// @return an iterator beyond the last
  iterator end() { return series.end(); }
  /// @return an iterator to the first
  const_iterator begin() const { return series.begin(); }
  /// @return an iterator beyond the last
  const_iterator end() const { return series.end(); }

  /// @return a reference to the first
  reference front() { return series.front(); }
  /// @return a reference to the last
  reference back() { return series.back(); }
  /// @return a reference to the first
  const_reference front() const { return series.front(); }
  /// @return a reference to the last
  const_reference back() const { return series.back(); }

  /// Inserts a at the given position.
  /// @param pos the position
  /// @param v the or instruction
  /// @return an iterator to the newly added
  iterator insert(iterator pos, ValuePtr v) { return series.insert(pos, std::move(v)); }
  /// Inserts a at the given position.
  /// @param pos the position
  /// @param v the or instruction
  /// @return an iterator to the newly added
  iterator insert(const_iterator pos, ValuePtr v) {
    return series.insert(pos, std::move(v));
  }
  /// Appends a.
  /// @param f the or instruction
  void push_back(ValuePtr f) { series.push_back(std::move(f)); }

  /// Erases the at the supplied position.
  /// @param pos the position
  /// @return the iterator beyond the removed or instruction
  iterator erase(iterator pos) { return series.erase(pos); }
  /// Erases the at the supplied position.
  /// @param pos the position
  /// @return the iterator beyond the removed or instruction
  iterator erase(const_iterator pos) { return series.erase(pos); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Flow representing a while loop.
class WhileFlow : public FlowBase<WhileFlow> {
private:
  /// the condition
  ValuePtr cond;
  /// the body
  ValuePtr body;

public:
  /// Constructs a while loop.
  /// @param cond the condition
  /// @param body the body
  /// @param name the flow's name
  WhileFlow(ValuePtr cond, ValuePtr body, std::string name = "")
      : FlowBase(std::move(name)), cond(std::move(cond)), body(std::move(body)) {}

  /// @return the condition
  const ValuePtr &getCond() const { return cond; }
  /// Sets the condition.
  /// @param c the new condition
  void setCond(ValuePtr c) { cond = std::move(c); }

  /// @return the body
  const ValuePtr &getBody() const { return body; }
  /// Sets the body.
  /// @param f the new value
  void setBody(ValuePtr f) { body = std::move(f); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Flow representing a for loop.
class ForFlow : public FlowBase<ForFlow> {
private:
  /// the setup
  ValuePtr setup;
  /// the condition
  ValuePtr cond;
  /// the body
  ValuePtr body;
  /// the update
  ValuePtr update;

public:
  /// Constructs a for loop.
  /// @param name the flow's name
  /// @param setup the setup
  /// @param cond the condition
  /// @param body the body
  /// @param update the update
  ForFlow(ValuePtr setup, ValuePtr cond, ValuePtr body, ValuePtr update,
          std::string name = "")
      : FlowBase(std::move(name)), setup(std::move(setup)), cond(std::move(cond)),
        body(std::move(body)), update(std::move(update)) {}

  /// @return the setup
  const ValuePtr &getSetup() const { return setup; }
  /// Sets the setup.
  /// @param f the new setup
  void setSetup(ValuePtr f) { setup = std::move(f); }

  /// @return the body
  const ValuePtr &getBody() const { return body; }
  /// Sets the body.
  /// @param f the new body
  void setBody(ValuePtr f) { body = std::move(f); }

  /// @return the update
  const ValuePtr &getUpdate() const { return update; }
  /// Sets the update.
  /// @param f the new update
  void setUpdate(ValuePtr f) { update = std::move(f); }

  /// @return the condition
  const ValuePtr &getCond() const { return cond; }
  /// Sets the condition.
  /// @param c the new condition
  void setCond(ValuePtr c) { cond = std::move(c); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Flow representing an if statement.
class IfFlow : public FlowBase<IfFlow> {
private:
  /// the condition
  ValuePtr cond;
  /// the true
  ValuePtr trueBranch;
  /// the false
  ValuePtr falseBranch;

public:
  /// Constructs an if.
  /// @param cond the condition
  /// @param trueBranch the true branch
  /// @param falseBranch the false branch
  /// @param name the flow's name
  IfFlow(ValuePtr cond, ValuePtr trueBranch, ValuePtr falseBranch,
         std::string name = "")
      : FlowBase(std::move(name)), cond(std::move(cond)),
        trueBranch(std::move(trueBranch)), falseBranch(std::move(falseBranch)) {}

  /// @return the true branch
  const ValuePtr &getTrueBranch() const { return trueBranch; }
  /// Sets the true branch.
  /// @param f the new true branch
  void setTrueBranch(ValuePtr f) { trueBranch = std::move(f); }

  /// @return the false branch
  const ValuePtr &getFalseBranch() const { return falseBranch; }
  /// Sets the false.
  /// @param f the new false
  void setFalseBranch(ValuePtr f) { falseBranch = std::move(f); }

  /// @return the condition
  const ValuePtr &getCond() const { return cond; }
  /// Sets the condition.
  /// @param c the new condition
  void setCond(ValuePtr c) { cond = std::move(c); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Flow representing a try-catch statement.
class TryCatchFlow : public FlowBase<TryCatchFlow> {
public:
  /// Struct representing a catch clause.
  struct Catch {
    /// the handler
    ValuePtr handler;
    /// the catch type, may be nullptr
    types::Type *type;
    /// the catch variable, may be nullptr
    ValuePtr catchVar;

    explicit Catch(ValuePtr handler, types::Type *type = nullptr,
                   ValuePtr catchVar = nullptr)
        : handler(std::move(handler)), type(type), catchVar(std::move(catchVar)) {}
  };

  using iterator = std::list<Catch>::iterator;
  using const_iterator = std::list<Catch>::const_iterator;
  using reference = std::list<Catch>::reference;
  using const_reference = std::list<Catch>::const_reference;

private:
  /// the catch clauses
  std::list<Catch> catches;

  /// the body
  ValuePtr body;
  /// the finally, may be nullptr
  ValuePtr finally;

public:
  /// Constructs an try-catch.
  /// @param name the's name
  /// @param body the body
  /// @param finally the finally
  explicit TryCatchFlow(ValuePtr body, ValuePtr finally = nullptr,
                        std::string name = "")
      : FlowBase(std::move(name)), body(std::move(body)), finally(std::move(finally)) {}

  /// @return the body
  const ValuePtr &getBody() const { return body; }
  /// Sets the body.
  /// @param f the new
  void setBody(ValuePtr f) { body = std::move(f); }

  /// @return the finally
  const ValuePtr &getFinally() const { return finally; }
  /// Sets the finally.
  /// @param f the new
  void setFinally(ValuePtr f) { finally = std::move(f); }

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
  template <typename... Args> void emplace_back(Args... args) {
    catches.emplace_back(args...);
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
};

} // namespace ir
} // namespace seq

// See https://github.com/fmtlib/fmt/issues/1283.
namespace fmt {
using seq::ir::Flow;

template <typename Char>
struct formatter<Flow, Char> : fmt::v6::internal::fallback_formatter<Flow, Char> {};
} // namespace fmt