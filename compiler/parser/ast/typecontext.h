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

class TContextItem {
protected:
  TypePtr type;
  bool typeVar;
  bool global;
  std::unordered_set<std::string> attributes;

public:
  TContextItem(TypePtr t, bool isType = false, bool global = false);
  virtual ~TContextItem() {}

  bool isType() const;
  bool isGlobal() const;
  TypePtr getType() const;
  bool hasAttr(const std::string &s) const;
};

/// Current identifier table
class TypeContext : public VTable<TContextItem> {
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
  std::unordered_map<SrcInfo, std::string> canonicalNames;

public:
  struct ClassBody {
    std::unordered_map<std::string, TypePtr> members;
    std::unordered_map<std::string, FuncTypePtr> methods;
  };

private: /** Lookup **/
  /// Store internal types separately for easier access
  std::unordered_map<std::string, TypePtr> internals;
  /// List of class methods and members
  /// Maps canonical class name to a map of methods and members
  /// and their generalized types
  std::unordered_map<std::string, ClassBody> classes;

public:
  /// Getters and setters for the method/member/realization lookup tables
  FuncTypePtr findMethod(const std::string &name,
                         const std::string &method) const;
  TypePtr findMember(const std::string &name, const std::string &member) const;
  ClassBody *findClass(const std::string &name);

private: /** Type-checking **/
  /// Current type-checking level
  int level;
  /// Current unbound type ID counter.
  /// Each unbound variable must have different ID.
  int unboundCount;
  /// Set of active unbound variables.
  /// If type checking is successful, all of them should be resolved.
  std::set<TypePtr> activeUnbounds;

public:
  /// Type-checking helpers
  void increaseLevel();
  void decreaseLevel();
  std::shared_ptr<LinkType> addUnbound(const SrcInfo &srcInfo,
                                       bool setActive = true);
  /// Calls type->instantiate, but populates the instantiation table
  /// with "parent" type.
  /// Example: for list[T].foo, list[int].foo will populate type of foo so that
  /// the generic T gets mapped to int.
  TypePtr instantiate(const SrcInfo &srcInfo, TypePtr type);
  TypePtr instantiate(const SrcInfo &srcInfo, TypePtr type,
                      const std::vector<std::pair<int, TypePtr>> &generics);

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

public:
  struct FuncRealization {
    FuncTypePtr type;
    std::shared_ptr<FunctionStmt> ast;
    seq::Func *handle;
  };
  struct ClassRealization {
    ClassTypePtr type;
    seq::types::Type *handle;
  };

  std::shared_ptr<Stmt> getAST(const std::string &name) const;

private:
  /// Current function realizations.
  /// Mapping from a canonical function name to a hashtable
  /// of realized and fully type-checked function ASTs.
  std::unordered_map<std::string,
                     std::unordered_map<std::string, FuncRealization>>
      funcRealizations;
  /// Current class realizations.
  /// Mapping from a canonical class name to a hashtable
  /// of realized and fully type-checked class ASTs.

  std::unordered_map<std::string,
                     std::unordered_map<std::string, ClassRealization>>
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
  std::shared_ptr<TContextItem> find(const std::string &name) const;
  TypePtr findInternal(const std::string &name) const;

  void add(const std::string &name, TypePtr t, bool isType = false,
           bool global = false);

  /// Get canonical name for a SrcInfo
  std::string getCanonicalName(const SrcInfo &info);
  /// Generate canonical name for a SrcInfo and original class/function name
  std::string generateCanonicalName(const SrcInfo &info,
                                    const std::string &name);

  std::vector<ClassRealization> getClassRealizations(const std::string &name);
  std::vector<FuncRealization> getFuncRealizations(const std::string &name);
};

} // namespace ast
} // namespace seq