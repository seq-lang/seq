#pragma once

#include "util/llvm.h"
#include <functional>
#include <map>
#include <string>
#include <utility>

namespace seq {
class BaseFunc;
class Func;
class Generic;
class TryCatch;

struct MagicMethod;
struct MagicOverload;

namespace types {
class Type;
class RecordType;
class RefType;
class GenType;
class OptionalType;
class KMer;

/**
 * VTable structure
 *
 * Holds all fields and methods (both magic and regular) associated
 * with a given type.
 */
struct VTable {
  /// Fields, encoded as named (int,type) pairs. The int part
  /// is the field's offset in the type (e.g. 0 for the int in an
  /// {int, float} structure); the type part is, well, the type.
  std::map<std::string, std::pair<int, Type *>> fields = {};

  /// Standard methods, encoded as named BaseFunc instances
  std::map<std::string, BaseFunc *> methods = {};

  /// Magic methods (i.e. those defined within the compiler)
  std::vector<MagicMethod> magic = {};

  /// Magic methods overload (i.e. those written by the user)
  std::vector<MagicOverload> overloads = {};
};

/**
 * The class from which all Seq "types" derive
 */
class Type {
protected:
  /// Type base name
  std::string name;

  /// Parent type, or null if there is none
  Type *parent;

  /// Whether this type is abstract (e.g. void)
  bool abstract;

  /// Whether this type can be extended via `extend`
  bool extendable;

  /// Fields and methods for this type
  VTable vtable;

public:
  /// Base type constructor
  /// @param name base name of this type
  /// @param parent parent type
  /// @param abstract whether this type can be instantiated
  /// @param extendable whether this type can be extended via an
  ///                   `extend` statement
  Type(std::string name, Type *parent, bool abstract = false, bool extendable = false);

  /// Returns a unique identifier for this type, based on
  /// the given name.
  static int getID(const std::string &name);

  /// Returns a unique identifier for this type, based on
  /// \ref getName() "getName()".
  virtual int getID() const;

  /// Returns the full name (e.g. `array[int]`, `Foo[str,bool]`) of this type.
  virtual std::string getName() const;

  /// Returns this type's parent type.
  virtual Type *getParent() const;

  /// Returns whether this type is abstract (i.e. cannot be instantiated).
  virtual bool isAbstract() const;

  /// Returns a reference to this type's VTable.
  virtual VTable &getVTable();

  /// Returns whether this type is "atomic". A type is defined to be
  /// atomic if its LLVM representation contains no pointers to heap
  /// allocated data. This is used internally by the GC, as we can
  /// allocate "atomic" and "non-atomic" blocks separately. Atomic
  /// blocks do not need to be scanned when searching for pointers.
  virtual bool isAtomic() const;

  /// Codegen's an allocation.
  /// @param count how many units of this type to allocate
  /// @param block where to codegen this allocation
  /// @return value representing pointer returned from allocation
  virtual llvm::Value *alloc(llvm::Value *count, llvm::BasicBlock *block);

  /// Calls this type. Usually a call to this method should be preceded
  /// by a call to \ref getCallType() "getCallType()" to validate types
  /// and determine the output type.
  /// @param base the function containing \p block
  /// @param self a value of this type
  /// @param args vector of argument values
  /// @param block where to codegen this call in
  /// @param normal if in a try, block to branch to if call succeeds,
  ///               or null otherwise
  /// @param unwind if in a try, block to branch to if call raises an
  ///               exception, or null otherwise
  /// @return value resulting from call
  virtual llvm::Value *call(BaseFunc *base, llvm::Value *self,
                            const std::vector<llvm::Value *> &args,
                            llvm::BasicBlock *block, llvm::BasicBlock *normal,
                            llvm::BasicBlock *unwind);

  /// Extract a given member (field or method) from this type.
  /// @param self a value of this type
  /// @param name name of the member
  /// @param block where to codegen the member
  /// @return member value
  virtual llvm::Value *memb(llvm::Value *self, const std::string &name,
                            llvm::BasicBlock *block);

  /// Return the type of a given member (field or method) of
  /// this type.
  /// @param name name of the member
  /// @return type of the member
  virtual Type *membType(const std::string &name);

  /// Sets the specified member of this type.
  /// @param self a value of this type
  /// @param name name of the member
  /// @param val value to assign to member
  /// @param block where to codegen the assignment
  /// @return value (possibly \p self) containing new member
  virtual llvm::Value *setMemb(llvm::Value *self, const std::string &name,
                               llvm::Value *val, llvm::BasicBlock *block);

