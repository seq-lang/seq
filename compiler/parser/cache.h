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

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/ctx.h"

#define FILE_GENERATED "<generated>"
#define MODULE_MAIN "__main__"
#define STDLIB_IMPORT ""
#define STDLIB_INTERNAL_MODULE "internal"
#define ATTR_INTERNAL "internal"
#define ATTR_TUPLE "tuple"
#define ATTR_TRAIT "trait"
#define ATTR_ATOMIC "atomic"
#define ATTR_EXTERN_C ".c"
#define ATTR_EXTERN_LLVM "llvm"
#define ATTR_EXTERN_PYTHON "python"
#define ATTR_BUILTIN "builtin"
#define ATTR_PARENT_FUNCTION ".parentFunc"
#define ATTR_PARENT_CLASS ".parentClass"
#define ATTR_NOT_STATIC ".notStatic"
#define ATTR_GENERIC ".generic"
#define ATTR_TOTAL_ORDERING "total_ordering"
#define ATTR_CONTAINER "container"
#define ATTR_PYTHON "python"
#define ATTR_PICKLE "pickle"
#define ATTR_NO(x) ("no_" x)

namespace seq {
namespace ast {

/// Forward declarations
struct SimplifyContext;

/**
 * Cache encapsulation that holds data structures shared across various transformation
 * stages (AST transformation, type checking etc.). The subsequent stages (e.g. type
 * checking) assumes that previous stages populated this structure correctly.
 * Implemented to avoid bunch of global objects.
 */
struct Cache {
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
  /// Table of imported files that maps an absolute filename to a Import structure.
  /// By convention, the key of Seq standard library is "".
  unordered_map<string, Import> imports;

  /// Previously generated variardic types (Function and Tuple).
  set<string> variardics;

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
    };
    /// Realization lookup table that maps a realized function name to the corresponding
    /// FunctionRealization instance.
    unordered_map<string, FunctionRealization> realizations;

    Function() : ast(nullptr) {}
  };
  /// Function lookup table that maps a canonical function identifier to the
  /// corresponding Function instance.
  unordered_map<string, Function> functions;

public:
  explicit Cache(string argv0 = "")
      : generatedSrcInfoCount(0), unboundCount(0), varCount(0), argv0(move(argv0)) {}

  /// Return a uniquely named temporary variable of a format
  /// "{sigil}_{prefix}{counter}". A sigil should be a non-lexable symbol.
  string getTemporaryVar(const string &prefix = "", char sigil = '$') {
    return fmt::format("{}{}_{}", sigil ? fmt::format("{}_", sigil) : "", prefix,
                       ++varCount);
  }
};

} // namespace ast
} // namespace seq
