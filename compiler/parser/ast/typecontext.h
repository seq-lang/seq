/**
 * transform.h
 * Type checking AST walker.
 *
 * Simplifies a given AST and generates types for each expression node.
 */

#pragma once

#include <deque>
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

using namespace types;

class TypeContext;

struct RealizationContext {
  struct ClassBody {
    std::unordered_map<std::string, TypePtr> members;
    std::unordered_map<std::string, std::vector<FuncTypePtr>> methods;
  };
  struct FuncRealization {
    FuncTypePtr type;
    std::shared_ptr<FunctionStmt> ast;
    seq::BaseFunc *handle;
  };
  struct ClassRealization {
    ClassTypePtr type;
    seq::types::Type *handle;
  };
  RealizationContext();

public: /* Names */
  struct SrcInfoHash {
    size_t operator()(const seq::SrcInfo &k) const {
      // http://stackoverflow.com/a/1646913/126995
      size_t res = 17;
      res = res * 31 + std::hash<std::string>()(k.file);
      res = res * 31 + std::hash<int>()(k.line);
      res = res * 31 + std::hash<int>()(k.id);
      return res;
    }
  };
  /// Name counter (how many times we used a name)
  /// Used for generating unique name for each identifier
  /// (e.g. if we have two def foo, one will be known as foo and one as foo.1
  std::unordered_map<std::string, int> moduleNames;
  /// Mapping to canonical names
  /// (each SrcInfo positions maps to a unique canonical name)
  std::unordered_map<SrcInfo, std::string, SrcInfoHash> canonicalNames;
  /// Current unbound type ID counter.
  /// Each unbound variable must have different ID.
  int unboundCount;

public:
  /// Get canonical name for a SrcInfo
  std::string getCanonicalName(const SrcInfo &info) const;
  /// Generate canonical name for a SrcInfo and original class/function name
  std::string generateCanonicalName(const SrcInfo &info,
                                    const std::string &module,
                                    const std::string &name);
  int &getUnboundCount();

public: /* Lookup */
  /// List of class methods and members
  /// Maps canonical class name to a map of methods and members
  /// and their generalized types
  std::unordered_map<std::string, ClassBody> classes;

public:
  /// Getters and setters for the method/member/realization lookup tables
  ClassBody *findClass(const std::string &name);
  const std::vector<FuncTypePtr> *findMethod(const std::string &name,
                                             const std::string &method) const;
  TypePtr findMember(const std::string &name, const std::string &member) const;

public: /** Template ASTs **/
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
  std::shared_ptr<Stmt> getAST(const std::string &name) const;

public: /* Realizations */
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

public:
  std::vector<ClassRealization> getClassRealizations(const std::string &name);
  std::vector<FuncRealization> getFuncRealizations(const std::string &name);
};

/**************************************************************************************/

class ImportContext {
public:
  struct Import {
    std::string filename;
    std::shared_ptr<TypeContext> ctx;
    StmtPtr statements;
  };

private:
  std::string argv0;
  /// By convention, stdlib is stored as ""
  std::unordered_map<std::string, Import> imports;

public:
  ImportContext(const std::string &argv0 = "");
  std::string getImportFile(const std::string &what,
                            const std::string &relativeTo,
                            bool forceStdlib = false) const;
  std::shared_ptr<TypeContext> getImport(const std::string &path) const;
  void addImport(const std::string &file, const std::string &name,
                 std::shared_ptr<TypeContext> ctx);
  void setBody(const std::string &name, StmtPtr body);
};

/**************************************************************************************/

class TItem {
protected:
  std::string base;
  bool global;
  std::unordered_set<std::string> attributes;

public:
  TItem(const std::string &base, bool global = false);
  virtual ~TItem() {}

  virtual bool isType() const { return false; }
  virtual bool isFunc() const { return false; }
  virtual bool isVar() const { return false; }
  virtual bool isImport() const { return false; }
  virtual bool isStatic() const { return false; }
  virtual TypePtr getType() const { return nullptr; }

  std::string getBase() const;
  bool isGlobal() const;
  void setGlobal();
  bool hasAttr(const std::string &s) const;
};

