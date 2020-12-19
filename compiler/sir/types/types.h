#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sir/base.h"
#include "sir/util/visitor.h"

namespace seq {
namespace ir {

namespace util {
class SIRVisitor;
}

namespace types {

class Type;
using TypePtr = std::unique_ptr<Type>;

/// Type from which other SIR types derive. Generally types are immutable.
class Type : public IRNode {
private:
  /// true if atomic
  bool atomic;

public:
  /// Constructs a type.
  /// @param name the type's name
  /// @param atomic atomicity of the type
  explicit Type(std::string name, bool atomic = false)
      : IRNode(std::move(name)), atomic(atomic) {}

  virtual ~Type() noexcept = default;

  void accept(util::SIRVisitor &v) override { v.visit(this); }

  /// @return true if the type is atomic
  bool isAtomic() const { return atomic; }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// CRTP that implements visitor functions.
template <typename Derived, typename Ancestor = Type> class TypeBase : public Ancestor {
public:
  template <typename... Args>
  explicit TypeBase(Args... args) : Ancestor(std::forward<Args>(args)...) {}

  virtual ~TypeBase() = default;

  void accept(util::SIRVisitor &v) override { v.visit(static_cast<Derived *>(this)); }
};

/// Type from which membered types derive.
class MemberedType : public Type {
public:
  /// Object that represents a field in a membered type.
  struct Field {
    /// the field's name
    std::string name;
    /// the field's type
    Type *type;

    /// Constructs a field.
    /// @param name the field's name
    /// @param type the field's type
    Field(std::string name, Type *type) : name(std::move(name)), type(type) {}
  };

  using const_iterator = std::vector<Field>::const_iterator;
  using const_reference = std::vector<Field>::const_reference;

  /// Constructs a membered type.
  /// @param name the type's name
  explicit MemberedType(std::string name) : Type(std::move(name)) {}

  virtual ~MemberedType() = default;

  /// Gets a field type by name.
  /// @param name the field's name
  /// @return the type if it exists
  virtual Type *getMemberType(const std::string &name) = 0;

  /// @return iterator to the first field
  virtual const_iterator begin() const = 0;
  /// @return iterator beyond the last field
  virtual const_iterator end() const = 0;
  /// @return a reference to the first field
  virtual const_reference front() const = 0;
  /// @return a reference to the last field
  virtual const_reference back() const = 0;

  /// Changes the body of the membered type.
  /// @param mTypes the new body
  /// @param mNames the new names
  virtual void realize(std::vector<Type *> mTypes, std::vector<std::string> mNames) = 0;
};

/// Membered type equivalent to C structs/C++ PODs
class RecordType : public TypeBase<RecordType, MemberedType> {
private:
  std::vector<Field> fields;

public:
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

  /// Constructs an empty record type.
  /// @param name the name
  explicit RecordType(std::string name) : TypeBase(std::move(name)) {}

  virtual ~RecordType() = default;

  Type *getMemberType(const std::string &n) override;

  const_iterator begin() const override { return fields.begin(); }
  const_iterator end() const override { return fields.end(); }
  virtual const_reference front() const override { return fields.front(); }
  virtual const_reference back() const override { return fields.back(); }

  void realize(std::vector<Type *> mTypes, std::vector<std::string> mNames) override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Membered type that is passed by reference. Similar to Python classes.
class RefType : public TypeBase<RecordType, MemberedType> {
private:
  /// the internal contents of the type
  RecordType *contents;

public:
  /// Constructs a reference type.
  /// @param name the type's name
  /// @param contents the type's contents
  RefType(std::string name, RecordType *contents)
      : TypeBase(std::move(name)), contents(contents) {}

  Type *getMemberType(const std::string &n) override {
    return contents->getMemberType(n);
  }

  const_iterator begin() const override { return contents->begin(); }
  const_iterator end() const override { return contents->end(); }
  virtual const_reference front() const override { return contents->front(); }
  virtual const_reference back() const override { return contents->back(); }

  /// @return the reference type's contents
  RecordType *getContents() const { return contents; }