  /// Checks whether type has specified (possibly magic) method.
  /// @param name name of the method
  /// @return whether this type has the specified method
  virtual bool hasMethod(const std::string &name);

  /// Adds a specified (possibly magic) method to this type.
  /// @param name name of the method
  /// @param func method to add
  /// @param force error on duplicate method names if false
  ///              override existing same-name method if true
  virtual void addMethod(std::string name, BaseFunc *func, bool force);

  /// Returns the specified (**non-magic**) method of this type.
  /// Throws an exception if the specified method does not exist.
  /// @param name name of the method
  /// @return BaseFunc representing the specified method
  virtual BaseFunc *getMethod(const std::string &name);

  /// Returns the specified static member (usually a method) of
  /// this type. This works for both regular and magic methods.
  /// @param name name of the member
  /// @param block where to codegen the member
  /// @return member value
  virtual llvm::Value *staticMemb(const std::string &name, llvm::BasicBlock *block);

  /// Returns the type of the specified static member (usually a
  /// method). This works for both regular and magic methods.
  /// @param name name of the member
  /// @return type of the member
  virtual Type *staticMembType(const std::string &name);

  /// Codegens the default value of this type. Usually this is
  /// just what you'd expect: zero for integral types, null for
  /// pointer and reference types, recursively defined for
  /// aggregate types.
  /// @param block where to codegen the default value
  /// @return value of the default value
  virtual llvm::Value *defaultValue(llvm::BasicBlock *block);

  /// Convenience method for calling `type.__bool__`.
  /// @param self value of this type
  /// @param block where to codegen the bool value
  /// @param tc enclosing try-catch statement, or null if none
  /// @return bool value of this type
  virtual llvm::Value *boolValue(llvm::Value *self, llvm::BasicBlock *&block,
                                 TryCatch *tc);

  /// Convenience method for calling `type.__str__`.
  /// @param self value of this type
  /// @param block where to codegen the str value
  /// @param tc enclosing try-catch statement, or null if none
  /// @return str value of this type
  virtual llvm::Value *strValue(llvm::Value *self, llvm::BasicBlock *&block,
                                TryCatch *tc);

  /// Convenience method for calling `type.__len__`.
  /// @param self value of this type
  /// @param block where to codegen the len value
  /// @param tc enclosing try-catch statement, or null if none
  /// @return len value of this type
  virtual llvm::Value *lenValue(llvm::Value *self, llvm::BasicBlock *&block,
                                TryCatch *tc);

  /// Performs a one-time initialization of this type's methods,
  /// including magic methods.
  virtual void initOps();

  /// Performs a one-time initialization of this type's fields.
  virtual void initFields();

  /// Returns the output type of the specified magic method
  /// with specified argument types. Throws an exception if
  /// the specified magic method does not exist by default.
  /// @param name full magic method name
  /// @param args vector of argument types (excluding 'self');
  ///             last element being null indicates static.
  /// @param nullOnMissing return null instead of throwing an
  ///                      exception if magic is missing
  /// @return output type of specified magic method
  virtual Type *magicOut(const std::string &name, std::vector<Type *> args,
                         bool nullOnMissing = false, bool overloadsOnly = false);

  BaseFunc *findMagic(const std::string &name, std::vector<types::Type *> args);

  /// Codegens a call to the specified magic method. Throws
  /// an exception if the specified magic method does not
  /// exist.
  /// @param name full magic method name
  /// @param argTypes vector of argument types (exclusing self)
  /// @param self value of this type; null if magic is static
  /// @param args vector of argument values (same size as \p argTypes)
  /// @param block where to codegen the call
  /// @param tc enclosing try-catch statement, or null if none
  /// @return result of calling the magic method
  virtual llvm::Value *callMagic(const std::string &name, std::vector<Type *> argTypes,
                                 llvm::Value *self, std::vector<llvm::Value *> args,
                                 llvm::BasicBlock *&block, TryCatch *tc);

  /// Returns the output type of the "__init__" magic method
  /// with specified argument types/names. Throws an exception if
  /// the "__init__" magic method does not exist for specified types/names by
  /// default. If \p initFunc is specified, always returns null but stores
  /// the found init function in that pointer and sets \p args to the fixed
  /// argument types based on \p names.
  /// @param args vector of argument types (excluding 'self');
  ///             last element being null indicates static.
  /// @param names vector of argument names (same size as \p args)
  /// @param nullOnMissing return null instead of throwing an
  ///                      exception if magic is missing
  /// @param initFunc will store resulting init function if non-null
  /// @return output type of "__init__" magic method
  virtual Type *initOut(std::vector<Type *> &args, std::vector<std::string> names,
                        bool nullOnMissing = false, Func **initFunc = nullptr);