class TImportItem : public TItem {
  std::string name;

public:
  TImportItem(const std::string &name, const std::string &base,
              bool global = false)
      : TItem(base, global), name(name) {}
  bool isImport() const override { return true; }
};

class TStaticItem : public TItem {
  int value;

public:
  TStaticItem(int value, const std::string &base, bool global = false)
      : TItem(base, global), value(value) {}
  bool isStatic() const override { return true; }
  int getValue() const { return value; }
};

class TVarItem : public TItem {
  TypePtr type;

public:
  TVarItem(TypePtr type, const std::string &base, bool global = false)
      : TItem(base, global), type(type) {}
  bool isVar() const override { return true; }
  TypePtr getType() const override { return type; }
};

class TTypeItem : public TItem {
  TypePtr type;

public:
  TTypeItem(TypePtr type, const std::string &base, bool global = false)
      : TItem(base, global), type(type) {}
  bool isType() const override { return true; }
  TypePtr getType() const override { return type; }
};

class TFuncItem : public TItem {
  TypePtr type;

public:
  TFuncItem(TypePtr type, const std::string &base, bool global = false)
      : TItem(base, global), type(type) {}
  bool isFunc() const override { return true; }
  TypePtr getType() const override { return type; }
};

/// Current identifier table
class TypeContext : public VTable<TItem>,
                    public std::enable_shared_from_this<TypeContext> {
  std::shared_ptr<RealizationContext> realizations;
  std::shared_ptr<ImportContext> imports;

  /** Naming **/
  /// Current filename
  std::string filename;
  /// Context module (e.g. __main__, sys etc)
  std::string module;
  /// Current name prefix (for functions within classes)
  std::vector<std::string> bases;

  /** Type-checking **/
  /// Current type-checking level
  int level;
  /// Set of active unbound variables.
  /// If type checking is successful, all of them should be resolved.
  std::set<TypePtr> activeUnbounds;

  /** Function utilities **/
  /// Function parsing helpers: maintain current return type
  TypePtr returnType;
  /// Indicates if a return was seen (to account for procedures)
  bool hasSetReturnType;

public:
  TypeContext(const std::string &filename,
              std::shared_ptr<RealizationContext> realizations,
              std::shared_ptr<ImportContext> imports);
  virtual ~TypeContext() {}

  std::shared_ptr<TItem> find(const std::string &name,
                              bool checkStdlib = true) const;
  TypePtr findInternal(const std::string &name) const;

  using VTable<TItem>::add;
  void add(const std::string &name, TypePtr type, bool global = false);
  void addImport(const std::string &name, const std::string &import,
                 bool global = false);
  void addType(const std::string &name, TypePtr type, bool global = false);
  void addFunc(const std::string &name, TypePtr type, bool global = false);
  void addStatic(const std::string &name, int value, bool global = false);

public:
  std::string getBase() const;
  std::string getModule() const;
  std::string getFilename() const;
  std::shared_ptr<RealizationContext> getRealizations() const;
  std::shared_ptr<ImportContext> getImports() const;
  void increaseLevel();
  void decreaseLevel();

public:
  std::shared_ptr<LinkType> addUnbound(const SrcInfo &srcInfo,
                                       bool setActive = true);
  /// Calls `type->instantiate`, but populates the instantiation table
  /// with "parent" type.
  /// Example: for list[T].foo, list[int].foo will populate type of foo so that
  /// the generic T gets mapped to int.
  TypePtr instantiate(const SrcInfo &srcInfo, TypePtr type);
  TypePtr instantiate(const SrcInfo &srcInfo, TypePtr type,
                      GenericTypePtr generics, bool activate = true);
  TypePtr instantiateGeneric(const SrcInfo &srcInfo, TypePtr root,
                             const std::vector<TypePtr> &generics);
  ImportContext::Import importFile(const std::string &file);

public:
  static std::shared_ptr<TypeContext> getContext(const std::string &argv0,
                                                 const std::string &file);
  // I am still debating should I provide 1000 getters or setters
  // or just leave the classes below friendly as they are by design
  // rather intimate with this class.
  friend class TransformVisitor;
  void dump();
};

} // namespace ast
} // namespace seq