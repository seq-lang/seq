#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sir/base.h"
#include "sir/util/visitor.h"

namespace seq {
namespace ir {
namespace types {

class Type;
using TypePtr = std::unique_ptr<Type>;

/// Type from which other SIR types derive. Generally types are immutable.
class Type : public AcceptorExtend<Type, IRNode> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  virtual ~Type() noexcept = default;

  /// A type is "atomic" iff it contains no pointers to dynamically
  /// allocated memory. Atomic types do not need to be scanned during
  /// garbage collection.
  /// @return true if the type is atomic
  virtual bool isAtomic() const = 0;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Type from which primitive atomic types derive.
class PrimitiveType : public AcceptorExtend<PrimitiveType, Type> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  bool isAtomic() const override { return true; }
};

/// Int type (64-bit signed integer)
class IntType : public AcceptorExtend<IntType, PrimitiveType> {
public:
  static const char NodeId;

  /// Constructs an int type.
  IntType() : AcceptorExtend(".int") {}
};

/// Float type (64-bit double)
class FloatType : public AcceptorExtend<FloatType, PrimitiveType> {
public:
  static const char NodeId;

  /// Constructs a float type.
  FloatType() : AcceptorExtend(".float") {}
};

/// Bool type (8-bit unsigned integer; either 0 or 1)
class BoolType : public AcceptorExtend<BoolType, PrimitiveType> {
public:
  static const char NodeId;

  /// Constructs a bool type.
  BoolType() : AcceptorExtend(".bool") {}
};

/// Byte type (8-bit unsigned integer)
class ByteType : public AcceptorExtend<ByteType, PrimitiveType> {
public:
  static const char NodeId;

  /// Constructs a byte type.
  ByteType() : AcceptorExtend(".byte") {}
};

/// Void type
class VoidType : public AcceptorExtend<VoidType, PrimitiveType> {
public:
  static const char NodeId;

  /// Constructs a void type.
  VoidType() : AcceptorExtend(".void") {}
};

/// Type from which membered types derive.
class MemberedType : public AcceptorExtend<MemberedType, Type> {
public:
  static const char NodeId;

  /// Object that represents a field in a membered type.
  struct Field {
    /// the field's name
    std::string name;
    /// the field's type
    const Type *type;

    /// Constructs a field.
    /// @param name the field's name
    /// @param type the field's type
    Field(std::string name, const Type *type) : name(std::move(name)), type(type) {}
  };

  using const_iterator = std::vector<Field>::const_iterator;
  using const_reference = std::vector<Field>::const_reference;

  /// Constructs a membered type.
  /// @param name the type's name
  explicit MemberedType(std::string name) : AcceptorExtend(std::move(name)) {}

  virtual ~MemberedType() = default;

  /// Gets a field type by name.
  /// @param name the field's name
  /// @return the type if it exists
  virtual const Type *getMemberType(const std::string &name) const = 0;

  /// Gets the index of a field by name.
  /// @param name the field's name
  /// @return 0-based field index, or -1 if not found
  virtual int getMemberIndex(const std::string &name) const = 0;

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
  virtual void realize(std::vector<const Type *> mTypes,
                       std::vector<std::string> mNames) = 0;
};

/// Membered type equivalent to C structs/C++ PODs
class RecordType : public AcceptorExtend<RecordType, MemberedType> {
private:
  std::vector<Field> fields;

public:
  static const char NodeId;

  /// Constructs a record type.
  /// @param name the type's name
  /// @param fieldTypes the member types
  /// @param fieldNames the member names
  RecordType(std::string name, std::vector<const Type *> fieldTypes,
             std::vector<std::string> fieldNames);

  /// Constructs a record type. The field's names are "1", "2"...
  /// @param name the type's name
  /// @param mTypes a vector of member types
  RecordType(std::string name, std::vector<const Type *> mTypes);

  /// Constructs an empty record type.
  /// @param name the name
  explicit RecordType(std::string name) : AcceptorExtend(std::move(name)) {}

  virtual ~RecordType() = default;

  bool isAtomic() const override {
    for (const auto &field : fields) {
      if (!field.type->isAtomic())
        return false;
    }
    return true;
  }

  const Type *getMemberType(const std::string &n) const override;

  int getMemberIndex(const std::string &n) const override;

  const_iterator begin() const override { return fields.begin(); }
  const_iterator end() const override { return fields.end(); }
  const_reference front() const override { return fields.front(); }
  const_reference back() const override { return fields.back(); }

  void realize(std::vector<const Type *> mTypes,
               std::vector<std::string> mNames) override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Membered type that is passed by reference. Similar to Python classes.
class RefType : public AcceptorExtend<RefType, MemberedType> {
private:
  /// the internal contents of the type
  RecordType *contents;

public:
  static const char NodeId;

  /// Constructs a reference type.
  /// @param name the type's name
  /// @param contents the type's contents
  RefType(std::string name, RecordType *contents)
      : AcceptorExtend(std::move(name)), contents(contents) {}

  bool isAtomic() const override { return false; }

  const Type *getMemberType(const std::string &n) const override {
    return contents->getMemberType(n);
  }

  int getMemberIndex(const std::string &n) const override {
    return contents->getMemberIndex(n);
  }