  void realize(std::vector<Type *> mTypes, std::vector<std::string> mNames) override {
    contents->realize(std::move(mTypes), std::move(mNames));
  }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Type associated with a SIR function.
class FuncType : public TypeBase<FuncType> {
public:
  using const_iterator = std::vector<Type *>::const_iterator;
  using const_reference = std::vector<Type *>::const_reference;

private:
  /// return type
  Type *rType;
  /// argument types
  std::vector<Type *> argTypes;

public:
  /// Constructs a function type.
  /// @param rType the function's return type
  /// @param argTypes the function's arg types
  FuncType(Type *rType, std::vector<Type *> argTypes)
      : TypeBase(getName(rType, argTypes)), rType(rType),
        argTypes(std::move(argTypes)) {}

  /// @return the function's return type
  Type *getReturnType() const { return rType; }

  /// @return iterator to the first argument
  const_iterator begin() const { return argTypes.begin(); }
  /// @return iterator beyond the last argument
  const_iterator end() const { return argTypes.end(); }

  /// @return a reference to the first argument
  const_reference front() const { return argTypes.front(); }
  /// @return a reference to the last argument
  const_reference back() const { return argTypes.back(); }

  static std::string getName(Type *rType, const std::vector<Type *> &argTypes);

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Base for simple derived types.
class DerivedType : public Type {
private:
  /// the base type
  Type *base;

public:
  /// Constructs a derived type.
  /// @param name the type's name
  /// @param base the type's base
  explicit DerivedType(std::string name, Type *base)
      : Type(std::move(name)), base(base) {}

  /// @return the type's base
  Type *getBase() const { return base; }
};

/// Type of a pointer to another SIR type
class PointerType : public TypeBase<PointerType, DerivedType> {
public:
  /// Constructs a pointer type.
  /// @param base the type's base
  explicit PointerType(Type *base) : TypeBase(getName(base), base) {}

  static std::string getName(Type *base);
};

/// Type of an optional containing another SIR type
class OptionalType : public TypeBase<OptionalType, RecordType> {
private:
  /// type's base type
  Type *base;

public:
  /// Constructs an optional type.
  /// @param pointerType the base's pointer type
  /// @param flagType type of the flag indicating if an object is present
  explicit OptionalType(Type *pointerType, Type *flagType);

  /// @return the type's base
  Type *getBase() const { return base; }

  void accept(util::SIRVisitor &v) override { v.visit(this); }

  static std::string getName(Type *base);
};

/// Type of an array containing another SIR type
class ArrayType : public TypeBase<ArrayType, RecordType> {
private:
  /// type's base type
  Type *base;

public:
  /// Constructs an array type.
  /// @param pointerType the base's pointer type
  /// @param countType the type of the array's count
  explicit ArrayType(Type *pointerType, Type *countType);

  /// @return the type's base
  Type *getBase() const { return base; }

  void accept(util::SIRVisitor &v) override { v.visit(this); }

  static std::string getName(Type *base);
};

/// Type of a generator yielding another SIR type
class GeneratorType : public TypeBase<GeneratorType, DerivedType> {
public:
  /// Constructs a generator type.
  /// @param base the type's base
  explicit GeneratorType(Type *base) : TypeBase(getName(base), base) {}

  static std::string getName(Type *base);
};

/// Type of a variably sized integer
class IntNType : public TypeBase<IntNType> {
private:
  /// length of the integer
  unsigned len;
  /// whether the variable is signed
  bool sign;

public:
  static const unsigned MAX_LEN = 2048;

  /// Constructs a variably sized integer type.
  /// @param len the length of the integer
  /// @param sign true if signed, false otherwise
  IntNType(unsigned len, bool sign)
      : TypeBase(getName(len, sign)), len(len), sign(sign) {}

  /// @return the length of the integer
  unsigned getLen() const { return len; }
  /// @return true if signed
  bool isSigned() const { return sign; }

  /// @return the name of the opposite signed corresponding type
  std::string oppositeSignName() const { return getName(len, !sign); }

  static std::string getName(unsigned len, bool sign);
};

} // namespace types
} // namespace ir
} // namespace seq
