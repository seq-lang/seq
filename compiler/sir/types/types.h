#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sir/base.h"

namespace seq {
namespace ir {

namespace util {
class SIRVisitor;
}

namespace types {

/// Type from which other SIR types derive.
struct Type : public AttributeHolder {
  /// true if atomic
  bool atomic;

  /// Constructs a type.
  /// @param name the type's name
  /// @param atomic atomicity of the type
  explicit Type(std::string name, bool atomic = false)
      : AttributeHolder(std::move(name)), atomic(atomic) {}

  virtual ~Type() noexcept = default;
  virtual void accept(util::SIRVisitor &v);

  /// Compares types.
  /// @return true if they are equivalent
  virtual bool is(Type *other) const { return other->name == name; }

  /// Compares types.
  /// @return true if they are equivalent
  bool is(Type &other) const { return is(&other); }

  std::string referenceString() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

struct MemberedType : public Type {
  explicit MemberedType(std::string name) : Type(std::move(name)) {}

  virtual Type *getMemberType(const std::string &name) = 0;
};

/// Membered type equivalent to C structs/C++ PODs
struct RecordType : public MemberedType {
  struct Field {
    std::string name;
    Type *type;

    Field(std::string name, Type *type) : name(std::move(name)), type(type) {}

    bool is(const Field &other) const {
      return name == other.name && type->is(other.type);
    }
  };

  std::vector<Field> fields;

  /// Constructs a record type.
  /// @param name the type's name
  /// @param fieldTypes the member types
  /// @param fieldNames the member names
  RecordType(std::string name, std::vector<Type *> fieldTypes,
             std::vector<std::string> fieldNames);

  /// Constructs a record type. The field's names are "1", "2"...
  /// @param name the type's name
  /// @param mTypes a vector of member types
  RecordType(std::string name, std::vector<Type *> mTypes);

  explicit RecordType(std::string name) : MemberedType(std::move(name)) {}

  void accept(util::SIRVisitor &v) override;

  bool is(Type *other) const override {
    if (auto r = dynamic_cast<const RecordType *>(other)) {
      auto check = [](auto &x, auto &y) -> bool { return x.is(y); };
      return std::equal(fields.begin(), fields.end(), r->fields.begin(),
                        r->fields.end(), check);
    }
    return false;
  }

  Type *getMemberType(const std::string &n) override;

  /// Changes the body of the record type.
  /// @param mTypes the new body
  /// @param mNames the new names
  void realize(std::vector<Type *> mTypes, std::vector<std::string> mNames);

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Membered type that is passed by reference. Similar to Python classes.
struct RefType : public MemberedType {
private:
  /// the internal contents of the type
  std::unique_ptr<RecordType> contents;

public:
  /// Constructs a reference type.
  /// @param name the type's name
  /// @param contents the type's contents
  RefType(std::string name, std::unique_ptr<RecordType> contents)
      : MemberedType(std::move(name)), contents(std::move(contents)) {}

  void accept(util::SIRVisitor &v) override;

  bool is(Type *other) const override {
    if (auto r = dynamic_cast<const RefType *>(other)) {
      return contents->is(r->contents.get());
    }
    return false;
  }

  Type *getMemberType(const std::string &n) override {
    return contents->getMemberType(n);
  }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Type associated with a SIR function.
struct FuncType : public Type {
  /// Return type
  Type *rType;
  /// Argument types
  std::vector<Type *> argTypes;

  /// Constructs a function type.
  /// @param name the type's name
  /// @param rType the function's return type
  /// @param argTypes the function's arg types
  FuncType(std::string name, Type *rType, std::vector<Type *> argTypes)
      : Type(std::move(name)), rType(rType), argTypes(std::move(argTypes)) {}

  void accept(util::SIRVisitor &v) override;

  bool is(Type *other) const override {
    if (auto f = dynamic_cast<const FuncType *>(other)) {
      auto check = [](auto &x, auto &y) -> bool { return x->is(y); };
      return rType->is(f->rType) &&
             std::equal(argTypes.begin(), argTypes.end(), f->argTypes.begin(),
                        f->argTypes.end(), check);
    }
    return false;
  }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Type of a pointer to another SIR type
struct PointerType : public Type {
  /// type's base type
  Type *base;

  /// Constructs a pointer type.
  /// @param base the type's base
  explicit PointerType(Type *base);

  void accept(util::SIRVisitor &v) override;

  bool is(Type *other) const override {
    if (auto o = dynamic_cast<const PointerType *>(other)) {
      return base->is(o->base);
    }
    return false;
  }
};

/// Type of an optional containing another SIR type
struct OptionalType : public RecordType {
  /// type's base type
  Type *base;

  /// Constructs an optional type.
  /// @param pointerType the base's pointer type
  /// @param flagType type of the flag indicating if an object is present
  explicit OptionalType(PointerType *pointerType, Type *flagType);

  void accept(util::SIRVisitor &v) override;

  bool is(Type *other) const override {
    if (auto o = dynamic_cast<const OptionalType *>(other)) {
      return base->is(o->base);
    }
    return false;
  }
};

/// Type of an array containing another SIR type
struct ArrayType : public RecordType {
  /// type's base type
  Type *base;

  /// Constructs an array type.
  /// @param pointerType the base's pointer type
  /// @param countType the type of the array's count
  explicit ArrayType(PointerType *pointerType, Type *countType);

  void accept(util::SIRVisitor &v) override;

  bool is(Type *other) const override {
    if (auto o = dynamic_cast<const ArrayType *>(other)) {
      return base->is(o->base);
    }
    return false;
  }
};

/// Type of a generator yielding another SIR type
struct GeneratorType : public Type {
  /// type's base type
  Type *base;

  /// Constructs a generator type.
  /// @param base the type's base
  explicit GeneratorType(Type *base);

  void accept(util::SIRVisitor &v) override;

  bool is(Type *other) const override {
    if (auto o = dynamic_cast<const GeneratorType *>(other)) {
      return base->is(o->base);
    }
    return false;
  }
};

/// Type of a variably sized integer
struct IntNType : public Type {
  /// length of the integer
  unsigned len;
  /// whether the variable is signed
  bool sign;

  static const unsigned MAX_LEN = 2048;

  /// Constructs a variably sized integer type.
  /// @param len the length of the integer
  /// @param sign true if signed, false otherwise
  IntNType(unsigned len, bool sign);

  void accept(util::SIRVisitor &v) override;

  bool is(Type *other) const override {
    if (auto o = dynamic_cast<const IntNType *>(other)) {
      return len == o->len && sign == o->sign;
    }
    return false;
  }

  /// @return the name of the opposite signed corresponding type
  std::string oppositeSignName() const;
};

} // namespace types
} // namespace ir
} // namespace seq
