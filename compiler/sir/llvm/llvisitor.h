#pragma once

#include "sir/sir.h"
#include "util/llvm.h"
#include <unordered_map>
#include <vector>

namespace seq {
namespace ir {

class LLVMVisitor : public util::SIRVisitor {
private:
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

  struct NestableData {
    int sequenceNumber;

    NestableData() : sequenceNumber(-1){};
  };

  struct LoopData : NestableData {
    llvm::BasicBlock *breakBlock;
    llvm::BasicBlock *continueBlock;

    LoopData(llvm::BasicBlock *breakBlock, llvm::BasicBlock *continueBlock)
        : NestableData(), breakBlock(breakBlock), continueBlock(continueBlock){};
  };

  struct TryCatchData : NestableData {
    /// Possible try-catch states when reaching finally block
    enum State { NOT_THROWN = 0, THROWN, CAUGHT, RETURN, BREAK, CONTINUE };
    /// Exception block
    llvm::BasicBlock *exceptionBlock;
    /// Exception route block
    llvm::BasicBlock *exceptionRouteBlock;
    /// Finally start block
    llvm::BasicBlock *finallyBlock;
    /// Try-catch catch types
    std::vector<const types::Type *> catchTypes;
    /// Try-catch handlers, corresponding to catch types
    std::vector<llvm::BasicBlock *> handlers;
    /// Exception state flag (see "State")
    llvm::Value *excFlag;
    /// Storage for caught exception
    llvm::Value *catchStore;
    /// How far to delegate up the finally chain
    llvm::Value *delegateDepth;
    /// Storage for postponed return
    llvm::Value *retStore;

    TryCatchData()
        : NestableData(), exceptionBlock(nullptr), exceptionRouteBlock(nullptr),
          finallyBlock(nullptr), catchTypes(), handlers(), excFlag(nullptr),
          catchStore(nullptr), delegateDepth(nullptr), retStore(nullptr){};
  };

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
  std::vector<LoopData> loops;
  /// Try-catch data stack
  std::vector<TryCatchData> trycatch;
  /// Whether we are compiling in debug mode
  bool debug;

  template <typename T> void process(T *x) { x->accept(*this); }

  /// GC allocation functions
  llvm::Function *makeAllocFunc(bool atomic);
  /// Personality function for exception handling
  llvm::Function *makePersonalityFunc();
  /// Exception allocation function
  llvm::Function *makeExcAllocFunc();
  /// Exception throw function
  llvm::Function *makeThrowFunc();
  /// Program termination function
  llvm::Function *makeTerminateFunc();

  // Try-catch types and utilities
  llvm::StructType *getTypeInfoType();
  llvm::StructType *getPadType();
  llvm::StructType *getExceptionType();
  llvm::GlobalVariable *getTypeIdxVar(const std::string &name);
  llvm::GlobalVariable *getTypeIdxVar(const types::Type *catchType);
  int getTypeIdx(const types::Type *catchType = nullptr);

  // General function helpers
  llvm::Value *call(llvm::Value *callee, llvm::ArrayRef<llvm::Value *> args);
  void makeLLVMFunction(Func *);
  void makeYield(llvm::Value *value = nullptr, bool finalYield = false);
  std::string buildLLVMCodeString(LLVMFunc *);

  // Loop and try-catch state
  void enterLoop(LoopData data);
  void exitLoop();
  void enterTryCatch(TryCatchData data);
  void exitTryCatch();
  TryCatchData *getInnermostTryCatchBeforeLoop();

  // LLVM passes
  void applyDebugTransformations();
  void applyGCTransformations();
  void runLLVMOptimizationPasses();
  void runLLVMPipeline();

public:
  LLVMVisitor(bool debug = false);

  void verify();
  void dump(const std::string &filename = "_dump.ll");
  void compile(const std::string &outname);
  void run(const std::vector<std::string> &args = {},
           const std::vector<std::string> &libs = {},
           const char *const *envp = nullptr);
  llvm::Type *getLLVMType(const types::Type *x);

  void visit(IRModule *) override;
  void visit(BodiedFunc *) override;
  void visit(ExternalFunc *) override;
  void visit(InternalFunc *) override;
  void visit(LLVMFunc *) override;
  void visit(Var *) override;
  void visit(VarValue *) override;
  void visit(PointerValue *) override;
  void visit(ValueProxy *) override;

  void visit(const types::IntType *) override;
  void visit(const types::FloatType *) override;
  void visit(const types::BoolType *) override;
  void visit(const types::ByteType *) override;
  void visit(const types::VoidType *) override;
  void visit(const types::RecordType *) override;
  void visit(const types::RefType *) override;
  void visit(const types::FuncType *) override;
  void visit(const types::OptionalType *) override;
  void visit(const types::ArrayType *) override;
  void visit(const types::PointerType *) override;
  void visit(const types::GeneratorType *) override;
  void visit(const types::IntNType *) override;

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
  void visit(TypePropertyInstr *) override;
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
