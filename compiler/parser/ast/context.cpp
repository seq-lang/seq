#include <libgen.h>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/context.h"
#include "parser/common.h"
#include "parser/ocaml.h"

using fmt::format;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::stack;
using std::string;
using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace seq {
namespace ast {

using namespace types;

// RealizationContext::RealizationContext() : unboundCount(0) {}

// int &RealizationContext::getUnboundCount() { return unboundCount; }

// RealizationContext::ClassBody *RealizationContext::findClass(const std::string &name)
// {
//   auto m = classes.find(name);
//   if (m != classes.end())
//     return &m->second;
//   return nullptr;
// }

// const std::vector<FuncTypePtr> *
// RealizationContext::findMethod(const string &name, const string &method) const {
//   auto m = classes.find(name);
//   if (m != classes.end()) {
//     auto t = m->second.methods.find(method);
//     if (t != m->second.methods.end())
//       return &t->second;
//   }
//   return nullptr;
// }

// TypePtr RealizationContext::findMember(const string &name, const string &member)
// const {
//   auto m = classes.find(name);
//   if (m != classes.end()) {
//     for (auto &mm : m->second.members)
//       if (mm.first == member)
//         return mm.second;
//   }
//   return nullptr;
// }

// shared_ptr<Stmt> RealizationContext::getAST(const string &name) const {
//   auto m = funcASTs.find(name);
//   if (m != funcASTs.end())
//     return m->second.second;
//   return nullptr;
// }

// vector<RealizationContext::ClassRealization>
// RealizationContext::getClassRealizations(const string &name) {
//   vector<RealizationContext::ClassRealization> result;
//   for (auto &i : classRealizations[name])
//     result.push_back(i.second);
//   return result;
// }

// vector<RealizationContext::FuncRealization>
// RealizationContext::getFuncRealizations(const string &name) {
//   vector<RealizationContext::FuncRealization> result;
//   for (auto &i : funcRealizations[name])
//     result.push_back(i.second);
//   return result;
// }

} // namespace ast
} // namespace seq
