#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "lang/seq.h"

using std::make_shared;
using std::make_unique;
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
  vector<SrcInfo> locations;
  vector<string> messages;

public:
  ParserException(const string &msg, SrcInfo info) noexcept : std::runtime_error(msg) {
    messages.push_back(msg);
    locations.push_back(info);
  }
  explicit ParserException(const string &msg) noexcept : ParserException(msg, {}) {}
  ParserException(const ParserException &e) noexcept : std::runtime_error(e) {}

  void trackRealize(const string &msg, SrcInfo i) {
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
bool getInt(seq_int_t *o, const unique_ptr<ast::Expr> &e, bool zeroOnNull = true);
vector<string> split(const string &s, char delim);
string escape(string s);
string executable_path(const char *argv0);
string getTemporaryVar(const string &prefix = "", char p = '$');
string chop(const string &s);
bool startswith(const string &s, const string &p);
bool endswith(const string &s, const string &p);
void error(const char *format);
void error(const SrcInfo &p, const char *format);

template <typename T> string join(const T &items, string delim = " ") {
  string s = "";
  for (int i = 0; i < items.size(); i++)
    s += (i ? delim : "") + items[i];
  return s;
}

template <typename T, typename U> bool in(const vector<T> &c, const U &i) {
  auto f = std::find(c.begin(), c.end(), i);
  return f != c.end();
}

template <typename T> string combine(const vector<T> &items, string delim = " ") {
  string s = "";
  for (int i = 0; i < items.size(); i++)
    if (items[i])
      s += (i ? delim : "") + items[i]->toString();
  return s;
}

template <typename T> auto clone(const unique_ptr<T> &t) {
  return t ? t->clone() : nullptr;
}

template <typename T> vector<T> clone(const vector<T> &t) {
  vector<T> v;
  for (auto &i : t)
    v.push_back(clone(i));
  return v;
}

template <typename T> vector<T> clone_nop(const vector<T> &t) {
  vector<T> v;
  for (auto &i : t)
    v.push_back(i.clone());
  return v;
}

template <typename K, typename V, typename U> bool in(const map<K, V> &c, const U &i) {
  auto f = c.find(i);
  return f != c.end();
}

template <typename T> string v2s(const vector<T> &targs) {
  vector<string> args;
  for (auto &t : targs)
    args.push_back(t->toString());
  return join(args, ", ");
}

template <typename T> string v2s(const vector<std::pair<string, T>> &targs) {
  vector<string> args;
  for (auto &t : targs)
    args.push_back(t.second->toString());
  return join(args, ", ");
}

string getImportFile(const string &argv0, const string &what, const string &relativeTo,
                     bool forceStdlib);

} // namespace ast
} // namespace seq
