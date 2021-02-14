/*
 * cache.h --- Code transformation cache (shared objects).
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast.h"
#include "parser/common.h"
#include "parser/ctx.h"

#include "sir/sir.h"

#define TYPECHECK_MAX_ITERATIONS 100
#define FILE_GENERATED "<generated>"
#define MODULE_MAIN "__main__"
#define MAIN_IMPORT ""
#define STDLIB_IMPORT ":stdlib:"
#define STDLIB_INTERNAL_MODULE "internal"
#define ATTR_INTERNAL "internal"
#define ATTR_TUPLE "tuple"
#define ATTR_TRAIT "trait"
#define ATTR_ATOMIC "atomic"
#define ATTR_TEST "test"
#define ATTR_EXTERN_C ".c"
#define ATTR_EXTERN_LLVM "llvm"
#define ATTR_EXTERN_PYTHON "python"
#define ATTR_FORCE_REALIZE "force_realize"
#define ATTR_PARENT_FUNCTION ".parentFunc"
#define ATTR_PARENT_CLASS ".parentClass"
#define ATTR_NOT_STATIC ".notStatic"
#define ATTR_TOTAL_ORDERING "total_ordering"
#define ATTR_CONTAINER "container"
#define ATTR_PYTHON "python"
#define ATTR_PICKLE "pickle"
#define ATTR_DICT "dict"
#define ATTR_STDLIB ".stdlib"
#define ATTR_NO(x) ("no_" x)

namespace seq {
namespace ast {

/// Forward declarations
struct SimplifyContext;
struct TypeContext;

/**
 * Cache encapsulation that holds data structures shared across various transformation
 * stages (AST transformation, type checking etc.). The subsequent stages (e.g. type
 * checking) assumes that previous stages populated this structure correctly.
 * Implemented to avoid bunch of global objects.
 */
struct Cache : public std::enable_shared_from_this<Cache> {
  /// Stores a count for each identifier (name) seen in the code.
  /// Used to generate unique identifier for each name in the code (e.g. Foo -> Foo.2).
  unordered_map<string, int> identifierCount;
  /// Maps a unique identifier back to the original name in the code
  /// (e.g. Foo.2 -> Foo).
  unordered_map<string, string> reverseIdentifierLookup;
  /// Number of code-generated source code positions. Used to generate the next unique
  /// source-code position information.
  int generatedSrcInfoCount;
  /// Number of unbound variables so far. Used to generate the next unique unbound
  /// identifier.
  int unboundCount;
  /// Number of auto-generated variables so far. Used to generate the next unique
  /// variable name in getTemporaryVar() below.
  int varCount;
  /// Stores the count of imported files. Used to track class method ages
  /// and to prevent using extended methods before they were seen.
  int age;
  /// Test flags for seqtest test cases. Zero if seqtest is not parsing the code.
  int testFlags;

  /// Holds module import data.
  struct Import {
    /// Absolute filename of an import.
    string filename;
    /// Import simplify context.
    shared_ptr<SimplifyContext> ctx;
    /// Unique import variable for checking already loaded imports.
    string importVar;
  };

  /// Absolute path of seqc executable (if available).
  string argv0;
  /// LLVM module.
  seq::ir::IRModule *module = nullptr;

  /// Table of imported files that maps an absolute filename to a Import structure.
  /// By convention, the key of Seq standard library is "".
  unordered_map<string, Import> imports;

  /// Set of unique (canonical) global identifiers for marking such variables as global
  /// in code-generation step.
  set<string> globals;

  /// Stores class data for each class (type) in the source code.
  struct Class {
    /// Generic (unrealized) class template AST.
    unique_ptr<ClassStmt> ast;

    /// A class function method.
    struct ClassMethod {
      /// Canonical name of a method (e.g. __init__.1).
      string name;
      /// A corresponding generic function type.
      types::FuncTypePtr type;
      /// Method age (how many class extension were seen before a method definition).
      /// Used to prevent the usage of a method before it was defined in the code.
      int age;
    };
    /// Class method lookup table. Each name points to a list of ClassMethod instances
    /// that share the same method name (a list because methods can be overloaded).
    unordered_map<string, vector<ClassMethod>> methods;

    /// A class field (member).
    struct ClassField {
      /// Field name.
      string name;
      /// A corresponding generic field type.
      types::TypePtr type;
    };
    /// A list of class' ClassField instances. List is needed (instead of map) because
    /// the order of the fields matters.
    vector<ClassField> fields;

    /// A class realization.
    struct ClassRealization {
      /// Realized class type.
      types::ClassTypePtr type;
      /// A list of field names and realization's realized field types.
      vector<std::pair<string, types::TypePtr>> fields;
      /// IR type pointer.
      seq::ir::types::Type *ir;
    };
    /// Realization lookup table that maps a realized class name to the corresponding
    /// ClassRealization instance.
    unordered_map<string, ClassRealization> realizations;

    Class() : ast(nullptr) {}
  };
  /// Class lookup table that maps a canonical class identifier to the corresponding
  /// Class instance.
  unordered_map<string, Class> classes;

  struct Function {
    /// Generic (unrealized) function template AST.
    unique_ptr<FunctionStmt> ast;

    /// A function realization.
    struct FunctionRealization {
      /// Realized function type.
      types::FuncTypePtr type;
      /// Realized function AST (stored here for later realization in code generations
      /// stage).
      unique_ptr<FunctionStmt> ast;
      /// IR function pointer.
      ir::Func *ir;
    };
    /// Realization lookup table that maps a realized function name to the corresponding
    /// FunctionRealization instance.
    unordered_map<string, FunctionRealization> realizations;

    Function() : ast(nullptr) {}
  };
  /// Function lookup table that maps a canonical function identifier to the
  /// corresponding Function instance.
  unordered_map<string, Function> functions;

  /// Pointer to the typechecking context needed for later type realization.
  shared_ptr<TypeContext> typeCtx;

public:
  explicit Cache(string argv0 = "");

  /// Return a uniquely named temporary variable of a format
  /// "{sigil}_{prefix}{counter}". A sigil should be a non-lexable symbol.
  string getTemporaryVar(const string &prefix = "", char sigil = '.');

  /// Generate a unique SrcInfo for internally generated AST nodes.
  SrcInfo generateSrcInfo();

  /// Realization API.

  /// Find a class with a given canonical name and return a matching types::Type pointer
  /// or a nullptr if a class is not found.
  /// Returns an _uninstantiated_ type.
  types::ClassTypePtr findClass(const string &name) const;
  /// Find a function with a given canonical name and return a matching types::Type
  /// pointer or a nullptr if a function is not found.
  /// Returns an _uninstantiated_ type.
  types::FuncTypePtr findFunction(const string &name) const;
  /// Find the class method in a given class type that best matches the given arguments.
  /// Returns an _uninstantiated_ type.
  types::FuncTypePtr findMethod(types::ClassType *typ, const string &member,
                                const vector<pair<string, types::TypePtr>> &args);

  /// Given a class type and the matching generic vector, instantiate the type and
  /// realize it.
  ir::types::Type *realizeType(types::ClassTypePtr type,
                               vector<types::TypePtr> generics = {});
  /// Given a function type and function arguments, instantiate the type and
  /// realize it. The first argument is the function return type.
  /// You can also pass function generics if a function has one (e.g. T in def
  /// foo[T](...)). If a generic is used as an argument, it will be auto-deduced. Pass
  /// only if a generic cannot be deduced from the provided args.
  ir::Func *realizeFunction(types::FuncTypePtr type, vector<types::TypePtr> args,
                            vector<types::TypePtr> generics = {});
};

} // namespace ast
} // namespace seq
