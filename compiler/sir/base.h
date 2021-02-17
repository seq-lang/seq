#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "attribute.h"
#include "util/iterators.h"
#include "util/visitor.h"

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"

namespace seq {
namespace ir {

class Func;
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

  /// @return the node's id.
  int getId() const { return id; }
};

/// Base for named IR nodes.
class IRNode {
private:
  /// the node's name
  std::string name;
  /// key-value attribute store
  std::map<std::string, std::unique_ptr<Attribute>> attributes;
  /// the module
  IRModule *module = nullptr;
  /// a replacement, if set
  IRNode *replacement = nullptr;

public:
  // RTTI is implemented using a port of LLVM's Extensible RTTI
  // For more details, see
  // https://llvm.org/docs/HowToSetUpLLVMStyleRTTI.html#rtti-for-open-class-hierarchies
  static const char NodeId;

  /// Constructs a node.
  /// @param name the node's name
  explicit IRNode(std::string name = "") : name(std::move(name)) {}

  /// See LLVM documentation.
  static const void *nodeId() { return &NodeId; }
  /// See LLVM documentation.
  virtual bool isConvertible(const void *other) const {
    if (hasReplacement())
      return getActual()->isConvertible(other);
    return other == nodeId();
  }
  /// See LLVM documentation.
  template <typename Target> bool is() const { return isConvertible(Target::nodeId()); }
  /// See LLVM documentation.
  template <typename Target> Target *as() {
    return isConvertible(Target::nodeId()) ? static_cast<Target *>(getActual())
                                           : nullptr;
  }
  /// See LLVM documentation.
  template <typename Target> const Target *as() const {
    return isConvertible(Target::nodeId()) ? static_cast<const Target *>(getActual())
                                           : nullptr;
  }

  /// @return the node's name
  const std::string &getName() const { return getActual()->name; }
  /// Sets the node's name
  /// @param n the new name
  void setName(std::string n) { getActual()->name = std::move(n); }

  /// Accepts visitors.
  /// @param v the visitor
  virtual void accept(util::IRVisitor &v) = 0;
  /// Accepts visitors.
  /// @param v the visitor
  virtual void accept(util::ConstIRVisitor &v) const = 0;

  /// Sets an attribute
  /// @param the attribute key
  /// @param value the attribute
  void setAttribute(std::unique_ptr<Attribute> value, const std::string &key) {
    getActual()->attributes[key] = std::move(value);
  }
  /// Sets an attribute
  /// @param value the attribute
  template <typename AttributeType>
  void setAttribute(std::unique_ptr<AttributeType> value) {
    setAttribute(std::move(value), AttributeType::AttributeName);
  }

  /// @param n the name
  /// @return true if the attribute is in the store
  bool hasAttribute(const std::string &n) const {
    auto *actual = getActual();
    return actual->attributes.find(n) != actual->attributes.end();
  }
  /// @return true if the attribute is in the store
  template <typename AttributeType> bool hasAttribute() const {
    return hasAttribute(AttributeType::AttributeName);
  }

  /// Gets the appropriate attribute.
  /// @param key the attribute key
  Attribute *getAttribute(const std::string &key) {
    auto *actual = getActual();

    auto it = actual->attributes.find(key);
    return it != actual->attributes.end() ? it->second.get() : nullptr;
  }
  /// Gets the appropriate attribute.
  /// @param key the attribute key
  const Attribute *getAttribute(const std::string &key) const {
    auto *actual = getActual();

    auto it = actual->attributes.find(key);
    return it != actual->attributes.end() ? it->second.get() : nullptr;
  }
  /// Gets the appropriate attribute.
  /// @tparam AttributeType the return type
  template <typename AttributeType> AttributeType *getAttribute() {
    return static_cast<AttributeType *>(getAttribute(AttributeType::AttributeName));
  }
  /// Gets the appropriate attribute.
  /// @tparam AttributeType the return type
  template <typename AttributeType> const AttributeType *getAttribute() const {
    return static_cast<const AttributeType *>(
        getAttribute(AttributeType::AttributeName));
  }

