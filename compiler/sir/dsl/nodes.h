#pragma once

#include <memory>

#include "sir/base.h"
#include "sir/constant.h"
#include "sir/instr.h"

#define CUSTOM_VALUE                                                                   \
  virtual std::unique_ptr<codegen::ValueBuilder> getBuilder() const = 0

namespace seq {
namespace ir {
namespace dsl {

namespace codegen {
struct TypeBuilder;
struct ValueBuilder;
} // namespace codegen

namespace types {

class CustomType : public AcceptorExtend<CustomType, ir::types::Type> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  virtual std::unique_ptr<codegen::TypeBuilder> getBuilder() const = 0;
};

} // namespace types

class CustomConstant : public AcceptorExtend<CustomConstant, Constant> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  CUSTOM_VALUE;
};

class CustomFlow : public AcceptorExtend<CustomFlow, Flow> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  CUSTOM_VALUE;
};

class CustomInstr : public AcceptorExtend<CustomInstr, Instr> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  CUSTOM_VALUE;
};

} // namespace dsl
} // namespace ir
} // namespace seq

#undef CUSTOM_VALUE