  const_iterator begin() const override { return contents->begin(); }
  const_iterator end() const override { return contents->end(); }
  const_reference front() const override { return contents->front(); }
  const_reference back() const override { return contents->back(); }

  /// @return the reference type's contents
  RecordType *getContents() { return contents; }
  /// @return the reference type's contents
  const RecordType *getContents() const { return contents; }

  void realize(std::vector<const Type *> mTypes,
               std::vector<std::string> mNames) override {
    contents->realize(std::move(mTypes), std::move(mNames));
  }

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Type associated with a SIR function.
class FuncType : public AcceptorExtend<FuncType, Type> {
public:
  using const_iterator = std::vector<const Type *>::const_iterator;
  using const_reference = std::vector<const Type *>::const_reference;

private:
  /// return type
  const Type *rType;
  /// argument types
  std::vector<const Type *> argTypes;

public:
  static const char NodeId;

  /// Constructs a function type.
  /// @param rType the function's return type
  /// @param argTypes the function's arg types
  FuncType(const Type *rType, std::vector<const Type *> argTypes)
      : AcceptorExtend(getInstanceName(rType, argTypes)), rType(rType),
        argTypes(std::move(argTypes)) {}

  bool isAtomic() const override { return false; }

  /// @return the function's return type
  const Type *getReturnType() const { return rType; }

  /// @return iterator to the first argument
  const_iterator begin() const { return argTypes.begin(); }
  /// @return iterator beyond the last argument
  const_iterator end() const { return argTypes.end(); }

  /// @return a reference to the first argument
  const_reference front() const { return argTypes.front(); }
  /// @return a reference to the last argument
  const_reference back() const { return argTypes.back(); }

  static std::string getInstanceName(const Type *rType,
                                     const std::vector<const Type *> &argTypes);

private:
  std::ostream &doFormat(std::ostream &os) const override;
};

/// Base for simple derived types.
class DerivedType : public AcceptorExtend<DerivedType, Type> {
private:
  /// the base type
  const Type *base;

public:
  static const char NodeId;

  /// Constructs a derived type.
  /// @param name the type's name
  /// @param base the type's base
  explicit DerivedType(std::string name, const Type *base)
      : AcceptorExtend(std::move(name)), base(base) {}

  bool isAtomic() const override { return base->isAtomic(); }

  /// @return the type's base
  const Type *getBase() const { return base; }
};

/// Type of a pointer to another SIR type
class PointerType : public AcceptorExtend<PointerType, DerivedType> {
public:
  static const char NodeId;

  /// Constructs a pointer type.
  /// @param base the type's base
  explicit PointerType(const Type *base)
      : AcceptorExtend(getInstanceName(base), base) {}

  bool isAtomic() const override { return false; }

  static std::string getInstanceName(const Type *base);
};

/// Type of an optional containing another SIR type
class OptionalType : public AcceptorExtend<OptionalType, DerivedType> {
public:
  static const char NodeId;

  /// Constructs an optional type.
  /// @param base the type's base
  explicit OptionalType(const Type *base)
      : AcceptorExtend(getInstanceName(base), base) {}

  bool isAtomic() const override { return getBase()->isAtomic(); }

  void accept(util::SIRVisitor &v) override { v.visit(this); }

  static std::string getInstanceName(const Type *base);
};

/// Type of an array containing another SIR type
class ArrayType : public AcceptorExtend<ArrayType, RecordType> {
private:
  /// type's base type
  const Type *base;

public:
  static const char NodeId;

  /// Constructs an array type.
  /// @param pointerType the base's pointer type
  /// @param countType the type of the array's count
  explicit ArrayType(const Type *pointerType, const Type *countType);

  bool isAtomic() const override { return false; }

  /// @return the type's base
  const Type *getBase() const { return base; }

  void accept(util::SIRVisitor &v) override { v.visit(this); }

  static std::string getInstanceName(const Type *base);
};

/// Type of a generator yielding another SIR type
class GeneratorType : public AcceptorExtend<GeneratorType, DerivedType> {
public:
  static const char NodeId;

  /// Constructs a generator type.
  /// @param base the type's base
  explicit GeneratorType(const Type *base)
      : AcceptorExtend(getInstanceName(base), base) {}

  bool isAtomic() const override { return false; }

  static std::string getInstanceName(const Type *base);
};

/// Type of a variably sized integer
class IntNType : public AcceptorExtend<IntNType, Type> {
private:
  /// length of the integer
  unsigned len;
  /// whether the variable is signed
  bool sign;

public:
  static const char NodeId;

  static const unsigned MAX_LEN = 2048;

  /// Constructs a variably sized integer type.
  /// @param len the length of the integer
  /// @param sign true if signed, false otherwise
  IntNType(unsigned len, bool sign)
      : AcceptorExtend(getInstanceName(len, sign)), len(len), sign(sign) {}

  bool isAtomic() const override { return true; }

  /// @return the length of the integer
  unsigned getLen() const { return len; }
  /// @return true if signed
  bool isSigned() const { return sign; }

  /// @return the name of the opposite signed corresponding type
  std::string oppositeSignName() const { return getInstanceName(len, !sign); }

  static std::string getInstanceName(unsigned len, bool sign);
};

} // namespace types
} // namespace ir
} // namespace seq
