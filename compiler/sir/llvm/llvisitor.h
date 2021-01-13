#pragma once

#include "llvm.h"
#include "sir/sir.h"
#include <string>
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
    /// Coroutine promise (where yielded values are stored)
    llvm::Value *promise;
    /// Coroutine handle
    llvm::Value *handle;
    /// Coroutine cleanup block
    llvm::BasicBlock *cleanup;
    /// Coroutine suspend block
    llvm::BasicBlock *suspend;
    /// Coroutine exit block
    llvm::BasicBlock *exit;
  };

  struct NestableData {
    int sequenceNumber;

    NestableData() : sequenceNumber(-1) {}
  };

  struct LoopData : NestableData {
    /// Block to branch to in case of "break"
    llvm::BasicBlock *breakBlock;
    /// Block to branch to in case of "continue"
    llvm::BasicBlock *continueBlock;

    LoopData(llvm::BasicBlock *breakBlock, llvm::BasicBlock *continueBlock)
        : NestableData(), breakBlock(breakBlock), continueBlock(continueBlock) {}
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
          catchStore(nullptr), delegateDepth(nullptr), retStore(nullptr) {}
  };

  struct DebugInfo {
    /// LLVM debug info builder
    std::unique_ptr<llvm::DIBuilder> builder;
    /// Current compilation unit
    llvm::DICompileUnit *unit;
    /// Whether we are compiling in debug mode
    bool debug;
    /// Program command-line flags
    std::string flags;

    explicit DebugInfo(bool debug, const std::string &flags)
        : builder(), unit(nullptr), debug(debug), flags(flags) {}

    llvm::DIFile *getFile(const std::string &path);
  };

  /// LLVM context used for compilation
  llvm::LLVMContext context;
  /// LLVM IR builder used for constructing LLVM IR
  llvm::IRBuilder<> builder;
  /// Module we are compiling
  std::unique_ptr<llvm::Module> module;
  /// Current function we are compiling
  llvm::Function *func;
  /// Current basic block we are compiling
  llvm::BasicBlock *block;
  /// Last compiled value
  llvm::Value *value;
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
  /// Debug information
  DebugInfo db;

  llvm::DIType *
  getDITypeHelper(const types::Type *t,
                  std::unordered_map<std::string, llvm::DICompositeType *> &cache);
  void setDebugInfoForNode(const IRNode *);
  void process(IRNode *);
  void process(const IRNode *);

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
  void callStage(const PipelineFlow::Stage *stage);
  void codegenPipeline(const std::vector<const PipelineFlow::Stage *> &stages,
                       llvm::Value *syncReg, unsigned where = 0);

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
  LLVMVisitor(bool debug = false, const std::string &flags = "");

  /// Performs LLVM's module verification on the contained module.
  /// Causes an assertion failure if verification fails.
  void verify();
  /// Dumps the unoptimized module IR to a file.
  /// @param filename name of file to write IR to
  void dump(const std::string &filename = "_dump.ll");
  /// Runs optimization passes on module and writes LLVM bitcode
  /// to the specified file.
  /// @param filename name of the file to write bitcode to
  void compile(const std::string &filename);
  /// Runs optimization passes on module and executes it.
  /// @param args vector of arguments to program
  /// @param libs vector of libraries to load
  /// @param envp program environment
  void run(const std::vector<std::string> &args = {},
           const std::vector<std::string> &libs = {},
           const char *const *envp = nullptr);

  /// Get LLVM type from IR type
  /// @param t the IR type
  /// @return corresponding LLVM type
  llvm::Type *getLLVMType(const types::Type *t);
  /// Get the LLVM debug info type from the IR type
  /// @param t the IR type
  /// @return corresponding LLVM DI type
  llvm::DIType *getDIType(const types::Type *t);

  void visit(IRModule *) override;
  void visit(BodiedFunc *) override;
  void visit(ExternalFunc *) override;
  void visit(InternalFunc *) override;
  void visit(LLVMFunc *) override;
  void visit(Var *) override;
  void visit(VarValue *) override;
  void visit(PointerValue *) override;
  void visit(ValueProxy *) override;

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
  void visit(PipelineFlow *) override;

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
