#pragma once

#include <vector>

#include "sir/util/named_list.h"

#include "base.h"
#include "instr.h"
#include "operand.h"

namespace seq {
namespace ir {

namespace util {
class SIRVisitor;
}

struct Var;

struct Flow : public AttributeHolder {
  explicit Flow(std::string name) : AttributeHolder(std::move(name)) {}

  virtual ~Flow() noexcept = default;

  virtual void accept(util::SIRVisitor &v);

  virtual std::vector<Flow *> getChildren() const { return {}; }

  std::string referenceString() const override;
};

using FlowPtr = std::unique_ptr<Flow>;

struct BlockFlow : public Flow {
  using InstrList = util::NamedList<Instr>;
  InstrList instructions;

  explicit BlockFlow(std::string name) : Flow(std::move(name)), instructions(this) {}

  void accept(util::SIRVisitor &v) override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

struct SeriesFlow : public Flow {
  using FlowList = util::NamedList<Flow>;
  FlowList series;

  explicit SeriesFlow(std::string name) : Flow(std::move(name)), series(this) {}

  void accept(util::SIRVisitor &v) override;

  std::vector<Flow *> getChildren() const override { return series.dump(); }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

struct WhileFlow : Flow {
  FlowPtr check;
  FlowPtr body;

  OperandPtr cond;

  WhileFlow(std::string name, OperandPtr cond, FlowPtr check, FlowPtr body);

  void accept(util::SIRVisitor &v) override;

  std::vector<Flow *> getChildren() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

struct ForFlow : Flow {
  FlowPtr setup;
  FlowPtr check;
  FlowPtr body;
  FlowPtr update;

  OperandPtr cond;

  ForFlow(std::string name, FlowPtr setup, OperandPtr cond, FlowPtr check, FlowPtr body,
          FlowPtr update);

  void accept(util::SIRVisitor &v) override;

  std::vector<Flow *> getChildren() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

struct IfFlow : Flow {
  FlowPtr check;
  FlowPtr trueBranch;
  FlowPtr falseBranch;

  OperandPtr cond;

  IfFlow(std::string name, OperandPtr cond, FlowPtr check, FlowPtr trueBranch,
         FlowPtr falseBranch);

  void accept(util::SIRVisitor &v) override;

  std::vector<Flow *> getChildren() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

struct TryCatchFlow : Flow {
  struct Catch {
    FlowPtr handler;
    types::Type *type;
    Var *catchVar;

    explicit Catch(FlowPtr handler, types::Type *type = nullptr,
                   Var *catchVar = nullptr);

    void setName(std::string n) { handler->setName(std::move(n)); }
    const std::string &getName() const { return handler->getName(); }
    void setParent(AttributeHolder *p) { handler->setParent(p); }
  };

  using CatchList = util::NamedList<Catch>;
  CatchList catches;

  FlowPtr body;
  FlowPtr finally;

  explicit TryCatchFlow(std::string name, FlowPtr body, FlowPtr finally = nullptr)
      : Flow(std::move(name)), catches(this), body(std::move(body)),
        finally(std::move(finally)) {}

  void accept(util::SIRVisitor &v) override;

  std::vector<Flow *> getChildren() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

} // namespace ir
} // namespace seq