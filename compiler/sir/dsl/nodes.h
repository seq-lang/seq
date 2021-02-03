#pragma once

#include <functional>

#include "sir/base.h"
#include "sir/constant.h"
#include "sir/instr.h"

namespace seq {
namespace ir {
namespace dsl {

namespace types {

class CustomType : public AcceptorExtend<CustomType, ir::types::Type> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;
};

} // namespace types

class CustomConstant : public AcceptorExtend<CustomConstant, Constant> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;
};

class CustomFlow : public AcceptorExtend<CustomFlow, Instr> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;
};

class CustomInstr : public AcceptorExtend<CustomInstr, Instr> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;
};

} // namespace dsl
} // namespace ir
} // namespace seq