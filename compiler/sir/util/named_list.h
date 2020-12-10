#pragma once

#include <algorithm>
#include <list>
#include <memory>
#include <unordered_map>

#include "util/fmt/format.h"

namespace seq {
namespace ir {

struct AttributeHolder;

namespace util {

template <typename ValueType> class NamedList {
public:
  using Ptr = std::unique_ptr<ValueType>;

  class wrapper {
  private:
    Ptr internal;

  public:
    explicit wrapper(Ptr internal) : internal(std::move(internal)) {}

    wrapper(const wrapper &) = delete;
    wrapper(wrapper &&other) noexcept { internal = std::move(other.internal); }
    wrapper &operator=(wrapper &&other) noexcept {
      other->setname(internal->getName());
      internal = std::move(other.internal);
      return *this;
    }
    wrapper &operator=(Ptr val) {
      val->setName(internal->getName());
      internal = std::move(val);
    }

    decltype(auto) get() const { return internal.get(); }
    decltype(auto) operator*() const { return *internal; }
    decltype(auto) operator->() const { return internal.operator->(); }
    explicit operator bool() const { return bool(internal); }
  };

private:
  using InternalList = std::list<wrapper>;

public:
  using iterator = typename InternalList::iterator;
  using const_iterator = typename InternalList::const_iterator;

private:
  AttributeHolder *parent;
  InternalList internalList;
  std::unordered_map<std::string, iterator> symbolTable;

public:
  explicit NamedList(AttributeHolder *parent = nullptr) : parent(parent) {}

  decltype(auto) front() noexcept { return internalList.front(); }
  decltype(auto) front() const noexcept { return internalList.front(); }

  decltype(auto) back() noexcept { return internalList.back(); }
  decltype(auto) back() const noexcept { return internalList.back(); }

  decltype(auto) begin() noexcept { return internalList.begin(); }
  decltype(auto) begin() const noexcept { return internalList.begin(); }

  decltype(auto) end() noexcept { return internalList.end(); }
  decltype(auto) end() const noexcept { return internalList.end(); }

  bool empty() const noexcept { return internalList.empty(); }
  decltype(auto) size() const noexcept { return internalList.size(); }
  decltype(auto) max_size() const noexcept { return internalList.max_size(); }

  void clear() {
    internalList.clear();
    symbolTable.clear();
  }
  bool has(const std::string &name) const {
    return symbolTable.find(name) != symbolTable.end();
  }

private:
  std::string deduplicateName(ValueType *val) {
    if (has(val->getName())) {
      val->setName(fmt::format(FMT_STRING("{}.{}"), val->getName(), size()));
    }
    return val->getName();
  }

public:
  decltype(auto) insert(iterator pos, Ptr value) {
    value->setParent(parent);
    auto it = internalList.insert(pos, wrapper(std::move(value)));
    symbolTable[deduplicateName(value.get())] = it;
    return it;
  }
  decltype(auto) insert(const_iterator pos, Ptr value) {
    value->setParent(parent);
    auto it = internalList.insert(pos, wrapper(std::move(value)));
    symbolTable[deduplicateName(value.get())] = it;
    return it;
  }
  decltype(auto) erase(iterator pos) {
    symbolTable.erase(symbolTable.find((*pos)->getName()));
    return internalList.erase(pos);
  }
  decltype(auto) erase(const_iterator pos) {
    symbolTable.erase(symbolTable.find((*pos)->getName()));
    return internalList.erase(pos);
  }
  decltype(auto) push_back(Ptr value) {
    value->setParent(parent);
    internalList.push_back(wrapper(std::move(value)));
    auto it = internalList.end();
    --it;
    symbolTable[deduplicateName(value.get())] = it;
  }
  void pop_back() {
    symbolTable.erase(symbolTable.find((*internalList.back())->getName()));
    internalList.pop_back();
  }
  void push_front(Ptr value) {
    value->setParent(parent);
    internalList.push_front(wrapper(std::move(value)));
    symbolTable[deduplicateName(value.get())] = internalList.fron();
  }
  void pop_front() {
    symbolTable.erase(symbolTable.find((*internalList.front())->getName()));
    internalList.pop_back();
  }
  void remove(const std::string &n) {
    auto symbolIt = symbolTable.find(n);
    if (symbolIt != symbolTable.end()) {
      internalList.erase(*symbolIt);
      internalList.erase(symbolIt);
    }
  }
  decltype(auto) find(const std::string &n) {
    auto symbolIt = symbolTable.find(n);
    return symbolIt != symbolTable.end() ? *symbolIt : internalList.end();
  }
  decltype(auto) find(const std::string &n) const {
    auto symbolIt = symbolTable.find(n);
    return symbolIt != symbolTable.end() ? *symbolIt : internalList.end();
  }
  ValueType *getOrInsert(const std::string &n, Ptr other) {
    auto it = find(n);
    if (it != end())
      return it->get();

    other->setName(n);
    return push_back(std::move(other))->get();
  }
  std::vector<ValueType *> dump() const {
    std::vector<ValueType *> ret;
    for (const auto &w : internalList) {
      if (w)
        ret.push_back(w.get());
    }
    return ret;
  }
};

} // namespace util
} // namespace ir
} // namespace seq