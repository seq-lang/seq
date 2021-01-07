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

} // namespace ir
} // namespace seq