  /// @return iterator to the first attribute
  auto attributes_begin() {
    return util::map_key_adaptor(getActual()->attributes.begin());
  }
  /// @return iterator beyond the last attribute
  auto attributes_end() { return util::map_key_adaptor(getActual()->attributes.end()); }
  /// @return iterator to the first attribute
  auto attributes_begin() const {
    return util::const_map_key_adaptor(getActual()->attributes.begin());
  }
  /// @return iterator beyond the last attribute
  auto attributes_end() const {
    return util::const_map_key_adaptor(getActual()->attributes.end());
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
  virtual std::string referenceString() const { return getActual()->name; }

  /// @return the IR module
  IRModule *getModule() const { return getActual()->module; }
  /// Sets the module.
  /// @param m the new module
  void setModule(IRModule *m) { getActual()->module = m; }

  friend std::ostream &operator<<(std::ostream &os, const IRNode &a) {
    if (a.hasReplacement())
      return os << *a.getActual();
    return a.doFormat(os);
  }

  bool hasReplacement() const { return replacement != nullptr; }

  /// @return a vector of all the node's children
  virtual std::vector<Value *> getUsedValues() { return {}; }
  /// @return a vector of all the node's children
  virtual std::vector<const Value *> getUsedValues() const { return {}; }
  /// Physically replaces all instances of a child value.
  /// @param id the id of the value to be replaced
  /// @param newValue the new value
  /// @return number of replacements
  virtual int replaceUsedValue(int id, Value *newValue) { return 0; }
  /// Physically replaces all instances of a child value.
  /// @param oldValue the old value
  /// @param newValue the new value
  /// @return number of replacements
  int replaceUsedValue(Value *old, Value *newValue);

  /// @return a vector of all the node's used types
  virtual std::vector<types::Type *> getUsedTypes() { return {}; }
  /// @return a vector of all the node's used types
  virtual std::vector<const types::Type *> getUsedTypes() const { return {}; }
  /// Physically replaces all instances of a used type.
  /// @param name the name of the type being replaced
  /// @param newType the new type
  /// @return number of replacements
  virtual int replaceUsedType(const std::string &name, types::Type *newType) {
    return 0;
  }
  /// Physically replaces all instances of a used type.
  /// @param old the old type
  /// @param newType the new type
  /// @return number of replacements
  int replaceUsedType(types::Type *old, types::Type *newType);

  /// @return a vector of all the node's used variables
  virtual std::vector<Var *> getUsedVariables() { return {}; }
  /// @return a vector of all the node's used variables
  virtual std::vector<const Var *> getUsedVariables() const { return {}; }
  /// Physically replaces all instances of a used variable.
  /// @param id the id of the variable
  /// @param newType the new type
  /// @return number of replacements
  virtual int replaceUsedVariable(int id, Var *newVar) { return 0; }
  /// Physically replaces all instances of a used variable.
  /// @param old the old variable
  /// @param newVar the new variable
  /// @return number of replacements
  int replaceUsedVariable(Var *old, Var *newVar);

  template <typename, typename> friend class AcceptorExtend;
  template <typename> friend class ReplaceableNodeBase;

private:
  IRNode *getActual() { return replacement ? replacement : this; }
  const IRNode *getActual() const { return replacement ? replacement : this; }

  virtual std::ostream &doFormat(std::ostream &os) const = 0;
};

template <typename Derived, typename Parent> class AcceptorExtend : public Parent {
public:
  using Parent::Parent;

  /// See LLVM documentation.
  static const void *nodeId() { return &Derived::NodeId; }
  /// See LLVM documentation.
  virtual bool isConvertible(const void *other) const {
    if (IRNode::hasReplacement())
      return IRNode::getActual()->isConvertible(other);

    return other == nodeId() || Parent::isConvertible(other);
  }

  void accept(util::IRVisitor &v) {
    if (IRNode::hasReplacement())
      IRNode::getActual()->accept(v);
    else
      v.visit(static_cast<Derived *>(this));
  }

  void accept(util::ConstIRVisitor &v) const {
    if (IRNode::hasReplacement())
      IRNode::getActual()->accept(v);
    else
      v.visit(static_cast<const Derived *>(this));
  }
};

template <typename Derived>
class ReplaceableNodeBase : public AcceptorExtend<Derived, IRNode> {
private:
  /// true if the node can be lazily replaced
  bool replaceable = true;

public:
  using AcceptorExtend<Derived, IRNode>::AcceptorExtend;

  static const char NodeId;

  /// @return the logical value of the node
  Derived *getActual() {
    return IRNode::replacement
               ? static_cast<Derived *>(IRNode::replacement)->getActual()
               : static_cast<Derived *>(this);
  }

  /// @return the logical value of the node
  const Derived *getActual() const {
    return IRNode::replacement
               ? static_cast<const Derived *>(IRNode::replacement)->getActual()
               : static_cast<const Derived *>(this);
  }

  /// Lazily replaces all instances of the node.
  /// @param v the new value
  void replaceAll(Derived *v) {
    assert(replaceable);
    IRNode::replacement = v;
  }

  /// @return true if the object can be replaced
  bool isReplaceable() const { return replaceable; }
  /// Sets the object's replaceable flag.
  /// @param v the new value
  void setReplaceable(bool v = true) { replaceable = v; }
};

template <typename Derived> const char ReplaceableNodeBase<Derived>::NodeId = 0;

template <typename Desired> Desired *cast(IRNode *other) {
  return other != nullptr ? other->as<Desired>() : nullptr;
}

template <typename Desired> const Desired *cast(const IRNode *other) {
  return other != nullptr ? other->as<Desired>() : nullptr;
}

template <typename Desired> bool isA(IRNode *other) {
  return other && other->is<Desired>();
}

template <typename Desired> bool isA(const IRNode *other) {
  return other && other->is<Desired>();
}

} // namespace ir
} // namespace seq

// See https://github.com/fmtlib/fmt/issues/1283.
namespace fmt {
using seq::ir::IRNode;

template <typename Char>
struct formatter<IRNode, Char> : fmt::v6::internal::fallback_formatter<IRNode, Char> {};
} // namespace fmt