  /// Codegens a call to the "__init__" magic method. Throws
  /// an exception if the "__init__" magic method does not
  /// exist for specified names/argument types.
  /// @param argTypes vector of argument types (exclusing self)
  /// @param names vector of argument names (same size as \p argTypes)
  /// @param self value of this type; null if magic is static
  /// @param args vector of argument values (same size as \p argTypes)
  /// @param block where to codegen the call
  /// @param tc enclosing try-catch statement, or null if none
  /// @return result of calling the "__init__" magic method
  virtual llvm::Value *callInit(std::vector<Type *> argTypes,
                                std::vector<std::string> names, llvm::Value *self,
                                std::vector<llvm::Value *> args,
                                llvm::BasicBlock *&block, TryCatch *tc);

  /// Checks whether this type "is" another type.
  virtual bool is(Type *type) const;

  /// Checks whether this type "is" another type, excluding
  /// base types. E.g., `array[int].is(array[float])`
  /// would return true.
  virtual bool isGeneric(Type *type) const;

  /// Returns the number of "base types" of this type. E.g.,
  /// `int.numBaseTypes()` would return 0, whereas
  /// `array[str].numBaseTypes()` would return 1, and
  /// `(int,float,str).numBaseTypes()` would return 3.
  virtual unsigned numBaseTypes() const;

  /// Obtain the base type at index \p idx. \p idx should be
  /// less than \ref numBaseTypes() "numBaseTypes()".
  virtual Type *getBaseType(unsigned idx) const;

  /// Returns the result of calling this type with the given
  /// argument types.
  virtual Type *getCallType(const std::vector<Type *> &inTypes);

  /// Returns the LLVM type corresponding to this type.
  virtual llvm::Type *getLLVMType(llvm::LLVMContext &context) const;

  /// Returns the size (in bytes) of the LLVM type corresponding
  /// to this type.
  virtual size_t size(llvm::Module *module) const;

  /// Returns this type as a record type, or null if it isn't
  /// a record type. This is basically for overriding C++'s
  /// RTTI/`dynamic_cast` so that generic types can be converted
  /// to their actual types.
  virtual RecordType *asRec();

  /// Returns this type as a reference type, or null if it isn't
  /// a reference type. This is basically for overriding C++'s
  /// RTTI/`dynamic_cast` so that generic types can be converted
  /// to their actual types.
  virtual RefType *asRef();

  /// Returns this type as a generator type, or null if it isn't
  /// a generator type. This is basically for overriding C++'s
  /// RTTI/`dynamic_cast` so that generic types can be converted
  /// to their actual types.
  virtual GenType *asGen();

  /// Returns this type as an optional type, or null if it isn't
  /// an optional type. This is basically for overriding C++'s
  /// RTTI/`dynamic_cast` so that generic types can be converted
  /// to their actual types.
  virtual OptionalType *asOpt();

  /// Returns this type as a k-mer type, or null if it isn't
  /// a k-mer type. This is basically for overriding C++'s
  /// RTTI/`dynamic_cast` so that generic types can be converted
  /// to their actual types.
  virtual KMer *asKMer();
};

/// Convenience method for checking if two types are equal.
/// Internally checks "\p type1 is \p type2" _or_ "\p type2
/// is \p type1".
bool is(Type *type1, Type *type2);
} // namespace types

/**
 * Intrinsic magic methods (such as `int.__add__`).
 *
 * Magic methods defined within the compiler are basically lambdas
 * that indicate how to perform code generation, with a few other
 * bits of information such as input/output types and a name.
 */
struct MagicMethod {
  /// Full magic method name
  std::string name;

  /// Magic method argument types, excluding `self`
  std::vector<types::Type *> args;

  /// Magic method output type
  types::Type *out;

  /// Code generation function. First argument is `self`, second
  /// is a vector of method arguments, third is an IRBuilder in
  /// the target BasicBlock.
  std::function<llvm::Value *(llvm::Value *, std::vector<llvm::Value *>,
                              llvm::IRBuilder<> &)>
      codegen;

  /// Whether this magic method is static
  bool isStatic;

  /// Converts magic method to a \ref BaseFunc "BaseFunc"
  /// @param type 'self' type
  BaseFunc *asFunc(types::Type *type) const;
};

/**
 * Overloaded magic methods
 */
struct MagicOverload {
  /// Full magic method name
  std::string name;

  /// Function representing the method
  BaseFunc *func;
};

} // namespace seq
