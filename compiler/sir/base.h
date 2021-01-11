#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"

#include "attribute.h"
#include "util/visitor.h"

namespace seq {
namespace ir {

class IRModule;

/// Mixin class for IR nodes that need ids.
class IdMixin {
private:
  /// the global id counter
  static int currentId;
  /// the instance's id
  int id;

public:
  /// Resets the global id counter.
  static void resetId();

  IdMixin() : id(currentId++) {}
  virtual ~IdMixin() = default;

  /// @return the node's id.
  int getId() const { return id; }
};

/// Base for named IR nodes.
class IRNode {
private:
  /// the node's name
  std::string name;
  /// key-value attribute store
  std::map<std::string, AttributePtr> kvStore;
  /// the module
  IRModule *module = nullptr;

public:
  // RTTI is implemented using a port of LLVM's Extensible RTTI
  // For more details, see
  // https://llvm.org/docs/HowToSetUpLLVMStyleRTTI.html#rtti-for-open-class-hierarchies
  static const char NodeId;

  /// Constructs a node.
  /// @param name the node's name
  explicit IRNode(std::string name = "") : name(std::move(name)) {}

  virtual ~IRNode() = default;

  /// See LLVM documentation.
  static const void *nodeId() { return &NodeId; }
  /// See LLVM documentation.
  virtual bool isConvertible(const void *other) const { return other == nodeId(); }
  /// See LLVM documentation.
  template <typename Target> bool is() const { return isConvertible(Target::nodeId()); }
  /// See LLVM documentation.
  template <typename Target> Target *as() {
    return isConvertible(Target::nodeId()) ? static_cast<Target *>(this) : nullptr;
  }
  /// See LLVM documentation.
  template <typename Target> const Target *as() const {
    return isConvertible(Target::nodeId()) ? static_cast<const Target *>(this)
                                           : nullptr;
  }

  /// @return the node's name
  const std::string &getName() const { return name; }
  /// Sets the node's name
  /// @param n the new name
  void setName(std::string n) { name = std::move(n); }

  /// Accepts visitors.
  /// @param v the visitor
  virtual void accept(util::SIRVisitor &v) = 0;
  /// Accepts visitors.
  /// @param v the visitor
  virtual void accept(util::SIRVisitor &v) const = 0;

  /// Sets an attribute
  /// @param value the attribute
  template <typename AttributeType>
  void setAttribute(std::unique_ptr<AttributeType> value) {
    kvStore[AttributeType::AttributeName] = std::move(value);
  }

  /// @return true if the attribute is in the store
  template <typename AttributeType> bool hasAttribute() const {
    return hasAttribute(AttributeType::AttributeName);
  }

  /// @param n the name
  /// @return true if the attribute is in the store
  bool hasAttribute(const std::string &n) { return kvStore.find(n) != kvStore.end(); }

  /// Gets the appropriate attribute.
  /// @tparam AttributeType the return type
  template <typename AttributeType> AttributeType *getAttribute() const {
    auto it = kvStore.find(AttributeType::AttributeName);
    return it != kvStore.end() ? static_cast<AttributeType *>(it->second.get())
                               : nullptr;
  }

  /// Helper to add source information.
  /// @param the source information
  void setSrcInfo(seq::SrcInfo s) {
    setAttribute(std::make_unique<SrcInfoAttribute>(std::move(s)));
  }
  /// @return the src info
  seq::SrcInfo getSrcInfo() const {
    return getAttribute<SrcInfoAttribute>() ? getAttribute<SrcInfoAttribute>()->info
                                            : seq::SrcInfo();
  }

  /// @return a text representation of a reference to the object
  virtual std::string referenceString() const { return name; }

  friend std::ostream &operator<<(std::ostream &os, const IRNode &a) {
    return a.doFormat(os);
  }

  /// @return the IR module
  IRModule *getModule() const { return module; }
  /// Sets the module.
  /// @param m the new module
  void setModule(IRModule *m) { module = m; }

private:
  virtual std::ostream &doFormat(std::ostream &os) const = 0;
};

template <typename Derived, typename Parent> class AcceptorExtend : public Parent {
public:
  using Parent::Parent;

  virtual ~AcceptorExtend() = default;

  /// See LLVM documentation.
  static const void *nodeId() { return &Derived::NodeId; }
  /// See LLVM documentation.
  virtual bool isConvertible(const void *other) const {
    return other == nodeId() || Parent::isConvertible(other);
  }

  void accept(util::SIRVisitor &v) { v.visit(static_cast<Derived *>(this)); }
  void accept(util::SIRVisitor &v) const {
    v.visit(static_cast<const Derived *>(this));
  }
};

template <typename Desired> Desired *cast(IRNode *other) {
  return other != nullptr ? other->as<Desired>() : nullptr;
}

template <typename Desired> const Desired *cast(const IRNode *other) {
  return other != nullptr ? other->as<Desired>() : nullptr;
}

template <typename Desired> bool isA(IRNode *other) {
  return other && other->is<Desired>();
}

} // namespace ir
} // namespace seq

namespace fmt {
using seq::ir::IRNode;

template <typename Char>
struct formatter<IRNode, Char> : fmt::v6::internal::fallback_formatter<IRNode, Char> {};

} // namespace fmt
