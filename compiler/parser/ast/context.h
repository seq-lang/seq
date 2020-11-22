#pragma once

#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/ast/stmt.h"
#include "parser/ast/types.h"
#include "parser/common.h"

namespace seq {
namespace ast {

// struct RealizationContext {
//   /// List of class methods and members
//   /// Maps canonical class name to a map of methods and members
//   /// and their generalized types
//   struct ClassBody {
//     // Needs vector as the order is important
//     vector<std::pair<string, types::TypePtr>> members;
//     unordered_map<string, vector<types::FuncTypePtr>> methods;
//   };
//   unordered_map<string, ClassBody> classes;

//   struct FuncRealization {
//     string fullName;
//     types::FuncTypePtr type;
//     shared_ptr<FunctionStmt> ast;
//     seq::BaseFunc *handle;
//     string base;
//   };
//   struct ClassRealization {
//     string fullName;
//     types::ClassTypePtr type;
//     vector<std::pair<string, types::ClassTypePtr>> args;
//     seq::types::Type *handle;
//     string base;
//   };
//   RealizationContext();

// public: /* Names */
//   /// Name counter (how many times we used a name)
//   /// Used for generating unique name for each identifier
//   /// (e.g. if we have two def foo, one will be known as foo and one as foo.1
//   unordered_map<string, int> moduleNames;
//   /// Mapping to canonical names
//   /// (each SrcInfo positions maps to a unique canonical name)
//   unordered_map<SrcInfo, string, SrcInfoHash> canonicalNames;
//   /// Current unbound type ID counter.
//   /// Each unbound variable must have different ID.

// public:
//   /// Generate canonical name for a SrcInfo and original class/function name
//   string generateCanonicalName(const string &base, const string
//   &name); int &getUnboundCount();

// public: /* Lookup */
// public:
//   /// Getters and setters for the method/member/realization lookup tables
//   ClassBody *findClass(const string &name);
//   const vector<types::FuncTypePtr> *findMethod(const string &name,
//                                                     const string &method) const;
//   types::TypePtr findMember(const string &name, const string &member)
//   const;

// public: /** Template ASTs **/
//   /// Template function ASTs.
//   /// Mapping from a canonical function name to a pair of
//   /// generalized function type and the untyped function AST.
//   unordered_map<string,
//                      std::pair<types::TypePtr, shared_ptr<FunctionStmt>>>
//       funcASTs;
//   /// Template class ASTs.
//   /// Mapping from a canonical class name to a pair of
//   /// generalized class type and the untyped class AST.
//   unordered_map<string, types::TypePtr> classASTs;

// public:
//   shared_ptr<Stmt> getAST(const string &name) const;

// public: /* Realizations */
//   /// Current function realizations.
//   /// Mapping from a canonical function name to a hashtable
//   /// of realized and fully type-checked function ASTs.
//   unordered_map<string, unordered_map<string, FuncRealization>>
//       funcRealizations;
//   /// Current class realizations.
//   /// Mapping from a canonical class name to a hashtable
//   /// of realized and fully type-checked class ASTs.

//   unordered_map<string, unordered_map<string, ClassRealization>>
//       classRealizations;

//   // Maps realizedName to canonicalName
//   unordered_map<string, string> realizationLookup;

//   // vector<set<std::pair<string>>>
//   // realizationCache; // add newly realized functions here; useful for jit

// public:
//   vector<ClassRealization> getClassRealizations(const string &name);
//   vector<FuncRealization> getFuncRealizations(const string &name);

//   unordered_map<string, types::TypePtr> globalNames;
//   unordered_set<string> variardicCache;
// };

template <typename T> class Context : public std::enable_shared_from_this<Context<T>> {
public:
  typedef unordered_map<string, std::deque<std::pair<int, shared_ptr<T>>>>
      Map; // tracks level as well

protected:
  Map map;
  std::deque<vector<string>> stack;
  unordered_set<string> flags;

public:
  typename Map::iterator begin() { return map.begin(); }
  typename Map::iterator end() { return map.end(); }

  shared_ptr<T> find(const string &name) const {
    auto it = map.find(name);
    return it != map.end() ? it->second.front().second : nullptr;
  }
  void add(const string &name, shared_ptr<T> var) {
    assert(!name.empty());
    // LOG7("++ {}", name);
    map[name].push_front({stack.size(), var});
    stack.front().push_back(name);
  }
  void addToplevel(const string &name, shared_ptr<T> var) {
    assert(!name.empty());
    // LOG7("+++ {}", name);
    auto &m = map[name];
    int pos = m.size();
    while (pos > 0 && m[pos - 1].first == 1)
      pos--;
    m.insert(m.begin() + pos, {1, var});
    stack.back().push_back(name); // add to the latest "level"
  }
  void addBlock() { stack.push_front(vector<string>()); }
  void removeFromMap(const string &name) {
    auto i = map.find(name);
    assert(!(i == map.end() || !i->second.size()));
    // LOG7("-- {}", name);
    i->second.pop_front();
    if (!i->second.size())
      map.erase(name);
  }
  void popBlock() {
    for (auto &name : stack.front())
      removeFromMap(name);
    stack.pop_front();
  }
  void remove(const string &name) {
    removeFromMap(name);
    for (auto &s : stack) {
      auto i = std::find(s.begin(), s.end(), name);
      if (i != s.end()) {
        s.erase(i);
        return;
      }
    }
    assert(false);
  }
  void setFlag(const string &s) { flags.insert(s); }
  void unsetFlag(const string &s) { flags.erase(s); }
  bool hasFlag(const string &s) { return flags.find(s) != flags.end(); }
  bool isToplevel() const { return stack.size() == 1; }

protected:
  string filename;

public:
  string getFilename() const { return filename; }
  void setFilename(const string &f) { filename = f; }

public:
  Context(const string &filename) : filename(filename) {}
  virtual ~Context() {}
  virtual void dump(int pad = 0) {}
};

} // namespace ast
} // namespace seq
