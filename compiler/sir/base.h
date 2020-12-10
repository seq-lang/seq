#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "util/common.h"

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"

namespace seq {
namespace ir {

extern const std::string kSrcInfoAttribute;
extern const std::string kFuncAttribute;

/// Base for SIR attributes.
struct Attribute {
  virtual ~Attribute() noexcept = default;

  friend std::ostream &operator<<(std::ostream &os, const Attribute &a) {
    return a.doFormat(os);
  }

private:
  virtual std::ostream &doFormat(std::ostream &os) const = 0;
};

using AttributePtr = std::unique_ptr<Attribute>;

/// Attribute containing SrcInfo
struct SrcInfoAttribute : public Attribute {
  /// source info
  seq::SrcInfo info;

  /// Constructs a SrcInfoAttribute.
  /// @param info the source info
  explicit SrcInfoAttribute(seq::SrcInfo info) : info(std::move(info)) {}
  SrcInfoAttribute() = default;

  std::ostream &doFormat(std::ostream &os) const override { return os << info; }
};

/// Attribute containing function information
struct FuncAttribute : public Attribute {
  /// attributes map
  std::map<std::string, std::string> attributes;

  /// Constructs a FuncAttribute.
  /// @param attributes the map of attributes
  explicit FuncAttribute(std::map<std::string, std::string> attributes)
      : attributes(std::move(attributes)) {}
  FuncAttribute() = default;

  /// @return true if the map contains val, false otherwise
  bool has(const std::string &val) const;

  std::ostream &doFormat(std::ostream &os) const override;
};

/// Base of all SIR objects.
struct AttributeHolder {
  std::string name;
  AttributeHolder *parent = nullptr;

  explicit AttributeHolder(std::string name = "") : name(std::move(name)) {}

  virtual ~AttributeHolder() noexcept = default;

  void setName(std::string n) { name = std::move(n); }
  const std::string &getName() const { return name; }
  void setParent(AttributeHolder *p) { parent = p; }

  /// Sets an attribute
  /// @param key the attribute's key
  /// @param value the attribute
  void setAttribute(const std::string &key, AttributePtr value) {
    kvStore[key] = std::move(value);
  }

  /// @return true if the key is in the store
  bool hasAttribute(const std::string &key) const {
    return kvStore.find(key) != kvStore.end();
  }

  /// Gets an attribute static casted to the desired type.
  /// @param key the key
  /// @tparam AttributeType the return type
  template <typename AttributeType = Attribute>
  AttributeType *getAttribute(const std::string &key) {
    auto it = kvStore.find(key);
    return it != kvStore.end() ? static_cast<AttributeType *>(it->second.get())
                               : nullptr;
  }

  void setSrcInfo(seq::SrcInfo s) {
    setAttribute(kSrcInfoAttribute, std::make_unique<SrcInfoAttribute>(std::move(s)));
  }

  /// @return a text representation of a reference to the object
  virtual std::string referenceString() const { return name; }

  friend std::ostream &operator<<(std::ostream &os, const AttributeHolder &a) {
    return a.doFormat(os);
  }

private:
  /// key-value attribute store
  std::map<std::string, AttributePtr> kvStore;

  virtual std::ostream &doFormat(std::ostream &os) const = 0;
};

} // namespace ir
} // namespace seq
