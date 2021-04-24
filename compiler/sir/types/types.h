#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "parser/ast.h"

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"

#include "sir/base.h"
#include "sir/util/packs.h"
#include "sir/util/visitor.h"

namespace seq {
namespace ir {

class Value;

namespace types {

class Type;

class Generic {
private:
  union {
    int64_t staticValue;
    types::Type *typeValue;
  } value;
  enum { STATIC, TYPE } tag;

public:
  Generic(int64_t staticValue) : value(), tag(STATIC) {
    value.staticValue = staticValue;
  }
  Generic(types::Type *typeValue) : value(), tag(TYPE) { value.typeValue = typeValue; }
  Generic(const types::Generic &) = default;

  /// @return true if the generic is a type
  bool isType() const { return tag == TYPE; }
  /// @return true if the generic is static
  bool isStatic() const { return tag == STATIC; }

  /// @return the static value
  int64_t getStaticValue() const { return value.staticValue; }
  /// @return the type value
  types::Type *getTypeValue() const { return value.typeValue; }
};

/// Type from which other SIR types derive. Generally types are immutable.
class Type : public ReplaceableNodeBase<Type> {
private:
  ast::types::TypePtr astType;

public:
  static const char NodeId;

  using ReplaceableNodeBase::ReplaceableNodeBase;

  virtual ~Type() noexcept = default;

  std::vector<Type *> getUsedTypes() const final {
    return getActual()->doGetUsedTypes();
  }
  int replaceUsedType(const std::string &name, Type *newType) final {
    seqassert(false, "types not replaceable");
    return -1;
  }
  using Node::replaceUsedType;

  /// @param other another type
  /// @return true if this type is equal to the argument type
  bool is(types::Type *other) const { return getName() == other->getName(); }

  /// A type is "atomic" iff it contains no pointers to dynamically
  /// allocated memory. Atomic types do not need to be scanned during
  /// garbage collection.
  /// @return true if the type is atomic
  bool isAtomic() const { return getActual()->doIsAtomic(); }

  /// @return the ast type
  ast::types::TypePtr getAstType() const { return getActual()->astType; }
  /// Sets the ast type. Should not generally be used.
  /// @param t the new type
  void setAstType(ast::types::TypePtr t) { getActual()->astType = std::move(t); }

  /// @return the generics used in the type
  std::vector<Generic> getGenerics() const { return getActual()->doGetGenerics(); }

  /// Constructs an instance of the type given the supplied args.
  /// @param args the arguments
  /// @return the new value
  Value *construct(std::vector<Value *> args) {
    return getActual()->doConstruct(std::move(args));
  }
  template <typename... Args> Value *operator()(Args &&... args) {
    std::vector<Value *> dst;
    util::stripPack(dst, std::forward<Args>(args)...);
    return construct(dst);
  }

private:
  virtual std::vector<Generic> doGetGenerics() const;

  virtual std::vector<Type *> doGetUsedTypes() const { return {}; }
  virtual bool doIsAtomic() const = 0;

  virtual Value *doConstruct(std::vector<Value *> args);
};

/// Type from which primitive atomic types derive.
class PrimitiveType : public AcceptorExtend<PrimitiveType, Type> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

private:
  bool doIsAtomic() const final { return true; }
};

/// Int type (64-bit signed integer)
class IntType : public AcceptorExtend<IntType, PrimitiveType> {
public:
  static const char NodeId;

  /// Constructs an int type.
  IntType() : AcceptorExtend("int") {}
};

/// Float type (64-bit double)
class FloatType : public AcceptorExtend<FloatType, PrimitiveType> {
public:
  static const char NodeId;

  /// Constructs a float type.
  FloatType() : AcceptorExtend("float") {}
};

/// Bool type (8-bit unsigned integer; either 0 or 1)
class BoolType : public AcceptorExtend<BoolType, PrimitiveType> {
public:
  static const char NodeId;

  /// Constructs a bool type.
  BoolType() : AcceptorExtend("bool") {}
};

/// Byte type (8-bit unsigned integer)
class ByteType : public AcceptorExtend<ByteType, PrimitiveType> {
public:
  static const char NodeId;

  /// Constructs a byte type.
  ByteType() : AcceptorExtend("byte") {}
};

/// Void type
class VoidType : public AcceptorExtend<VoidType, PrimitiveType> {
public:
  static const char NodeId;

  /// Constructs a void type.
  VoidType() : AcceptorExtend("void") {}
};

/// Type from which membered types derive.
class MemberedType : public AcceptorExtend<MemberedType, Type> {
public:
  static const char NodeId;

  /// Object that represents a field in a membered type.
  class Field {
  private:
    /// the field's name
    std::string name;
    /// the field's type
    Type *type;

  public:
    /// Constructs a field.
    /// @param name the field's name
    /// @param type the field's type
    Field(std::string name, Type *type) : name(std::move(name)), type(type) {}

    /// @return the field's name
    const std::string &getName() const { return name; }
    /// @return the field type
    Type *getType() const { return type; }
  };

  using const_iterator = std::vector<Field>::const_iterator;
  using const_reference = std::vector<Field>::const_reference;

  /// Constructs a membered type.
  /// @param name the type's name
  explicit MemberedType(std::string name) : AcceptorExtend(std::move(name)) {}

