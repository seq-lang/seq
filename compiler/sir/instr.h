#pragma once

#include <memory>
#include <string>

#include "base.h"
#include "lvalue.h"
#include "rvalue.h"

namespace seq {
namespace ir {

namespace util {
class SIRVisitor;
}
struct Flow;

/// SIR object representing an "instruction," or discrete operation in the context of a
/// basic block.
struct Instr : public AttributeHolder {
public:
  virtual ~Instr() = default;

  virtual void accept(util::SIRVisitor &v);

  std::string referenceString() const override;
};

using InsrPtr = std::unique_ptr<Instr>;

/// SIR instruction representing setting a memory location.
struct AssignInstr : public Instr {
  /// the left-hand side
  LvaluePtr left;
  /// the right-hand side
  RvaluePtr right;

  /// Constructs an assign instruction.
  /// @param left the left-hand side
  /// @param right the right-hand side
  explicit AssignInstr(LvaluePtr left, RvaluePtr right);

  void accept(util::SIRVisitor &v) override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// SIR instruction representing an operation where the rvalue is discarded.
struct RvalueInstr : public Instr {
  /// the rvalue
  RvaluePtr rvalue;

  /// Constructs an rvalue instruction.
  /// @param rvalue the rvalue
  explicit RvalueInstr(RvaluePtr rvalue);

  void accept(util::SIRVisitor &v) override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

struct BreakInstr : public Instr {
  Flow *target;

  explicit BreakInstr(Flow *target) : target(target) {}

  void accept(util::SIRVisitor &v) override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

struct ContinueInstr : public Instr {
  Flow *target;

  explicit ContinueInstr(Flow *target) : target(target) {}

  void accept(util::SIRVisitor &v) override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

struct ReturnInstr : public Instr {
  RvaluePtr value;

  explicit ReturnInstr(RvaluePtr value = nullptr);

  void accept(util::SIRVisitor &v) override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

struct YieldInstr : public Instr {
  RvaluePtr value;

  explicit YieldInstr(RvaluePtr value = nullptr);

  void accept(util::SIRVisitor &v) override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

struct ThrowInstr : public Instr {
  RvaluePtr value;

  explicit ThrowInstr(RvaluePtr value);

  void accept(util::SIRVisitor &v) override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

struct AssertInstr : public Instr {
  RvaluePtr value;
  std::string msg;

  explicit AssertInstr(RvaluePtr value, std::string msg = "");

  void accept(util::SIRVisitor &v) override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

} // namespace ir
} // namespace seq
