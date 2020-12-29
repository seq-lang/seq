#pragma once

#include "sir/sir.h"
#include "util/llvm.h"
#include <stack>
#include <unordered_map>

namespace seq {
namespace ir {

template <typename V> using CacheBase = std::unordered_map<int, V *>;
template <typename K, typename V> class Cache : public CacheBase<V> {
public:
  using CacheBase<V>::CacheBase;

  V *operator[](const K *key) {
    auto it = CacheBase<V>::find(key->getId());
    return (it != CacheBase<V>::end()) ? it->second : nullptr;
  }

  void insert(const K *key, V *value) { CacheBase<V>::emplace(key->getId(), value); }
};

struct CoroData {
  // coroutine-specific data
  llvm::Value *promise;
  llvm::Value *handle;
  llvm::BasicBlock *cleanup;
  llvm::BasicBlock *suspend;
  llvm::BasicBlock *exit;
};

struct LoopData {
  llvm::BasicBlock *breakBlock;
  llvm::BasicBlock *continueBlock;
};

struct InternalFuncData {
  using CodegenFunc =
      std::function<llvm::Value *(types::Type *, std::vector<llvm::Value *>)>;
  std::string name;
  CodegenFunc codegen;
};

class LLVMVisitor : public util::SIRVisitor {
private:
  /// LLVM context used for compilation
  llvm::LLVMContext context;
  /// Module we are compiling
  std::unique_ptr<llvm::Module> module;
  /// Current function we are compiling
  llvm::Function *func;
  /// Current basic block we are compiling
  llvm::BasicBlock *block;
  /// Last compiled value
  llvm::Value *value;
  /// LLVM type that was just translated from the IR type
  llvm::Type *type;
  /// LLVM values corresponding to IR variables
  Cache<Var, llvm::Value> vars;
  /// LLVM functions corresponding to IR functions
  Cache<Func, llvm::Function> funcs;
  /// Coroutine data, if current function is a coroutine
  CoroData coro;
  /// Loop data stack, containing break/continue blocks
  std::stack<LoopData> loops;
  /// Internal functions
  std::vector<InternalFuncData> internalFuncs;
  /// Whether we are compiling in debug mode
  bool debug;

  template <typename T> void process(const T &x) { x->accept(*this); }

  llvm::StructType *getTypeInfoType();
  llvm::StructType *getPadType();
  llvm::StructType *getExceptionType();
  llvm::GlobalVariable *getTypeIdxVar(const std::string &name);
  llvm::GlobalVariable *getTypeIdxVar(types::Type *catchType);

  llvm::Value *call(llvm::Value *callee, llvm::ArrayRef<llvm::Value *> args);
  void initInternalFuncs();
  void makeLLVMFunction(Func *);
  void makeYield(llvm::Value *value = nullptr, bool finalYield = false);
  void enterLoop(llvm::BasicBlock *breakBlock, llvm::BasicBlock *continueBlock);
  void exitLoop();
  std::string buildLLVMCodeString(LLVMFunc *);

public:
  LLVMVisitor(bool debug = false);

  void validate();
  llvm::Type *getLLVMType(types::Type *x);

  void visit(IRModule *) override;
  void visit(BodiedFunc *) override;
  void visit(ExternalFunc *) override;
  void visit(InternalFunc *) override;
  void visit(LLVMFunc *) override;
  void visit(Var *) override;
  void visit(VarValue *) override;
  void visit(PointerValue *) override;
  void visit(ValueProxy *) override;

  void visit(types::IntType *) override;
  void visit(types::FloatType *) override;
  void visit(types::BoolType *) override;
  void visit(types::ByteType *) override;
  void visit(types::VoidType *) override;
  void visit(types::RecordType *) override;
  void visit(types::RefType *) override;
  void visit(types::FuncType *) override;
  void visit(types::OptionalType *) override;
  void visit(types::ArrayType *) override;
  void visit(types::PointerType *) override;
  void visit(types::GeneratorType *) override;
  void visit(types::IntNType *) override;

  void visit(IntConstant *) override;
  void visit(FloatConstant *) override;
  void visit(BoolConstant *) override;
  void visit(StringConstant *) override;

  void visit(SeriesFlow *) override;
  void visit(IfFlow *) override;
  void visit(WhileFlow *) override;
  void visit(ForFlow *) override;
  void visit(TryCatchFlow *) override;
  void visit(UnorderedFlow *) override;

  void visit(AssignInstr *) override;
  void visit(ExtractInstr *) override;
  void visit(InsertInstr *) override;
  void visit(CallInstr *) override;
  void visit(StackAllocInstr *) override;
  void visit(YieldInInstr *) override;
  void visit(TernaryInstr *) override;
  void visit(BreakInstr *) override;
  void visit(ContinueInstr *) override;
  void visit(ReturnInstr *) override;
  void visit(YieldInstr *) override;
  void visit(ThrowInstr *) override;
  void visit(FlowInstr *) override;
};

} // namespace ir
} // namespace seq