  /// Gets a field type by name.
  /// @param name the field's name
  /// @return the type if it exists
  virtual Type *getMemberType(const std::string &name) const = 0;
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
  virtual void realize(std::vector<Type *> mTypes, std::vector<std::string> mNames) = 0;
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
  RecordType(std::string name, std::vector<Type *> fieldTypes,
             std::vector<std::string> fieldNames);
  /// Constructs a record type. The field's names are "1", "2"...
  /// @param name the type's name
  /// @param mTypes a vector of member types
  RecordType(std::string name, std::vector<Type *> mTypes);
  /// Constructs an empty record type.
  /// @param name the name
  explicit RecordType(std::string name) : AcceptorExtend(std::move(name)) {}

  Type *getMemberType(const std::string &n) const override;
  int getMemberIndex(const std::string &n) const override;

  const_iterator begin() const override { return fields.begin(); }
  const_iterator end() const override { return fields.end(); }
  const_reference front() const override { return fields.front(); }
  const_reference back() const override { return fields.back(); }

  void realize(std::vector<Type *> mTypes, std::vector<std::string> mNames) override;

private:
  std::vector<Type *> doGetUsedTypes() const override;

  bool doIsAtomic() const override {
    return !std::any_of(fields.begin(), fields.end(),
                        [](auto &field) { return !field.getType()->isAtomic(); });
  }
};

/// Membered type that is passed by reference. Similar to Python classes.
class RefType : public AcceptorExtend<RefType, MemberedType> {
private:
  /// the internal contents of the type
  Type *contents;

public:
  static const char NodeId;

  /// Constructs a reference type.
  /// @param name the type's name
  /// @param contents the type's contents
  RefType(std::string name, RecordType *contents)
      : AcceptorExtend(std::move(name)), contents(contents) {}

  Type *getMemberType(const std::string &n) const override {
    return getContents()->getMemberType(n);
  }
  int getMemberIndex(const std::string &n) const override {
    return getContents()->getMemberIndex(n);
  }

  const_iterator begin() const override { return getContents()->begin(); }
  const_iterator end() const override { return getContents()->end(); }
  const_reference front() const override { return getContents()->front(); }
  const_reference back() const override { return getContents()->back(); }

  /// @return the reference type's contents
  RecordType *getContents() const { return cast<RecordType>(contents); }
  /// Sets the reference type's contents. Should not generally be used.
  /// @param t the new contents
  void setContents(RecordType *t) { contents = t; }

  void realize(std::vector<Type *> mTypes, std::vector<std::string> mNames) override {
    getContents()->realize(std::move(mTypes), std::move(mNames));
  }

private:
  std::vector<Type *> doGetUsedTypes() const override { return {contents}; }

  bool doIsAtomic() const override { return false; }

  Value *doConstruct(std::vector<Value *> args) override;
};

/// Type associated with a SIR function.
class FuncType : public AcceptorExtend<FuncType, Type> {
public:
  using const_iterator = std::vector<Type *>::const_iterator;
  using const_reference = std::vector<Type *>::const_reference;

private:
  /// return type
  Type *rType;
  /// argument types
  std::vector<Type *> argTypes;

public:
  static const char NodeId;

  /// Constructs a function type.
  /// @param rType the function's return type
  /// @param argTypes the function's arg types
  FuncType(std::string name, Type *rType, std::vector<Type *> argTypes)
      : AcceptorExtend(std::move(name)), rType(rType), argTypes(std::move(argTypes)) {}

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

private:
  std::vector<Generic> doGetGenerics() const override;

  std::vector<Type *> doGetUsedTypes() const override;

  bool doIsAtomic() const override { return false; }
};

/// Base for simple derived types.
class DerivedType : public AcceptorExtend<DerivedType, Type> {
private:
  /// the base type
  Type *base;

public:
  static const char NodeId;

  /// Constructs a derived type.
  /// @param name the type's name
  /// @param base the type's base
  explicit DerivedType(std::string name, Type *base)
      : AcceptorExtend(std::move(name)), base(base) {}

  /// @return the type's base
  Type *getBase() const { return base; }

private:
  bool doIsAtomic() const override { return base->isAtomic(); }

  std::vector<Type *> doGetUsedTypes() const override { return {base}; }
};

/// Type of a pointer to another SIR type
class PointerType : public AcceptorExtend<PointerType, DerivedType> {
public:
  static const char NodeId;

  /// Constructs a pointer type.
  /// @param base the type's base
  explicit PointerType(Type *base) : AcceptorExtend(getInstanceName(base), base) {}

  static std::string getInstanceName(Type *base);

private:
  bool doIsAtomic() const override { return false; }
};

/// Type of an optional containing another SIR type
class OptionalType : public AcceptorExtend<OptionalType, DerivedType> {
public:
  static const char NodeId;

  /// Constructs an optional type.
  /// @param base the type's base
  explicit OptionalType(Type *base) : AcceptorExtend(getInstanceName(base), base) {}

  static std::string getInstanceName(Type *base);

private:
  bool doIsAtomic() const override { return getBase()->isAtomic(); }
};

/// Type of a generator yielding another SIR type
class GeneratorType : public AcceptorExtend<GeneratorType, DerivedType> {
public:
  static const char NodeId;

  /// Constructs a generator type.
  /// @param base the type's base
  explicit GeneratorType(Type *base) : AcceptorExtend(getInstanceName(base), base) {}

  static std::string getInstanceName(Type *base);

private:
  bool doIsAtomic() const override { return false; }
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

  /// @return the length of the integer
  unsigned getLen() const { return len; }
  /// @return true if signed
  bool isSigned() const { return sign; }

  /// @return the name of the opposite signed corresponding type
  std::string oppositeSignName() const { return getInstanceName(len, !sign); }

  static std::string getInstanceName(unsigned len, bool sign);

private:
  bool doIsAtomic() const override { return true; }
};

} // namespace types
} // namespace ir
} // namespace seq
