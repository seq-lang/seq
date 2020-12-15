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

#define STDLIB_IMPORT ""
#define STDLIB_INTERNAL_MODULE "internal"
#define ATTR_INTERNAL "internal"
#define ATTR_TUPLE "tuple"
#define ATTR_TRAIT "trait"
#define ATTR_EXTERN_C ".c"
#define ATTR_EXTERN_LLVM ".c"
#define ATTR_EXTERN_PYTHON ".c"
#define ATTR_BUILTIN "builtin"
#define ATTR_PARENT_FUNCTION ".parentFunc"
#define ATTR_PARENT_CLASS ".parentClass"
#define ATTR_NOT_STATIC ".notStatic"
#define ATTR_TOTAL_ORDERING "total_ordering"
#define ATTR_CONTAINER "container"
#define ATTR_PYTHON "python"
#define ATTR_PICKLE "pickle"
#define ATTR_NO(x) ("no_" x)

namespace seq {
namespace ast {

/// Forward declarations
struct SimplifyItem;

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
    shared_ptr<Context<SimplifyItem>> ctx;
  };

  /// Absolute path of seqc executable (if available).
  string argv0;
  /// Table of imported files that maps an absolute filename to a Import structure.
  /// By convention, the key of Seq standard library is "".
  unordered_map<string, Import> imports;

  /// Previously generated variardic types (Function and Tuple).
  set<string> variardics;
  /// Table of generic AST nodes that maps a unique function or class identifier to a
  /// generic AST for later realization.
  unordered_map<string, StmtPtr> asts;

  /// Table of class methods that maps a unique class identifier to a map of method
  /// names. Each method name points to a list of FuncType instances with that name (a
  /// list because methods can be overloaded).
  unordered_map<string, unordered_map<string, vector<types::FuncTypePtr>>> classMethods;
  /// Table of class fields (object variables) that maps a unique class identifier to a
  /// list of field names and their types. List is used here instead of map because
  /// field order matters.
  unordered_map<string, vector<std::pair<string, types::TypePtr>>> classFields;
  /// Table of realizations that maps a unique generic function or class identifier to a
  /// map of their realization names and realized types.
  unordered_map<string, unordered_map<string, types::TypePtr>> realizations;
  /// Table of field realizations that maps a realized class identifier to a
  /// list of realized field names and their realized types.
  unordered_map<string, vector<std::pair<string, types::TypePtr>>> fieldRealizations;
  /// Table that maps realized name to its realized ASTs for code generation stage.
  unordered_map<string, StmtPtr> realizationAsts;

public:
  explicit Cache(string argv0 = "")
      : generatedSrcInfoCount(0), unboundCount(0), varCount(0), argv0(move(argv0)) {}

  /// Return a uniquely named temporary variable of a format
  /// "{sigil}_{prefix}{counter}". A sigil should be an unlexable symbol.
  string getTemporaryVar(const string &prefix = "", char sigil = '$') {
    return fmt::format("{}{}_{}", sigil ? fmt::format("{}_", sigil) : "", prefix,
                       ++varCount);
  }
};

} // namespace ast
} // namespace seq
