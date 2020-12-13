#pragma once

#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lang/seq.h"
#include "parser/ast.h"
#include "parser/common.h"

namespace seq {
namespace ast {

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
