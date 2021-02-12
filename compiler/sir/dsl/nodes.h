#pragma once

#include <memory>

#include "sir/base.h"
#include "sir/constant.h"
#include "sir/instr.h"

namespace seq {
namespace ir {
namespace dsl {

namespace codegen {
struct TypeBuilder;
struct ValueBuilder;
} // namespace codegen

namespace types {

/// DSL type.
class CustomType : public AcceptorExtend<CustomType, ir::types::Type> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  /// @return the type builder
  virtual std::unique_ptr<codegen::TypeBuilder> getBuilder() const = 0;

  /// Compares DSL nodes.
  /// @param v the other node
  /// @return true if equal
  virtual bool equals(const Type *v) const = 0;
};

} // namespace types

/// DSL constant.
class CustomConstant : public AcceptorExtend<CustomConstant, Constant> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  /// @return the value builder
  virtual std::unique_ptr<codegen::ValueBuilder> getBuilder() const = 0;
  /// Compares DSL nodes.
  /// @param v the other node
  /// @return true if equal
  virtual bool equals(const Value *v) const = 0;
};

/// DSL flow.
class CustomFlow : public AcceptorExtend<CustomFlow, Flow> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  /// @return the value builder
  virtual std::unique_ptr<codegen::ValueBuilder> getBuilder() const = 0;
  /// Compares DSL nodes.
  /// @param v the other node
  /// @return true if equal
  virtual bool equals(const Value *v) const = 0;
};

/// DSL instruction.
class CustomInstr : public AcceptorExtend<CustomInstr, Instr> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  /// @return the value builder
  virtual std::unique_ptr<codegen::ValueBuilder> getBuilder() const = 0;
  /// Compares DSL nodes.
  /// @param v the other node
  /// @return true if equal
  virtual bool equals(const Value *v) const = 0;
};

} // namespace dsl
} // namespace ir
} // namespace seq
