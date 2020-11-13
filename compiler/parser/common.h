#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "lang/seq.h"

/// Macro that makes node cloneable and visitable
#define NODE_UTILITY(X, Y)                                                             \
  virtual X##Ptr clone() const override { return std::make_unique<Y>(*this); }         \
  virtual void accept(ASTVisitor &visitor) const override { visitor.visit(this); }

using std::map;
using std::pair;
using std::set;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;

namespace seq {

namespace exc {
class ParserException : public std::runtime_error {
public:
  std::vector<SrcInfo> locations;
  std::vector<std::string> messages;

public:
  ParserException(const std::string &msg, SrcInfo info) noexcept
      : std::runtime_error(msg) {
    messages.push_back(msg);
    locations.push_back(info);
  }
  explicit ParserException(const std::string &msg) noexcept
      : ParserException(msg, {}) {}
  ParserException(const ParserException &e) noexcept : std::runtime_error(e) {}

  void trackRealize(const std::string &msg, SrcInfo i) {
    locations.push_back(i);
    messages.push_back(fmt::format("while realizing {}", msg));
  }
};
} // namespace exc

namespace ast {

struct SrcInfoHash {
  size_t operator()(const seq::SrcInfo &k) const { return std::hash<int>()(k.id); }
};

struct Expr;
bool getInt(seq_int_t *o, const std::unique_ptr<ast::Expr> &e, bool zeroOnNull = true);
std::vector<std::string> split(const std::string &s, char delim);
std::string escape(std::string s);
std::string executable_path(const char *argv0);
std::string getTemporaryVar(const std::string &prefix = "", char p = '$');
std::string chop(const std::string &s);
bool startswith(const std::string &s, const std::string &p);
bool endswith(const std::string &s, const std::string &p);
void error(const char *format);
void error(const SrcInfo &p, const char *format);

template <typename T> std::string join(const T &items, std::string delim = " ") {
  std::string s = "";
  for (int i = 0; i < items.size(); i++)
    s += (i ? delim : "") + items[i];
  return s;
}

template <typename T, typename U> bool in(const std::vector<T> &c, const U &i) {
  auto f = std::find(c.begin(), c.end(), i);
  return f != c.end();
}

template <typename T>
std::string combine(const std::vector<T> &items, std::string delim = " ") {
  std::string s = "";
  for (int i = 0; i < items.size(); i++)
    if (items[i])
      s += (i ? delim : "") + items[i]->toString();
  return s;
}

template <typename T> auto clone(const std::unique_ptr<T> &t) {
  return t ? t->clone() : nullptr;
}

template <typename T> std::vector<T> clone(const std::vector<T> &t) {
  std::vector<T> v;
  for (auto &i : t)
    v.push_back(clone(i));
  return v;
}

template <typename T> std::vector<T> clone_nop(const std::vector<T> &t) {
  std::vector<T> v;
  for (auto &i : t)
    v.push_back(i.clone());
  return v;
}

template <typename K, typename V, typename U>
bool in(const std::map<K, V> &c, const U &i) {
  auto f = c.find(i);
  return f != c.end();
}

template <typename T> std::string v2s(const std::vector<T> &targs) {
  std::vector<std::string> args;
  for (auto &t : targs)
    args.push_back(t->toString());
  return join(args, ", ");
}

template <typename T>
std::string v2s(const std::vector<std::pair<std::string, T>> &targs) {
  std::vector<std::string> args;
  for (auto &t : targs)
    args.push_back(t.second->toString());
  return join(args, ", ");
}

std::string getImportFile(const std::string &argv0, const std::string &what,
                          const std::string &relativeTo, bool forceStdlib);

} // namespace ast
} // namespace seq
