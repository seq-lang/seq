#pragma once

#include "sir/sir.h"

namespace seq {
namespace ir {
namespace util {

/// Base class for IR nodes that match anything.
class Any {};

/// Any type.
class AnyType : public AcceptorExtend<AnyType, types::Type>, public Any {
public:
  static const char NodeId;
  using AcceptorExtend::AcceptorExtend;

private:
  std::ostream &doFormat(std::ostream &os) const override { return os << "any"; }
  bool doIsAtomic() const override { return true; }
};

/// Any value.
class AnyValue : public AcceptorExtend<AnyValue, Value>, public Any {
public:
  static const char NodeId;
  using AcceptorExtend::AcceptorExtend;

private:
  std::ostream &doFormat(std::ostream &os) const override { return os << "any"; }
  types::Type *doGetType() const override { return getModule()->getVoidType(); }
};

/// Any flow.
class AnyFlow : public AcceptorExtend<AnyFlow, Flow>, public Any {
public:
  static const char NodeId;
  using AcceptorExtend::AcceptorExtend;

private:
  std::ostream &doFormat(std::ostream &os) const override { return os << "any"; }
};

/// Any variable.
class AnyVar : public AcceptorExtend<AnyVar, Var>, public Any {
public:
  static const char NodeId;
  using AcceptorExtend::AcceptorExtend;

private:
  std::ostream &doFormat(std::ostream &os) const override { return os << "any"; }
};

/// Any function.
class AnyFunc : public AcceptorExtend<AnyFunc, Func>, public Any {
public:
  static const char NodeId;
  using AcceptorExtend::AcceptorExtend;

private:
  std::ostream &doFormat(std::ostream &os) const override { return os << "any"; }
  std::string getUnmangledName() const override { return "any"; }
};

/// Checks if IR nodes match.
/// @param a the first IR node
/// @param b the second IR node
/// @param checkNames whether or not to check the node names
/// @return true if the nodes are equal
bool match(Node *a, Node *b, bool checkNames = false);

} // namespace util
} // namespace ir
} // namespace seq
