#pragma once

#include <list>
#include <memory>
#include <unordered_set>

#include "sir/sir.h"
#include "sir/util/iterators.h"

namespace seq {
namespace ir {
namespace analyze {

class CFBlock {
private:
  /// the in-order list of values in this block
  std::list<const Value *> values;
  /// an un-ordered list of successor blocks
  std::unordered_set<CFBlock *> successors;
  /// the block's name
  std::string name;

public:
  /// Constructs a control-flow block.
  explicit CFBlock(std::string name = "")
      : name(std::move(name)) {}

  /// @return an iterator to the first value
  auto begin() { return values.begin(); }
  /// @return an iterator beyond the last value
  auto end() { return values.end(); }
  /// @return an iterator to the first value
  auto begin() const { return values.begin(); }
  /// @return an iterator beyond the last value
  auto end() const { return values.end(); }
  /// @return a pointer to the first value
  const Value *front() const { return values.front(); }
  /// @return a pointer to the last value
  const Value *back() const { return values.back(); }

  /// Inserts a value at a given position.
  /// @param it the position
  /// @param v the new value
  /// @param an iterator to the new value
  template <typename It>
  auto insert(It it, const Value *v) { values.insert(it, v); }
  /// Inserts a value at the back.
  /// @param v the new value
  void push_back(const Value *v) { values.push_back(v); }
  /// Erases a value at the given position.
  /// @param it the position
  /// @return an iterator following the removed value
  template <typename It>
  auto erase(It it) { values.erase(it); }

  /// @return an iterator to the first successor
  auto successors_begin() { return successors.begin(); }
  /// @return an iterator beyond the last successor
  auto successors_end() { return successors.end(); }
  /// @return an iterator to the first successor
  auto successors_begin() const { return successors.begin(); }
  /// @return an iterator beyond the last successor
  auto successors_end() const { return successors.end(); }

  /// Inserts a successor at some position.
  /// @param v the new successor
  /// @return an iterator to the new successor
  auto successors_insert(CFBlock *v) { successors.insert(v); }
  /// Removes a given successor.
  /// @param v the successor to remove
  auto successors_erase(CFBlock *v) { successors.erase(v); }
};

class CFGraph {
private:
  /// owned list of blocks
  std::list<std::unique_ptr<CFBlock>> blocks;
  /// the current block
  CFBlock *cur = nullptr;

public:
  /// Constructs a control-flow graph.
  CFGraph();

  /// @return an iterator to the first block
  auto begin() { return util::dereference_adaptor(blocks.begin()); }
  /// @return an iterator beyond the last block
  auto end() { return util::dereference_adaptor(blocks.end()); }
  /// @return an iterator to the first block
  auto begin() const { return util::dereference_adaptor(blocks.begin()); }
  /// @return an iterator beyond the last block
  auto end() const { return util::dereference_adaptor(blocks.end()); }

  /// @return the entry block
  CFBlock *getEntryBlock() { return blocks.front().get(); }
  /// @return the entry block
  const CFBlock *getEntryBlock() const { return blocks.front().get(); }

  /// @return the entry block
  CFBlock *getCurrentBlock() { return cur; }
  /// @return the entry block
  const CFBlock *getCurrentBlock() const { return cur; }
  /// Sets the current block.
  /// @param v the new value
  void setCurrentBlock(CFBlock *v) { cur = v; }

  /// Creates and inserts a new block
  /// @param name the name
  /// @param setCur true if the block should be made the current one
  /// @return a newly inserted block
  CFBlock *newBlock(std::string name = "", bool setCur = true) {
    auto *ret = new CFBlock(std::move(name));
    blocks.emplace_back(ret);
    if (setCur)
      setCurrentBlock(ret);
    return ret;
  }
};

/// Builds a control-flow graph from a given function.
/// @param f the function
/// @return the control-flow graph
std::unique_ptr<CFGraph> buildCFGraph(const Func *f);

} // namespace analyze
} // namespace ir
} // namespace seq
