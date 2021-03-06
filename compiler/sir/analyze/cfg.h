#pragma once

#include <list>
#include <memory>
#include <unordered_set>

#include "sir/sir.h"
#include "sir/util/iterators.h"

namespace seq {
namespace ir {
namespace analyze {

class CFBlock : public IdMixin {
private:
  /// the in-order list of values in this block
  std::list<const Value *> values;
  /// an un-ordered list of successor blocks
  std::unordered_set<CFBlock *> successors;
  /// the block's name
  std::string name;

public:
  /// Constructs a control-flow block.
  explicit CFBlock(std::string name = "") : name(std::move(name)) {}

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
  template <typename It> auto insert(It it, const Value *v) { values.insert(it, v); }
  /// Inserts a value at the back.
  /// @param v the new value
  void push_back(const Value *v) { values.push_back(v); }
  /// Erases a value at the given position.
  /// @param it the position
  /// @return an iterator following the removed value
  template <typename It> auto erase(It it) { values.erase(it); }

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

class SyntheticAssignInstr : public AcceptorExtend<SyntheticAssignInstr, Instr> {
public:
  enum Kind { UNKNOWN, KNOWN, NEXT_VALUE };

private:
  /// the left-hand side
  Var *lhs;
  /// the kind of synthetic assignment
  Kind kind;
  /// any argument to the synthetic assignment
  Value *arg = nullptr;

public:
  static const char NodeId;

  /// Constructs a synthetic assignment.
  /// @param lhs the variable being assigned
  /// @param arg the argument
  /// @param k the kind of assignment
  /// @param name the name of the instruction
  explicit SyntheticAssignInstr(Var *lhs, Value *arg, Kind k = KNOWN,
                                std::string name = "")
      : AcceptorExtend(std::move(name)), lhs(lhs), kind(k), arg(arg) {}
  /// Constructs an unknown synthetic assignment.
  /// @param lhs the variable being assigned
  /// @param name the name of the instruction
  explicit SyntheticAssignInstr(Var *lhs, std::string name = "")
      : SyntheticAssignInstr(lhs, nullptr, UNKNOWN, std::move(name)) {}

  /// @return the variable being assigned
  Var *getLhs() { return lhs; }
  /// @return the variable being assigned
  const Var *getLhs() const { return lhs; }
  /// Sets the variable being assigned.
  /// @param v the variable
  void setLhs(Var *v) { lhs = v; }

  /// @return the argument
  Value *getArg() { return arg; }
  /// @return the argument
  const Value *getArg() const { return arg; }
  /// Sets the argument.
  /// @param v the new value
  void setArg(Value *v) { arg = v; }

  /// @return the kind of synthetic assignment
  Kind getKind() const { return kind; }
  /// Sets the kind.
  /// @param k the new value
  void setKind(Kind k) { kind = k; }

protected:
  std::vector<Value *> doGetUsedValues() const override { return {arg}; }
  int doReplaceUsedValue(int id, Value *newValue) override;

  std::vector<Var *> doGetUsedVariables() const override { return {lhs}; }
  int doReplaceUsedVariable(int id, Var *newVar) override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
  Value *doClone() const override;
};

class SyntheticPhiInstr : public AcceptorExtend<SyntheticAssignInstr, Instr> {
public:
  class Predecessor {
  private:
    /// the predecessor block
    CFBlock *pred;
    /// the value
    Value *result;

  public:
    /// Constructs a predecessor.
    /// @param pred the predecessor block
    /// @param result the result of this predecessor.
    Predecessor(CFBlock *pred, Value *result) : pred(pred), result(result) {}

    /// @return the predecessor block
    CFBlock *getPred() { return pred; }
    /// @return the predecessor block
    const CFBlock *getPred() const { return pred; }
    /// Sets the predecessor.
    /// @param v the new value
    void setPred(CFBlock *v) { pred = v; }

    /// @return the result
    Value *getResult() { return result; }
    /// @return the result
    const Value *getResult() const { return result; }
    /// Sets the result
    /// @param v the new value
    void setResult(Value *v) { result = v; }
  };

private:
  std::list<Predecessor> preds;

public:
  static const char NodeId;

  SyntheticPhiInstr(std::string name = "") : AcceptorExtend(std::move(name)) {}

  /// @return an iterator to the first instruction/flow
  auto begin() { return preds.begin(); }
  /// @return an iterator beyond the last instruction/flow
  auto end() { return preds.end(); }
  /// @return an iterator to the first instruction/flow
  auto begin() const { return preds.begin(); }
  /// @return an iterator beyond the last instruction/flow
  auto end() const { return preds.end(); }

  /// @return a pointer to the first instruction/flow
  Predecessor &front() { return preds.front(); }
  /// @return a pointer to the last instruction/flow
  Predecessor &back() { return preds.back(); }
  /// @return a pointer to the first instruction/flow
  const Predecessor &front() const { return preds.front(); }
  /// @return a pointer to the last instruction/flow
  const Predecessor &back() const { return preds.back(); }

  /// Inserts a predecessor.
  /// @param pos the position
  /// @param v the predecessor
  /// @return an iterator to the newly added predecessor
  template <typename It> auto insert(It pos, Predecessor v) {
    return preds.insert(pos, v);
  }
  /// Appends an predecessor.
  /// @param v the predecessor
  void push_back(Predecessor v) { preds.push_back(v); }

  /// Erases the item at the supplied position.
  /// @param pos the position
  /// @return the iterator beyond the removed predecessor
  template <typename It> auto erase(It pos) { return preds.erase(pos); }

  /// Emplaces a predecessor.
  /// @param args the args
  template <typename... Args> void emplace_back(Args &&... args) {
    preds.emplace_back(std::forward<Args>(args)...);
  }

protected:
  std::vector<Value *> doGetUsedValues() const override;
  int doReplaceUsedValue(int id, Value *newValue) override;

private:
  std::ostream &doFormat(std::ostream &os) const override;
  Value *doClone() const override;
};

class CFGraph {
private:
  /// owned list of blocks
  std::list<std::unique_ptr<CFBlock>> blocks;
  /// the current block
  CFBlock *cur = nullptr;
  /// the function being analyzed
  const Func *func;
  /// a list of synthetic values
  std::list<std::unique_ptr<Value>> syntheticValues;

public:
  /// Constructs a control-flow graph.
  explicit CFGraph(const Func *f);

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

  /// @return the function
  const Func *getFunc() const { return func; }
  /// Sets the function.
  /// @param f the new value
  void setFunc(Func *f) { func = f; }

  /// Creates and inserts a new block
  /// @param name the name
  /// @param setCur true if the block should be made the current one
  /// @return a newly inserted block
  CFBlock *newBlock(std::string name = "", bool setCur = false) {
    auto *ret = new CFBlock(std::move(name));
    blocks.emplace_back(ret);
    if (setCur)
      setCurrentBlock(ret);
    return ret;
  }

  template <typename NodeType, typename... Args> NodeType *N(Args &&... args) {
    auto *ret = new NodeType(std::forward<Args>(args)...);
    syntheticValues.emplace_back(ret);
    ret->setModule(func->getModule());
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
