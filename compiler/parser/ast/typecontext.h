/**
 * transform.h
 * Type checking AST walker.
 *
 * Simplifies a given AST and generates types for each expression node.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/ast/types.h"
#include "parser/common.h"
#include "parser/context.h"

namespace seq {
namespace ast {

/// Current identifier table
class TypeContext : public VTable<Type> {
private: /** Naming **/
  /// Current filename
  std::string filename;
  /// Context module (e.g. __main__, sys etc)
  std::string module;
  /// Current name prefix (for functions within classes)
  std::string prefix;

  /// Name counter (how many times we used a name)
  /// Used for generating unique name for each identifier
  /// (e.g. if we have two def foo, one will be known as foo and one as foo.1
  std::unordered_map<std::string, int> moduleNames;
  /// Mapping to canonical names
  /// (each SrcInfo positions maps to a unique canonical name)
  std::unordered_map<seq::SrcInfo, std::string> canonicalNames;

private: /** Lookup **/
  /// Hashtable that determines if an identifier is a type variable
  std::unordered_map<std::string, std::stack<bool>> isType;
  /// Store internal types separately for easier access
  std::unordered_map<std::string, TypePtr> internals;
  /// List of class methods and members
  /// Maps canonical class name to a map of methods and members
  /// and their generalized types
  std::unordered_map<std::string, std::unordered_map<std::string, TypePtr>>
      classMembers;
  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::shared_ptr<FuncType>>>
      classMethods;

private: /** Type-checking **/
  /// Current type-checking level
  int level;
  /// Current unbound type ID counter.
  /// Each unbound variable must have different ID.
  int unboundCount;
  /// Set of active unbound variables.
  /// If type checking is successful, all of them should be resolved.
  std::set<TypePtr> activeUnbounds;

private: /** Realization **/
  /// Template function ASTs.
  /// Mapping from a canonical function name to a pair of
  /// generalized function type and the untyped function AST.
  std::unordered_map<std::string,
                     std::pair<TypePtr, std::shared_ptr<FunctionStmt>>>
      funcASTs;
  /// Template class ASTs.
  /// Mapping from a canonical class name to a pair of
  /// generalized class type and the untyped class AST.
  std::unordered_map<std::string,
                     std::pair<TypePtr, std::shared_ptr<ClassStmt>>>
      classASTs;
  /// Current function realizations.
  /// Mapping from a canonical function name to a hashtable
  /// of realized and fully type-checked function ASTs.
  std::unordered_map<
      std::string,
      std::unordered_map<std::string,
                         std::pair<TypePtr, std::shared_ptr<FunctionStmt>>>>
      funcRealizations;
  /// Current class realizations.
  /// Mapping from a canonical class name to a hashtable
  /// of realized and fully type-checked class ASTs.
  std::unordered_map<
      std::string, std::unordered_map<std::string, std::shared_ptr<ClassStmt>>>
      classRealizations;

private: /** Function utilities **/
  /// Function parsing helpers: maintain current return type
  TypePtr returnType;
  /// Indicates if a return was seen (to account for procedures)
  bool hasSetReturnType;

  // I am still debating should I provide 1000 getters or setters
  // or just leave the classes below friendly as they are by design
  // rather intimate with this class.
  friend class TransformVisitor;

public:
  TypeContext(const std::string &filename);
  TypePtr find(const std::string &name, bool *isType = nullptr) const;
  TypePtr findInternal(const std::string &name) const;

  /// add/remove overrides that handle isType flag
  void add(const std::string &name, TypePtr var, bool type = false) {
    map[name].push(var);
    isType[name].push(type);
    stack.top().push_back(name);
  }
  void remove(const std::string &name) override {
    VTable::remove(name);
    auto i = isType.find(name);
    i->second.pop();
    if (!i->second.size()) {
      isType.erase(name);
    }
  }

  /// Get canonical name for a SrcInfo
  std::string getCanonicalName(const seq::SrcInfo &info);
  /// Generate canonical name for a SrcInfo and original class/function name
  std::string getCanonicalName(const std::string &name,
                               const seq::SrcInfo &info);

  /// Type-checking helpers
  void increaseLevel();
  void decreaseLevel();
  std::shared_ptr<LinkType> addUnbound(const seq::SrcInfo &srcInfo,
                                       bool setActive = true);
  /// Calls type->instantiate, but populates the instantiation table
  /// with "parent" type.
  /// Example: for list[T].foo, list[int].foo will populate type of foo so that
  /// the generic T gets mapped to int.
  TypePtr instantiate(const seq::SrcInfo &srcInfo, TypePtr type);
  TypePtr instantiate(const seq::SrcInfo &srcInfo, TypePtr type,
                      const std::vector<std::pair<int, TypePtr>> &generics);

  /// Getters and setters for the method/member/realization lookup tables
  std::shared_ptr<FuncType> findMethod(std::shared_ptr<ClassType> type,
                                       const std::string &method);
  TypePtr findMember(std::shared_ptr<ClassType> type,
                     const std::string &member);
  std::vector<std::pair<std::string, const FunctionStmt *>>
  getRealizations(const FunctionStmt *stmt);
};

} // namespace ast
} // namespace seq