#include "sir/llvm/llvisitor.h"
#include "llvm/CodeGen/CommandFlags.def"
#include <algorithm>
#include <utility>

extern llvm::StructType *IdentTy;
extern llvm::FunctionType *Kmpc_MicroTy;
extern llvm::Constant *DefaultOpenMPPSource;
extern llvm::Constant *DefaultOpenMPLocation;
extern llvm::PointerType *KmpRoutineEntryPtrTy;
extern llvm::Type *getOrCreateIdentTy(llvm::Module *module);
extern llvm::Value *getOrCreateDefaultLocation(llvm::Module *module);
extern llvm::PointerType *getIdentTyPointerTy();
extern llvm::FunctionType *getOrCreateKmpc_MicroTy(llvm::LLVMContext &context);
extern llvm::PointerType *getKmpc_MicroPointerTy(llvm::LLVMContext &context);
extern llvm::cl::opt<bool> fastOpenMP;

extern "C" void seq_gc_add_roots(void *start, void *end);
extern "C" void seq_gc_remove_roots(void *start, void *end);
extern "C" void seq_add_symbol(void *addr, const std::string &symbol);

namespace seq {
namespace ir {
namespace {
void resetOMPABI() {
  IdentTy = nullptr;
  Kmpc_MicroTy = nullptr;
  DefaultOpenMPPSource = nullptr;
  DefaultOpenMPLocation = nullptr;
  KmpRoutineEntryPtrTy = nullptr;
}

std::string getNameForFunction(Func *x) {
  if (auto *externalFunc = cast<ExternalFunc>(x)) {
    return x->getUnmangledName();
  } else {
    return x->referenceString();
  }
}

llvm::TargetMachine *getTargetMachine(llvm::Triple triple, llvm::StringRef cpuStr,
                                      llvm::StringRef featuresStr,
                                      const llvm::TargetOptions &options) {
  std::string err;
  const llvm::Target *target = llvm::TargetRegistry::lookupTarget(MArch, triple, err);

  if (!target)
    return nullptr;

  return target->createTargetMachine(triple.getTriple(), cpuStr, featuresStr, options,
                                     getRelocModel(), getCodeModel(),
                                     llvm::CodeGenOpt::Aggressive);
}

/**
 * Simple extension of LLVM's SectionMemoryManager which catches data section
 * allocations and registers them with the GC. This allows the GC to know not
 * to collect globals even in JIT mode.
 */
class BoehmGCMemoryManager : public llvm::SectionMemoryManager {
private:
  /// Vector of (start, end) address pairs registered with GC.
  std::vector<std::pair<void *, void *>> roots;

  uint8_t *allocateDataSection(uintptr_t size, unsigned alignment, unsigned sectionID,
                               llvm::StringRef sectionName, bool isReadOnly) override {
    uint8_t *result = SectionMemoryManager::allocateDataSection(
        size, alignment, sectionID, sectionName, isReadOnly);
    void *start = result;
    void *end = result + size;
    seq_gc_add_roots(start, end);
    roots.emplace_back(start, end);
    return result;
  }

public:
  BoehmGCMemoryManager() : SectionMemoryManager(), roots() {}

  ~BoehmGCMemoryManager() override {
    for (const auto &root : roots) {
      seq_gc_remove_roots(root.first, root.second);
    }
  }
};
} // namespace

LLVMVisitor::LLVMVisitor(bool debug)
    : util::SIRVisitor(), context(), module(), func(nullptr), block(nullptr),
      value(nullptr), type(nullptr), vars(), funcs(), coro(), loops(), trycatch(),
      debug(debug) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  resetOMPABI();
}

void LLVMVisitor::verify() {
  const bool broken = llvm::verifyModule(*module.get(), &llvm::errs());
  assert(!broken);
}

void LLVMVisitor::dump(const std::string &filename) {
  auto fo = fopen(filename.c_str(), "w");
  llvm::raw_fd_ostream fout(fileno(fo), true);
  fout << *module.get();
  fout.close();
}

void LLVMVisitor::applyDebugTransformations() {
  if (!debug)
    return;
  // remove tail calls and fix linkage for stack traces
  for (auto &f : *module) {
    f.setLinkage(llvm::GlobalValue::ExternalLinkage);
    if (f.hasFnAttribute(llvm::Attribute::AttrKind::AlwaysInline))
      f.removeFnAttr(llvm::Attribute::AttrKind::AlwaysInline);
    f.addFnAttr(llvm::Attribute::AttrKind::NoInline);
    f.setHasUWTable();
    f.addFnAttr("no-frame-pointer-elim", "true");
    f.addFnAttr("no-frame-pointer-elim-non-leaf");
    f.addFnAttr("no-jump-tables", "false");

    for (auto &block : f.getBasicBlockList()) {
      for (auto &inst : block) {
        if (auto *call = llvm::dyn_cast<llvm::CallInst>(&inst)) {
          call->setTailCall(false);
        }
      }
    }
  }
}

void LLVMVisitor::applyGCTransformations() {
  auto *addRoots = llvm::cast<llvm::Function>(module->getOrInsertFunction(
      "seq_gc_add_roots", llvm::Type::getVoidTy(context),
      llvm::Type::getInt8PtrTy(context), llvm::Type::getInt8PtrTy(context)));
  addRoots->setDoesNotThrow();

  // insert add_roots calls where needed
  for (auto &f : *module) {
    for (auto &block : f.getBasicBlockList()) {
      for (auto &inst : block) {
        if (auto *call = dyn_cast<CallInst>(&inst)) {
          if (auto *g = call->getCalledFunction()) {
            // tell GC about OpenMP's allocation
            if (g->getName() == "__kmpc_omp_task_alloc") {
              llvm::Value *taskSize = call->getArgOperand(3);
              llvm::Value *sharedSize = call->getArgOperand(4);
              llvm::IRBuilder<> builder(call->getNextNode());
              llvm::Value *baseOffset = builder.CreateSub(taskSize, sharedSize);
              llvm::Value *ptr = builder.CreateBitCast(call, builder.getInt8PtrTy());
              llvm::Value *lo = builder.CreateGEP(ptr, baseOffset);
              llvm::Value *hi = builder.CreateGEP(ptr, taskSize);
              builder.CreateCall(addRoots, {lo, hi});
            }
          }
        }
      }
    }
  }
}

void LLVMVisitor::runLLVMOptimizationPasses() {
  using namespace llvm::orc;
  applyDebugTransformations();
  auto pm = std::make_unique<llvm::legacy::PassManager>();
  auto fpm = std::make_unique<llvm::legacy::FunctionPassManager>(module.get());

  llvm::Triple moduleTriple(module->getTargetTriple());
  std::string cpuStr, featuresStr;
  llvm::TargetMachine *machine = nullptr;
  const llvm::TargetOptions options = InitTargetOptionsFromCodeGenFlags();
  llvm::TargetLibraryInfoImpl tlii(moduleTriple);
  pm->add(new llvm::TargetLibraryInfoWrapperPass(tlii));

  if (moduleTriple.getArch()) {
    cpuStr = getCPUStr();
    featuresStr = getFeaturesStr();
    machine = getTargetMachine(moduleTriple, cpuStr, featuresStr, options);
  }

  std::unique_ptr<llvm::TargetMachine> tm(machine);
  setFunctionAttributes(cpuStr, featuresStr, *module);
  pm->add(llvm::createTargetTransformInfoWrapperPass(tm ? tm->getTargetIRAnalysis()
                                                        : llvm::TargetIRAnalysis()));
  fpm->add(llvm::createTargetTransformInfoWrapperPass(tm ? tm->getTargetIRAnalysis()
                                                         : llvm::TargetIRAnalysis()));

  if (tm) {
    auto &ltm = dynamic_cast<llvm::LLVMTargetMachine &>(*tm);
    llvm::Pass *tpc = ltm.createPassConfig(*pm);
    pm->add(tpc);
  }

  unsigned optLevel = 3;
  unsigned sizeLevel = 0;
  llvm::PassManagerBuilder builder;
  static llvm::OpenMPABI omp;
  builder.tapirTarget = &omp;

  if (!debug) {
    builder.OptLevel = optLevel;
    builder.SizeLevel = sizeLevel;
    builder.Inliner = llvm::createFunctionInliningPass(optLevel, sizeLevel, false);
    builder.DisableUnitAtATime = false;
    builder.DisableUnrollLoops = false;
    builder.LoopVectorize = true;
    builder.SLPVectorize = true;
  }

  if (tm)
    tm->adjustPassManager(builder);

  llvm::addCoroutinePassesToExtensionPoints(builder);
  builder.populateModulePassManager(*pm);
  builder.populateFunctionPassManager(*fpm);

  fpm->doInitialization();
  for (llvm::Function &f : *module)
    fpm->run(f);
  fpm->doFinalization();
  pm->run(*module);
  applyDebugTransformations();
}

void LLVMVisitor::runLLVMPipeline() {
  verify();
  runLLVMOptimizationPasses();
  applyGCTransformations();
  if (!debug) {
    runLLVMOptimizationPasses();
  }
  verify();
}

void LLVMVisitor::compile(const std::string &outname) {
  runLLVMPipeline();
  std::error_code err;
  llvm::raw_fd_ostream stream(outname, err, llvm::sys::fs::F_None);
  llvm::WriteBitcodeToFile(module.get(), stream);
  if (err) {
    throw std::runtime_error(err.message());
  }
}

void LLVMVisitor::run(const std::vector<std::string> &args,
                      const std::vector<std::string> &libs, const char *const *envp) {
  runLLVMPipeline();

  std::vector<std::string> functionNames;
  if (debug) {
    for (auto &f : *module) {
      functionNames.push_back(f.getName());
    }
  }

  llvm::Function *main = module->getFunction("main");
  llvm::EngineBuilder EB(std::move(module));
  EB.setMCJITMemoryManager(std::make_unique<BoehmGCMemoryManager>());
  EB.setUseOrcMCJITReplacement(true);
  llvm::ExecutionEngine *eng = EB.create();

  std::string err;
  for (auto &lib : libs) {
    if (llvm::sys::DynamicLibrary::LoadLibraryPermanently(lib.c_str(), &err)) {
      throw std::runtime_error(err);
    }
  }

  if (debug) {
    for (auto &name : functionNames) {
      void *addr = eng->getPointerToNamedFunction(name, /*AbortOnFailure=*/false);
      if (addr)
        seq_add_symbol(addr, name);
    }
  }

  eng->runFunctionAsMain(main, args, envp);
  delete eng;
}

llvm::Type *LLVMVisitor::getLLVMType(const types::Type *x) {
  process(x);
  return type;
}

llvm::Function *LLVMVisitor::makeAllocFunc(bool atomic) {
  auto *f = llvm::cast<llvm::Function>(module->getOrInsertFunction(
      atomic ? "seq_alloc_atomic" : "seq_alloc", llvm::Type::getInt8PtrTy(context),
      llvm::Type::getInt64Ty(context)));
  f->setDoesNotThrow();
  f->setReturnDoesNotAlias();
  f->setOnlyAccessesInaccessibleMemory();
  return f;
}

llvm::Function *LLVMVisitor::makePersonalityFunc() {
  return llvm::cast<llvm::Function>(module->getOrInsertFunction(
      "seq_personality", llvm::Type::getInt32Ty(context),
      llvm::Type::getInt32Ty(context), llvm::Type::getInt32Ty(context),
      llvm::Type::getInt64Ty(context), llvm::Type::getInt8PtrTy(context),
      llvm::Type::getInt8PtrTy(context)));
}

llvm::Function *LLVMVisitor::makeExcAllocFunc() {
  auto *f = llvm::cast<llvm::Function>(module->getOrInsertFunction(
      "seq_alloc_exc", llvm::Type::getInt8PtrTy(context),
      llvm::Type::getInt32Ty(context), llvm::Type::getInt8PtrTy(context)));
  f->setDoesNotThrow();
  return f;
}

llvm::Function *LLVMVisitor::makeThrowFunc() {
  auto *f = llvm::cast<llvm::Function>(module->getOrInsertFunction(
      "seq_throw", llvm::Type::getVoidTy(context), llvm::Type::getInt8PtrTy(context)));
  f->setDoesNotReturn();
  return f;
}

llvm::Function *LLVMVisitor::makeTerminateFunc() {
  auto *f = llvm::cast<llvm::Function>(
      module->getOrInsertFunction("seq_terminate", llvm::Type::getVoidTy(context),
                                  llvm::Type::getInt8PtrTy(context)));
  f->setDoesNotReturn();
  return f;
}

llvm::StructType *LLVMVisitor::getTypeInfoType() {
  return llvm::StructType::get(llvm::Type::getInt32Ty(context));
}

llvm::StructType *LLVMVisitor::getPadType() {
  return llvm::StructType::get(llvm::Type::getInt8PtrTy(context),
                               llvm::Type::getInt32Ty(context));
}

llvm::StructType *LLVMVisitor::getExceptionType() {
  return llvm::StructType::get(getTypeInfoType(), llvm::Type::getInt8PtrTy(context));
}

namespace {
int typeIdxLookup(const std::string &name) {
  static std::unordered_map<std::string, int> cache;
  static int next = 1000;
  if (name.empty())
    return 0;
  auto it = cache.find(name);
  if (it != cache.end()) {
    return it->second;
  } else {
    const int myID = next++;
    cache[name] = myID;
    return myID;
  }
}
} // namespace

llvm::GlobalVariable *LLVMVisitor::getTypeIdxVar(const std::string &name) {
  auto *typeInfoType = getTypeInfoType();
  const std::string typeVarName = "seq.typeidx." + (name.empty() ? "<all>" : name);
  llvm::GlobalVariable *tidx = module->getGlobalVariable(typeVarName);
  int idx = typeIdxLookup(name);
  if (!tidx)
    tidx = new llvm::GlobalVariable(
        *module.get(), typeInfoType, true, llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantStruct::get(
            typeInfoType, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context),
                                                 (uint64_t)idx, false)),
        typeVarName);
  return tidx;
}

llvm::GlobalVariable *LLVMVisitor::getTypeIdxVar(const types::Type *catchType) {
  return getTypeIdxVar(catchType ? catchType->getName() : "");
}

int LLVMVisitor::getTypeIdx(const types::Type *catchType) {
  return typeIdxLookup(catchType ? catchType->getName() : "");
}

llvm::Value *LLVMVisitor::call(llvm::Value *callee,
                               llvm::ArrayRef<llvm::Value *> args) {
  llvm::IRBuilder<> builder(block);
  if (trycatch.empty()) {
    return builder.CreateCall(callee, args);
  } else {
    auto *normalBlock = llvm::BasicBlock::Create(context, "invoke.normal", func);
    auto *unwindBlock = trycatch.back().exceptionBlock;
    auto *result = builder.CreateInvoke(callee, normalBlock, unwindBlock, args);
    block = normalBlock;
    return result;
  }
}

static int nextSequenceNumber = 0;

void LLVMVisitor::enterLoop(LoopData data) {
  loops.push_back(std::move(data));
  loops.back().sequenceNumber = nextSequenceNumber++;
}

void LLVMVisitor::exitLoop() {
  assert(!loops.empty());
  loops.pop_back();
}

void LLVMVisitor::enterTryCatch(TryCatchData data) {
  trycatch.push_back(std::move(data));
  trycatch.back().sequenceNumber = nextSequenceNumber++;
}

void LLVMVisitor::exitTryCatch() {
  assert(!trycatch.empty());
  trycatch.pop_back();
}

LLVMVisitor::TryCatchData *LLVMVisitor::getInnermostTryCatchBeforeLoop() {
  if (!trycatch.empty() &&
      (loops.empty() || trycatch.back().sequenceNumber > loops.back().sequenceNumber))
    return &trycatch.back();
  return nullptr;
}

/*
 * General values, module, functions, vars
 */

void LLVMVisitor::visit(IRModule *x) {
  module = std::make_unique<llvm::Module>("seq", context);
  module->setTargetTriple(
      llvm::EngineBuilder().selectTarget()->getTargetTriple().str());
  module->setDataLayout(llvm::EngineBuilder().selectTarget()->createDataLayout());

  // args variable
  Var *argVar = x->getArgVar();
  llvm::Type *argVarType = getLLVMType(argVar->getType());
  auto *argStorage = new llvm::GlobalVariable(
      *module.get(), argVarType, false, llvm::GlobalValue::PrivateLinkage,
      llvm::Constant::getNullValue(argVarType), argVar->getName());
  vars.insert(argVar, argStorage);

  // set up global variables and initialize functions
  for (auto *var : *x) {
    if (auto *f = cast<Func>(var)) {
      makeLLVMFunction(f);
      funcs.insert(f, func);
    } else {
      llvm::Type *llvmType = getLLVMType(var->getType());
      auto *storage = new llvm::GlobalVariable(
          *module.get(), llvmType, false, llvm::GlobalValue::PrivateLinkage,
          llvm::Constant::getNullValue(llvmType), var->getName());
      vars.insert(var, storage);
    }
  }

  // process functions
  for (auto *var : *x) {
    if (auto *f = cast<Func>(var)) {
      process(f);
    }
  }

  Func *main = x->getMainFunc();
  makeLLVMFunction(main);
  llvm::Function *realMain = func;
  process(main);

  // build canonical main function
  llvm::IRBuilder<> builder(context);
  auto *strType =
      llvm::StructType::get(context, {builder.getInt64Ty(), builder.getInt8PtrTy()});
  auto *arrType =
      llvm::StructType::get(context, {builder.getInt64Ty(), strType->getPointerTo()});

  auto *initFunc = llvm::cast<llvm::Function>(
      module->getOrInsertFunction("seq_init", builder.getVoidTy()));
  auto *strlenFunc = llvm::cast<llvm::Function>(module->getOrInsertFunction(
      "strlen", builder.getInt64Ty(), builder.getInt8PtrTy()));

  auto *canonicalMainFunc = llvm::cast<llvm::Function>(
      module->getOrInsertFunction("main", builder.getInt32Ty(), builder.getInt32Ty(),
                                  builder.getInt8PtrTy()->getPointerTo()));

  canonicalMainFunc->setPersonalityFn(makePersonalityFunc());
  auto argiter = canonicalMainFunc->arg_begin();
  llvm::Value *argc = argiter++;
  llvm::Value *argv = argiter;
  argc->setName("argc");
  argv->setName("argv");

  // The following generates code to put program arguments in an array, i.e.:
  //    for (int i = 0; i < argc; i++)
  //      array[i] = {strlen(argv[i]), argv[i]}
  auto *entryBlock = llvm::BasicBlock::Create(context, "entry", canonicalMainFunc);
  auto *loopBlock = llvm::BasicBlock::Create(context, "loop", canonicalMainFunc);
  auto *bodyBlock = llvm::BasicBlock::Create(context, "body", canonicalMainFunc);
  auto *exitBlock = llvm::BasicBlock::Create(context, "exit", canonicalMainFunc);

  builder.SetInsertPoint(entryBlock);
  auto *allocFunc = makeAllocFunc(/*atomic=*/false);
  llvm::Value *len = builder.CreateZExt(argc, builder.getInt64Ty());
  llvm::Value *elemSize =
      builder.getInt64(module->getDataLayout().getTypeAllocSize(strType));
  llvm::Value *allocSize = builder.CreateMul(len, elemSize);
  llvm::Value *ptr = builder.CreateCall(allocFunc, allocSize);
  ptr = builder.CreateBitCast(ptr, strType->getPointerTo());
  llvm::Value *arr = llvm::UndefValue::get(arrType);
  arr = builder.CreateInsertValue(arr, len, 0);
  arr = builder.CreateInsertValue(arr, ptr, 1);
  builder.CreateBr(loopBlock);

  builder.SetInsertPoint(loopBlock);
  llvm::PHINode *control = builder.CreatePHI(builder.getInt32Ty(), 2, "i");
  llvm::Value *next = builder.CreateAdd(control, builder.getInt32(1), "next");
  llvm::Value *cond = builder.CreateICmpSLT(control, argc);
  control->addIncoming(builder.getInt32(0), entryBlock);
  control->addIncoming(next, bodyBlock);
  builder.CreateCondBr(cond, bodyBlock, exitBlock);

  builder.SetInsertPoint(bodyBlock);
  llvm::Value *arg = builder.CreateLoad(builder.CreateGEP(argv, control));
  llvm::Value *argLen = builder.CreateZExtOrTrunc(builder.CreateCall(strlenFunc, arg),
                                                  builder.getInt64Ty());
  llvm::Value *str = llvm::UndefValue::get(strType);
  str = builder.CreateInsertValue(str, argLen, 0);
  str = builder.CreateInsertValue(str, arg, 1);
  builder.CreateStore(str, builder.CreateGEP(ptr, control));
  builder.CreateBr(loopBlock);

  builder.SetInsertPoint(exitBlock);
  builder.CreateStore(arr, argStorage);
  builder.CreateCall(initFunc);

  // Put the entire program in a parallel+single region
  {
    getOrCreateKmpc_MicroTy(context);
    getOrCreateIdentTy(module.get());
    getOrCreateDefaultLocation(module.get());

    auto *IdentTyPtrTy = getIdentTyPointerTy();

    llvm::Type *forkParams[] = {IdentTyPtrTy, builder.getInt32Ty(),
                                getKmpc_MicroPointerTy(context)};
    auto *forkFnTy = llvm::FunctionType::get(builder.getVoidTy(), forkParams, true);
    auto *forkFunc = llvm::cast<llvm::Function>(
        module->getOrInsertFunction("__kmpc_fork_call", forkFnTy));

    llvm::Type *singleParams[] = {IdentTyPtrTy, builder.getInt32Ty()};
    auto *singleFnTy =
        llvm::FunctionType::get(builder.getInt32Ty(), singleParams, false);
    auto *singleFunc = llvm::cast<llvm::Function>(
        module->getOrInsertFunction("__kmpc_single", singleFnTy));

    llvm::Type *singleEndParams[] = {IdentTyPtrTy, builder.getInt32Ty()};
    auto *singleEndFnTy =
        llvm::FunctionType::get(builder.getVoidTy(), singleEndParams, false);
    auto *singleEndFunc = llvm::cast<llvm::Function>(
        module->getOrInsertFunction("__kmpc_end_single", singleEndFnTy));

    // make the proxy main function that will be called by __kmpc_fork_call
    std::vector<llvm::Type *> proxyArgs = {builder.getInt32Ty()->getPointerTo(),
                                           builder.getInt32Ty()->getPointerTo()};
    auto *proxyMainTy = llvm::FunctionType::get(builder.getVoidTy(), proxyArgs, false);
    auto *proxyMain = llvm::cast<llvm::Function>(
        module->getOrInsertFunction("seq.proxy_main", proxyMainTy));
    proxyMain->setLinkage(llvm::GlobalValue::PrivateLinkage);
    proxyMain->setPersonalityFn(makePersonalityFunc());
    auto *proxyBlockEntry = llvm::BasicBlock::Create(context, "entry", proxyMain);
    auto *proxyBlockMain = llvm::BasicBlock::Create(context, "main", proxyMain);
    auto *proxyBlockExit = llvm::BasicBlock::Create(context, "exit", proxyMain);
    builder.SetInsertPoint(proxyBlockEntry);

    llvm::Value *tid = proxyMain->arg_begin();
    tid = builder.CreateLoad(tid);
    llvm::Value *singleCall =
        builder.CreateCall(singleFunc, {DefaultOpenMPLocation, tid});
    llvm::Value *shouldExit = builder.CreateICmpEQ(singleCall, builder.getInt32(0));
    builder.CreateCondBr(shouldExit, proxyBlockExit, proxyBlockMain);

    builder.SetInsertPoint(proxyBlockExit);
    builder.CreateRetVoid();

    // invoke real main
    auto *normal = llvm::BasicBlock::Create(context, "normal", proxyMain);
    auto *unwind = llvm::BasicBlock::Create(context, "unwind", proxyMain);
    builder.SetInsertPoint(proxyBlockMain);
    builder.CreateInvoke(realMain, normal, unwind);

    builder.SetInsertPoint(unwind);
    llvm::LandingPadInst *caughtResult = builder.CreateLandingPad(getPadType(), 1);
    caughtResult->setCleanup(true);
    caughtResult->addClause(getTypeIdxVar(nullptr));
    llvm::Value *unwindException = builder.CreateExtractValue(caughtResult, 0);
    builder.CreateCall(makeTerminateFunc(), unwindException);
    builder.CreateUnreachable();

    builder.SetInsertPoint(normal);
    builder.CreateCall(singleEndFunc, {DefaultOpenMPLocation, tid});
    builder.CreateRetVoid();

    // actually make the fork call
    std::vector<llvm::Value *> forkArgs = {
        DefaultOpenMPLocation, builder.getInt32(0),
        builder.CreateBitCast(proxyMain, getKmpc_MicroPointerTy(context))};
    builder.SetInsertPoint(exitBlock);
    builder.CreateCall(forkFunc, forkArgs);

    // tell Tapir to NOT create its own parallel regions
    fastOpenMP.setValue(true);
  }

  builder.SetInsertPoint(exitBlock);
  builder.CreateRet(builder.getInt32(0));
}

void LLVMVisitor::makeLLVMFunction(Func *x) {
  // process LLVM functions in full immediately
  if (auto *llvmFunc = cast<LLVMFunc>(x)) {
    process(llvmFunc);
    return;
  }

  auto *funcType = cast<types::FuncType>(x->getType());
  llvm::Type *returnType = getLLVMType(funcType->getReturnType());
  std::vector<llvm::Type *> argTypes;
  for (const auto &argType : *funcType) {
    argTypes.push_back(getLLVMType(argType));
  }

  auto *llvmFuncType =
      llvm::FunctionType::get(returnType, argTypes, /*isVarArg=*/false);
  const std::string functionName = getNameForFunction(x);
  func = llvm::cast<llvm::Function>(
      module->getOrInsertFunction(functionName, llvmFuncType));
}

void LLVMVisitor::makeYield(llvm::Value *value, bool finalYield) {
  llvm::IRBuilder<> builder(block);
  if (value) {
    assert(coro.promise);
    builder.CreateStore(value, coro.promise);
  }
  llvm::Function *coroSuspend =
      llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_suspend);
  llvm::Value *suspendResult =
      builder.CreateCall(coroSuspend, {llvm::ConstantTokenNone::get(context),
                                       builder.getInt1(finalYield)});

  block = llvm::BasicBlock::Create(context, "yield.new", func);

  llvm::SwitchInst *inst = builder.CreateSwitch(suspendResult, coro.suspend, 2);
  inst->addCase(builder.getInt8(0), block);
  inst->addCase(builder.getInt8(1), coro.cleanup);
}

void LLVMVisitor::visit(ExternalFunc *x) {
  func = module->getFunction(getNameForFunction(x)); // inserted during module visit
  coro = {};
  assert(func);
  func->setDoesNotThrow();
}

namespace {
// internal function type checking
template <typename ParentType>
bool internalFuncMatchesIgnoreArgs(const std::string &name, InternalFunc *x) {
  return name == x->getUnmangledName() && cast<ParentType>(x->getParentType());
}

template <typename ParentType, typename... ArgTypes, std::size_t... Index>
bool internalFuncMatches(const std::string &name, InternalFunc *x,
                         std::index_sequence<Index...>) {
  auto *funcType = cast<types::FuncType>(x->getType());
  if (name != x->getUnmangledName() ||
      std::distance(funcType->begin(), funcType->end()) != sizeof...(ArgTypes))
    return false;
  std::vector<const types::Type *> argTypes(funcType->begin(), funcType->end());
  std::vector<bool> m = {bool(cast<ParentType>(x->getParentType())),
                         bool(cast<ArgTypes>(argTypes[Index]))...};
  const bool match = std::all_of(m.begin(), m.end(), [](bool b) { return b; });
  return match;
}

template <typename ParentType, typename... ArgTypes>
bool internalFuncMatches(const std::string &name, InternalFunc *x) {
  return internalFuncMatches<ParentType, ArgTypes...>(
      name, x, std::make_index_sequence<sizeof...(ArgTypes)>());
}

// reverse complement
// TODO: move to Seq
unsigned revcompBits(unsigned n) {
  unsigned c1 = (n & (3u << 0u)) << 6u;
  unsigned c2 = (n & (3u << 2u)) << 2u;
  unsigned c3 = (n & (3u << 4u)) >> 2u;
  unsigned c4 = (n & (3u << 6u)) >> 6u;
  return ~(c1 | c2 | c3 | c4) & 0xffu;
}

// table mapping 8-bit encoded 4-mers to reverse complement encoded 4-mers
llvm::GlobalVariable *getRevCompTable(llvm::Module *module,
                                      const std::string &name = "seq.revcomp_table") {
  llvm::LLVMContext &context = module->getContext();
  llvm::Type *ty = llvm::Type::getInt8Ty(context);
  llvm::GlobalVariable *table = module->getGlobalVariable(name);

  if (!table) {
    std::vector<llvm::Constant *> v(256, llvm::ConstantInt::get(ty, 0));
    for (unsigned i = 0; i < v.size(); i++)
      v[i] = llvm::ConstantInt::get(ty, revcompBits(i));

    auto *arrTy = llvm::ArrayType::get(llvm::Type::getInt8Ty(context), v.size());
    table = new GlobalVariable(*module, arrTy, true, llvm::GlobalValue::PrivateLinkage,
                               llvm::ConstantArray::get(arrTy, v), name);
  }

  return table;
}

llvm::Value *codegenRevCompByBitShift(const unsigned k, llvm::Value *self,
                                      llvm::IRBuilder<> &b) {
  llvm::Type *kmerType = b.getIntNTy(2 * k);
  llvm::LLVMContext &context = b.getContext();

  unsigned kpow2 = 1;
  while (kpow2 < k)
    kpow2 *= 2;
  const unsigned w = 2 * kpow2;

  llvm::Type *ty = llvm::IntegerType::get(context, w);
  llvm::Value *comp = b.CreateNot(self);
  comp = b.CreateZExt(comp, ty);
  llvm::Value *result = comp;

  for (unsigned i = 2; i <= kpow2; i = i * 2) {
    llvm::Value *mask = llvm::ConstantInt::get(ty, 0);
    llvm::Value *bitpattern = llvm::ConstantInt::get(ty, 1);
    bitpattern = b.CreateShl(bitpattern, i);
    bitpattern = b.CreateSub(bitpattern, ConstantInt::get(ty, 1));

    unsigned j = 0;
    while (j < w) {
      llvm::Value *shift = b.CreateShl(bitpattern, j);
      mask = b.CreateOr(mask, shift);
      j += 2 * i;
    }

    llvm::Value *r1 = b.CreateLShr(result, i);
    r1 = b.CreateAnd(r1, mask);
    llvm::Value *r2 = b.CreateAnd(result, mask);
    r2 = b.CreateShl(r2, i);
    result = b.CreateOr(r1, r2);
  }

  if (w != 2 * k) {
    assert(w > 2 * k);
    result = b.CreateLShr(result, w - (2 * k));
    result = b.CreateTrunc(result, kmerType);
  }
  return result;
}

llvm::Value *codegenRevCompByLookup(const unsigned k, llvm::Value *self,
                                    llvm::IRBuilder<> &b) {
  Type *kmerType = b.getIntNTy(2 * k);
  llvm::Module *module = b.GetInsertBlock()->getModule();
  llvm::Value *table = getRevCompTable(module);
  llvm::Value *mask = llvm::ConstantInt::get(kmerType, 0xffu);
  llvm::Value *result = llvm::ConstantInt::get(kmerType, 0);

  // deal with 8-bit chunks:
  for (unsigned i = 0; i < k / 4; i++) {
    llvm::Value *slice = b.CreateShl(mask, i * 8);
    slice = b.CreateAnd(self, slice);
    slice = b.CreateLShr(slice, i * 8);
    slice = b.CreateZExtOrTrunc(slice, b.getInt64Ty());

    llvm::Value *sliceRC = b.CreateInBoundsGEP(table, {b.getInt64(0), slice});
    sliceRC = b.CreateLoad(sliceRC);
    sliceRC = b.CreateZExtOrTrunc(sliceRC, kmerType);
    sliceRC = b.CreateShl(sliceRC, (k - 4 * (i + 1)) * 2);
    result = b.CreateOr(result, sliceRC);
  }

  // deal with remaining high bits:
  unsigned rem = k % 4;
  if (rem > 0) {
    mask = llvm::ConstantInt::get(kmerType, (1u << (rem * 2)) - 1);
    llvm::Value *slice = b.CreateShl(mask, (k - rem) * 2);
    slice = b.CreateAnd(self, slice);
    slice = b.CreateLShr(slice, (k - rem) * 2);
    slice = b.CreateZExtOrTrunc(slice, b.getInt64Ty());

    llvm::Value *sliceRC = b.CreateInBoundsGEP(table, {b.getInt64(0), slice});
    sliceRC = b.CreateLoad(sliceRC);
    sliceRC = b.CreateAShr(sliceRC,
                           (4 - rem) * 2); // slice isn't full 8-bits, so shift out junk
    sliceRC = b.CreateZExtOrTrunc(sliceRC, kmerType);
    sliceRC = b.CreateAnd(sliceRC, mask);
    result = b.CreateOr(result, sliceRC);
  }

  return result;
}

llvm::Value *codegenRevCompBySIMD(const unsigned k, llvm::Value *self,
                                  llvm::IRBuilder<> &b) {
  llvm::Type *kmerType = b.getIntNTy(2 * k);
  llvm::LLVMContext &context = b.getContext();
  llvm::Value *comp = b.CreateNot(self);

  llvm::Type *ty = kmerType;
  const unsigned w = ((2 * k + 7) / 8) * 8;
  const unsigned m = w / 8;

  if (w != 2 * k) {
    ty = llvm::IntegerType::get(context, w);
    comp = b.CreateZExt(comp, ty);
  }

  auto *vecTy = llvm::VectorType::get(b.getInt8Ty(), m);
  std::vector<unsigned> shufMask;
  for (unsigned i = 0; i < m; i++)
    shufMask.push_back(m - 1 - i);

  llvm::Value *vec = llvm::UndefValue::get(llvm::VectorType::get(ty, 1));
  vec = b.CreateInsertElement(vec, comp, (uint64_t)0);
  vec = b.CreateBitCast(vec, vecTy);
  // shuffle reverses bytes
  vec = b.CreateShuffleVector(vec, UndefValue::get(vecTy), shufMask);

  // shifts reverse 2-bit chunks in each byte
  llvm::Value *shift1 = llvm::ConstantVector::getSplat(m, b.getInt8(6));
  llvm::Value *shift2 = llvm::ConstantVector::getSplat(m, b.getInt8(2));
  llvm::Value *mask1 = llvm::ConstantVector::getSplat(m, b.getInt8(0x0c));
  llvm::Value *mask2 = llvm::ConstantVector::getSplat(m, b.getInt8(0x30));

  llvm::Value *vec1 = b.CreateLShr(vec, shift1);
  llvm::Value *vec2 = b.CreateShl(vec, shift1);
  llvm::Value *vec3 = b.CreateLShr(vec, shift2);
  llvm::Value *vec4 = b.CreateShl(vec, shift2);
  vec3 = b.CreateAnd(vec3, mask1);
  vec4 = b.CreateAnd(vec4, mask2);

  vec = b.CreateOr(vec1, vec2);
  vec = b.CreateOr(vec, vec3);
  vec = b.CreateOr(vec, vec4);

  vec = b.CreateBitCast(vec, llvm::VectorType::get(ty, 1));
  llvm::Value *result = b.CreateExtractElement(vec, (uint64_t)0);
  if (w != 2 * k) {
    assert(w > 2 * k);
    result = b.CreateLShr(result, w - (2 * k));
    result = b.CreateTrunc(result, kmerType);
  }
  return result;
}
} // namespace

void LLVMVisitor::visit(InternalFunc *x) {
  using namespace types;
  func = module->getFunction(getNameForFunction(x)); // inserted during module visit
  coro = {};

  const Type *parentType = x->getParentType();
  auto *funcType = cast<FuncType>(x->getType());
  std::vector<const Type *> argTypes(funcType->begin(), funcType->end());

  assert(func);
  func->setLinkage(llvm::GlobalValue::PrivateLinkage);
  func->addFnAttr(llvm::Attribute::AttrKind::AlwaysInline);
  std::vector<llvm::Value *> args;
  for (auto it = func->arg_begin(); it != func->arg_end(); ++it) {
    args.push_back(it);
  }
  block = llvm::BasicBlock::Create(context, "entry", func);
  llvm::IRBuilder<> builder(block);
  llvm::Value *result = nullptr;

  if (internalFuncMatches<PointerType>("__elemsize__", x)) {
    auto *pointerType = cast<PointerType>(parentType);
    result = builder.getInt64(
        module->getDataLayout().getTypeAllocSize(getLLVMType(pointerType->getBase())));
  }

  if (internalFuncMatches<PointerType>("__atomic__", x)) {
    result = builder.getInt8(parentType->isAtomic() ? 1 : 0);
  }

  if (internalFuncMatches<PointerType, IntType>("__new__", x)) {
    auto *pointerType = cast<PointerType>(parentType);
    const Type *baseType = pointerType->getBase();
    llvm::Type *llvmBaseType = getLLVMType(baseType);
    llvm::Function *allocFunc = makeAllocFunc(baseType->isAtomic());
    llvm::Value *elemSize =
        builder.getInt64(module->getDataLayout().getTypeAllocSize(llvmBaseType));
    llvm::Value *allocSize = builder.CreateMul(elemSize, args[0]);
    result = builder.CreateCall(allocFunc, allocSize);
    result = builder.CreateBitCast(result, llvmBaseType->getPointerTo());
  }

  else if (internalFuncMatches<IntType, IntNType>("__new__", x)) {
    auto *intNType = cast<IntNType>(argTypes[0]);
    if (intNType->isSigned()) {
      result = builder.CreateSExtOrTrunc(args[0], builder.getInt64Ty());
    } else {
      result = builder.CreateZExtOrTrunc(args[0], builder.getInt64Ty());
    }
  }

  else if (internalFuncMatches<IntNType, IntType>("__new__", x)) {
    auto *intNType = cast<IntNType>(parentType);
    if (intNType->isSigned()) {
      result = builder.CreateSExtOrTrunc(args[0], getLLVMType(intNType));
    } else {
      result = builder.CreateZExtOrTrunc(args[0], getLLVMType(intNType));
    }
  }

  else if (internalFuncMatches<RefType>("__new__", x)) {
    auto *refType = cast<RefType>(parentType);
    llvm::Function *allocFunc = makeAllocFunc(parentType->isAtomic());
    llvm::Value *size = builder.getInt64(
        module->getDataLayout().getTypeAllocSize(getLLVMType(refType->getContents())));
    result = builder.CreateCall(allocFunc, size);
  }

  else if (internalFuncMatches<RefType, RefType>("__raw__", x)) {
    result = args[0];
  }

  else if (internalFuncMatches<GeneratorType, GeneratorType>("done", x)) {
    llvm::Function *coroResume =
        llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_resume);
    llvm::Function *coroDone =
        llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_done);
    builder.CreateCall(coroResume, args[0]);
    result =
        builder.CreateZExt(builder.CreateCall(coroDone, args[0]), builder.getInt8Ty());
  }

  else if (internalFuncMatches<GeneratorType, GeneratorType>("next", x)) {
    auto *generatorType = cast<GeneratorType>(parentType);
    llvm::Type *baseType = getLLVMType(generatorType->getBase());
    llvm::Function *coroPromise =
        llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_promise);
    llvm::Value *aln =
        builder.getInt32(module->getDataLayout().getPrefTypeAlignment(baseType));
    llvm::Value *from = builder.getFalse();
    llvm::Value *ptr = builder.CreateCall(coroPromise, {args[0], aln, from});
    ptr = builder.CreateBitCast(ptr, baseType->getPointerTo());
    result = builder.CreateLoad(ptr);
  }

  else if (internalFuncMatches<OptionalType>("__new__", x)) {
    auto *optionalType = cast<OptionalType>(parentType);
    if (cast<RefType>(optionalType->getBase())) {
      result = llvm::ConstantPointerNull::get(builder.getInt8PtrTy());
    } else {
      result = llvm::UndefValue::get(getLLVMType(optionalType));
      result = builder.CreateInsertValue(result, builder.getFalse(), 0);
    }
  } else if (internalFuncMatchesIgnoreArgs<OptionalType>("__new__", x)) {
    assert(args.size() == 1);
    auto *optionalType = cast<OptionalType>(parentType);
    if (cast<RefType>(optionalType->getBase())) {
      result = args[0];
    } else {
      result = llvm::UndefValue::get(getLLVMType(optionalType));
      result = builder.CreateInsertValue(result, builder.getTrue(), 0);
      result = builder.CreateInsertValue(result, args[0], 1);
    }
  }

  else if (internalFuncMatches<OptionalType, OptionalType>("__bool__", x)) {
    auto *optionalType = cast<OptionalType>(parentType);
    if (cast<RefType>(optionalType->getBase())) {
      result = builder.CreateIsNotNull(args[0]);
    } else {
      result = builder.CreateExtractValue(args[0], 0);
    }
    result = builder.CreateZExt(result, builder.getInt8Ty());
  }

  else if (internalFuncMatches<OptionalType, OptionalType>("__invert__", x)) {
    auto *optionalType = cast<OptionalType>(parentType);
    if (cast<RefType>(optionalType->getBase())) {
      result = args[0];
    } else {
      result = builder.CreateExtractValue(args[0], 1);
    }
  }

  else if (internalFuncMatchesIgnoreArgs<RecordType>("__new__", x)) {
    auto *recordType = cast<RecordType>(parentType);
    assert(args.size() == std::distance(recordType->begin(), recordType->end()));
    result = llvm::UndefValue::get(getLLVMType(recordType));
    for (auto i = 0; i < args.size(); i++) {
      result = builder.CreateInsertValue(result, args[i], i);
    }
  }

  else if (internalFuncMatches<RecordType, RecordType, IntType>("__getitem__", x)) {
    // TODO: move to Seq (does not perform bounds check or index correction)
    auto *recordType = cast<RecordType>(parentType);
    llvm::Value *storage = builder.CreateAlloca(getLLVMType(recordType));
    builder.CreateStore(args[0], storage);
    llvm::Value *ptr = builder.CreateBitCast(
        storage, getLLVMType(recordType->front().type)->getPointerTo());
    ptr = builder.CreateGEP(ptr, args[1]);
    result = builder.CreateLoad(ptr);
  }

  else if (internalFuncMatches<IntNType, IntNType>("__revcomp__", x)) {
    auto *intNType = cast<IntNType>(parentType);
    if (intNType->getLen() % 2 != 0) {
      result = llvm::ConstantAggregateZero::get(getLLVMType(intNType));
    } else {
      const unsigned k = intNType->getLen() / 2;
      if (k <= 20) {
        result = codegenRevCompByLookup(k, args[0], builder);
      } else if (k < 32) {
        result = codegenRevCompByBitShift(k, args[0], builder);
      } else {
        result = codegenRevCompBySIMD(k, args[0], builder);
      }
    }
  }

  assert(result && "internal function not found");
  builder.CreateRet(result);
}

std::string LLVMVisitor::buildLLVMCodeString(LLVMFunc *x) {
  auto *funcType = cast<types::FuncType>(x->getType());
  assert(funcType);
  std::string bufStr;
  llvm::raw_string_ostream buf(bufStr);

  // build function signature
  buf << "define ";
  getLLVMType(funcType->getReturnType())->print(buf);
  buf << " @\"" << getNameForFunction(x) << "\"(";
  const int numArgs = std::distance(x->arg_begin(), x->arg_end());
  int argIndex = 0;
  for (auto it = x->arg_begin(); it != x->arg_end(); ++it) {
    getLLVMType((*it)->getType())->print(buf);
    buf << " %" << (*it)->getName();
    if (argIndex < numArgs - 1)
      buf << ", ";
    ++argIndex;
  }
  buf << ")";
  std::string signature = buf.str();
  bufStr.clear();

  // replace literal '{' and '}'
  std::string::size_type n = 0;
  while ((n = signature.find("{", n)) != std::string::npos) {
    signature.replace(n, 1, "{{");
    n += 2;
  }
  n = 0;
  while ((n = signature.find("}", n)) != std::string::npos) {
    signature.replace(n, 1, "}}");
    n += 2;
  }

  // build remaining code
  buf << x->getLLVMDeclarations() << "\n"
      << signature << " {{\n"
      << x->getLLVMBody() << "\n}}";
  return buf.str();
}

void LLVMVisitor::visit(LLVMFunc *x) {
  func = module->getFunction(getNameForFunction(x));
  coro = {};
  if (func)
    return;

  // build code
  std::string code = buildLLVMCodeString(x);

  // format code
  fmt::dynamic_format_arg_store<fmt::format_context> store;
  for (auto it = x->literal_begin(); it != x->literal_end(); ++it) {
    switch (it->tag) {
    case LLVMFunc::LLVMLiteral::STATIC: {
      store.push_back(it->val.staticVal);
      break;
    }
    case LLVMFunc::LLVMLiteral::TYPE: {
      llvm::Type *llvmType = getLLVMType(it->val.type);
      std::string bufStr;
      llvm::raw_string_ostream buf(bufStr);
      llvmType->print(buf);
      store.push_back(buf.str());
      break;
    }
    default:
      assert(0);
    }
  }
  code = fmt::vformat(code, store);

  llvm::SMDiagnostic err;
  std::unique_ptr<llvm::MemoryBuffer> buf = llvm::MemoryBuffer::getMemBuffer(code);
  assert(buf);
  std::unique_ptr<llvm::Module> sub =
      llvm::parseIR(buf->getMemBufferRef(), err, context);
  if (!sub) {
    std::string bufStr;
    llvm::raw_string_ostream buf(bufStr);
    err.print("LLVM", buf);
    throw std::runtime_error(buf.str());
  }
  sub->setDataLayout(module->getDataLayout());

  llvm::Linker L(*module);
  const bool fail = L.linkInModule(std::move(sub));
  assert(!fail);
  func = module->getFunction(getNameForFunction(x));
  assert(func);
  func->setLinkage(llvm::GlobalValue::PrivateLinkage);
  func->addFnAttr(llvm::Attribute::AttrKind::AlwaysInline);
}

void LLVMVisitor::visit(BodiedFunc *x) {
  func = module->getFunction(getNameForFunction(x)); // inserted during module visit
  coro = {};
  assert(func);

  if (x->hasAttribute("export")) {
    func->setLinkage(llvm::GlobalValue::ExternalLinkage);
  } else {
    func->setLinkage(llvm::GlobalValue::PrivateLinkage);
  }
  if (x->hasAttribute("inline")) {
    func->addFnAttr(llvm::Attribute::AttrKind::AlwaysInline);
  }
  if (x->hasAttribute("noinline")) {
    func->addFnAttr(llvm::Attribute::AttrKind::NoInline);
  }
  func->setPersonalityFn(makePersonalityFunc());

  auto *funcType = cast<types::FuncType>(x->getType());
  auto *returnType = funcType->getReturnType();
  assert(funcType);
  auto *entryBlock = llvm::BasicBlock::Create(context, "entry", func);
  llvm::IRBuilder<> builder(entryBlock);

  // set up arguments and other symbols
  assert(std::distance(func->arg_begin(), func->arg_end()) ==
         std::distance(x->arg_begin(), x->arg_end()));
  auto argIter = func->arg_begin();
  for (auto varIter = x->arg_begin(); varIter != x->arg_end(); ++varIter) {
    Var *var = *varIter;
    llvm::Value *storage = builder.CreateAlloca(getLLVMType(var->getType()));
    builder.CreateStore(argIter, storage);
    vars.insert(var, storage);
    ++argIter;
  }

  for (auto *symbol : *x) {
    llvm::Value *storage = builder.CreateAlloca(getLLVMType(symbol->getType()));
    vars.insert(symbol, storage);
  }

  auto *startBlock = llvm::BasicBlock::Create(context, "start", func);

  if (x->isGenerator()) {
    auto *generatorType = cast<types::GeneratorType>(returnType);
    assert(generatorType);

    llvm::Function *coroId =
        llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_id);
    llvm::Function *coroBegin =
        llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_begin);
    llvm::Function *coroSize = llvm::Intrinsic::getDeclaration(
        module.get(), llvm::Intrinsic::coro_size, {builder.getInt64Ty()});
    llvm::Function *coroEnd =
        llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_end);
    llvm::Function *coroAlloc =
        llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_alloc);
    llvm::Function *coroFree =
        llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_free);

    coro.cleanup = llvm::BasicBlock::Create(context, "coro.cleanup", func);
    coro.suspend = llvm::BasicBlock::Create(context, "coro.suspend", func);
    coro.exit = llvm::BasicBlock::Create(context, "coro.exit", func);
    auto *allocBlock = llvm::BasicBlock::Create(context, "coro.alloc", func);
    auto *freeBlock = llvm::BasicBlock::Create(context, "coro.free", func);

    // coro ID and promise
    llvm::Value *id = nullptr;
    llvm::Value *nullPtr = llvm::ConstantPointerNull::get(builder.getInt8PtrTy());
    if (!cast<types::VoidType>(generatorType->getBase())) {
      coro.promise = makeAlloca(getLLVMType(generatorType->getBase()), entryBlock);
      coro.promise->setName("coro.promise");
      llvm::Value *promiseRaw =
          builder.CreateBitCast(coro.promise, builder.getInt8PtrTy());
      id = builder.CreateCall(coroId,
                              {builder.getInt32(0), promiseRaw, nullPtr, nullPtr});
    } else {
      id = builder.CreateCall(coroId, {builder.getInt32(0), nullPtr, nullPtr, nullPtr});
    }
    id->setName("coro.id");
    llvm::Value *needAlloc = builder.CreateCall(coroAlloc, id);
    builder.CreateCondBr(needAlloc, allocBlock, startBlock);

    // coro alloc
    builder.SetInsertPoint(allocBlock);
    llvm::Value *size = builder.CreateCall(coroSize);
    auto *allocFunc = makeAllocFunc(/*atomic=*/false);
    llvm::Value *alloc = builder.CreateCall(allocFunc, size);
    builder.CreateBr(startBlock);

    // coro start
    builder.SetInsertPoint(startBlock);
    llvm::PHINode *phi = builder.CreatePHI(builder.getInt8PtrTy(), 2);
    phi->addIncoming(nullPtr, entryBlock);
    phi->addIncoming(alloc, allocBlock);
    coro.handle = builder.CreateCall(coroBegin, {id, phi});
    coro.handle->setName("coro.handle");

    // coro cleanup
    builder.SetInsertPoint(coro.cleanup);
    llvm::Value *mem = builder.CreateCall(coroFree, {id, coro.handle});
    llvm::Value *needFree = builder.CreateIsNotNull(mem);
    builder.CreateCondBr(needFree, freeBlock, coro.suspend);

    // coro free
    builder.SetInsertPoint(freeBlock); // no-op: GC will free automatically
    builder.CreateBr(coro.suspend);

    // coro suspend
    builder.SetInsertPoint(coro.suspend);
    builder.CreateCall(coroEnd, {coro.handle, builder.getFalse()});
    builder.CreateRet(coro.handle);

    // coro exit
    block = coro.exit;
    makeYield(nullptr, /*finalYield=*/true);
    builder.SetInsertPoint(block);
    builder.CreateUnreachable();

    // initial yield
    block = startBlock;
    makeYield(); // coroutine will be initially suspended
  } else {
    builder.CreateBr(startBlock);
    block = startBlock;
  }

  process(x->getBody());
  builder.SetInsertPoint(block);

  if (x->isGenerator()) {
    builder.CreateBr(coro.exit);
  } else {
    if (cast<types::VoidType>(returnType)) {
      builder.CreateRetVoid();
    } else {
      builder.CreateRet(llvm::Constant::getNullValue(getLLVMType(returnType)));
    }
  }
}

void LLVMVisitor::visit(Var *x) { assert(0); }

void LLVMVisitor::visit(VarValue *x) {
  if (auto *f = cast<Func>(x->getVar())) {
    value = funcs[f];
    assert(value);
  } else {
    llvm::Value *varPtr = vars[x->getVar()];
    assert(varPtr);
    llvm::IRBuilder<> builder(block);
    value = builder.CreateLoad(varPtr);
  }
}

void LLVMVisitor::visit(PointerValue *x) {
  llvm::Value *var = vars[x->getVar()];
  assert(var);
  value = var; // note: we don't load the pointer
}

void LLVMVisitor::visit(ValueProxy *x) { assert(0); }

/*
 * Types
 */

void LLVMVisitor::visit(const types::IntType *x) {
  type = llvm::Type::getInt64Ty(context);
}

void LLVMVisitor::visit(const types::FloatType *x) {
  type = llvm::Type::getDoubleTy(context);
}

void LLVMVisitor::visit(const types::BoolType *x) {
  type = llvm::Type::getInt8Ty(context);
}

void LLVMVisitor::visit(const types::ByteType *x) {
  type = llvm::Type::getInt8Ty(context);
}

void LLVMVisitor::visit(const types::VoidType *x) {
  type = llvm::Type::getVoidTy(context);
}

void LLVMVisitor::visit(const types::RecordType *x) {
  std::vector<llvm::Type *> body;
  for (const auto &field : *x) {
    body.push_back(getLLVMType(field.type));
  }
  type = llvm::StructType::get(context, body);
}

void LLVMVisitor::visit(const types::RefType *x) {
  type = llvm::Type::getInt8PtrTy(context);
}

void LLVMVisitor::visit(const types::FuncType *x) {
  llvm::Type *returnType = getLLVMType(x->getReturnType());
  std::vector<llvm::Type *> argTypes;
  for (const auto &argType : *x) {
    argTypes.push_back(getLLVMType(argType));
  }
  type =
      llvm::FunctionType::get(returnType, argTypes, /*isVarArg=*/false)->getPointerTo();
}

void LLVMVisitor::visit(const types::OptionalType *x) {
  if (cast<types::RefType>(x->getBase())) {
    type = llvm::Type::getInt8PtrTy(context);
  } else {
    type = llvm::StructType::get(llvm::Type::getInt1Ty(context),
                                 getLLVMType(x->getBase()));
  }
}

void LLVMVisitor::visit(const types::ArrayType *x) {
  type = llvm::StructType::get(llvm::Type::getInt64Ty(context),
                               getLLVMType(x->getBase())->getPointerTo());
}

void LLVMVisitor::visit(const types::PointerType *x) {
  type = getLLVMType(x->getBase())->getPointerTo();
}

void LLVMVisitor::visit(const types::GeneratorType *x) {
  type = llvm::Type::getInt8PtrTy(context);
}

void LLVMVisitor::visit(const types::IntNType *x) {
  type = llvm::Type::getIntNTy(context, x->getLen());
}

/*
 * Constants
 */

void LLVMVisitor::visit(IntConstant *x) {
  llvm::IRBuilder<> builder(block);
  value = builder.getInt64(x->getVal());
}

void LLVMVisitor::visit(FloatConstant *x) {
  llvm::IRBuilder<> builder(block);
  value = llvm::ConstantFP::get(builder.getDoubleTy(), x->getVal());
}

void LLVMVisitor::visit(BoolConstant *x) {
  llvm::IRBuilder<> builder(block);
  value = builder.getInt8(x->getVal() ? 1 : 0);
}

void LLVMVisitor::visit(StringConstant *x) {
  llvm::IRBuilder<> builder(block);
  std::string s = x->getVal();
  auto *strVar = new llvm::GlobalVariable(
      *module, llvm::ArrayType::get(builder.getInt8Ty(), s.length() + 1), true,
      llvm::GlobalValue::PrivateLinkage, llvm::ConstantDataArray::getString(context, s),
      "str_literal");
  auto *strType = llvm::StructType::get(builder.getInt64Ty(), builder.getInt8PtrTy());
  llvm::Value *ptr = builder.CreateBitCast(strVar, builder.getInt8PtrTy());
  llvm::Value *len = builder.getInt64(s.length());
  llvm::Value *str = llvm::UndefValue::get(strType);
  str = builder.CreateInsertValue(str, len, 0);
  str = builder.CreateInsertValue(str, ptr, 1);
  value = str;
}

/*
 * Control flow
 */

void LLVMVisitor::visit(SeriesFlow *x) {
  for (auto *value : *x) {
    process(value);
  }
}

void LLVMVisitor::visit(IfFlow *x) {
  auto *trueBlock = llvm::BasicBlock::Create(context, "if.true", func);
  auto *falseBlock = llvm::BasicBlock::Create(context, "if.false", func);
  auto *exitBlock = llvm::BasicBlock::Create(context, "if.exit", func);

  process(x->getCond());
  llvm::Value *cond = value;
  llvm::IRBuilder<> builder(block);
  cond = builder.CreateTrunc(cond, builder.getInt1Ty());
  builder.CreateCondBr(cond, trueBlock, falseBlock);

  block = trueBlock;
  if (x->getTrueBranch()) {
    process(x->getTrueBranch());
  }
  builder.SetInsertPoint(block);
  builder.CreateBr(exitBlock);

  block = falseBlock;
  if (x->getFalseBranch()) {
    process(x->getFalseBranch());
  }
  builder.SetInsertPoint(block);
  builder.CreateBr(exitBlock);

  block = exitBlock;
}

void LLVMVisitor::visit(WhileFlow *x) {
  auto *condBlock = llvm::BasicBlock::Create(context, "while.cond", func);
  auto *bodyBlock = llvm::BasicBlock::Create(context, "while.body", func);
  auto *exitBlock = llvm::BasicBlock::Create(context, "while.exit", func);

  llvm::IRBuilder<> builder(block);
  builder.CreateBr(condBlock);

  block = condBlock;
  process(x->getCond());
  llvm::Value *cond = value;
  builder.SetInsertPoint(block);
  cond = builder.CreateTrunc(cond, builder.getInt1Ty());
  builder.CreateCondBr(cond, bodyBlock, exitBlock);

  block = bodyBlock;
  enterLoop({/*breakBlock=*/exitBlock, /*continueBlock=*/condBlock});
  process(x->getBody());
  exitLoop();
  builder.SetInsertPoint(block);
  builder.CreateBr(condBlock);

  block = exitBlock;
}

void LLVMVisitor::visit(ForFlow *x) {
  llvm::Type *loopVarType = getLLVMType(x->getVar()->getType());
  llvm::Value *loopVar = vars[x->getVar()];
  assert(loopVar);

  auto *condBlock = llvm::BasicBlock::Create(context, "for.cond", func);
  auto *bodyBlock = llvm::BasicBlock::Create(context, "for.body", func);
  auto *cleanupBlock = llvm::BasicBlock::Create(context, "for.cleanup", func);
  auto *exitBlock = llvm::BasicBlock::Create(context, "for.exit", func);

  // LLVM coroutine intrinsics
  // https://prereleases.llvm.org/6.0.0/rc3/docs/Coroutines.html
  llvm::Function *coroResume =
      llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_resume);
  llvm::Function *coroDone =
      llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_done);
  llvm::Function *coroPromise =
      llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_promise);
  llvm::Function *coroDestroy =
      llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_destroy);

  process(x->getIter());
  llvm::Value *iter = value;
  llvm::IRBuilder<> builder(block);
  builder.CreateBr(condBlock);

  block = condBlock;
  call(coroResume, {iter});
  builder.SetInsertPoint(block);
  llvm::Value *done = builder.CreateCall(coroDone, iter);
  builder.CreateCondBr(done, cleanupBlock, bodyBlock);

  if (!loopVarType->isVoidTy()) {
    builder.SetInsertPoint(bodyBlock);
    llvm::Value *alignment =
        builder.getInt32(module->getDataLayout().getPrefTypeAlignment(loopVarType));
    llvm::Value *from = builder.getFalse();
    llvm::Value *promise = builder.CreateCall(coroPromise, {iter, alignment, from});
    promise = builder.CreateBitCast(promise, loopVarType->getPointerTo());
    llvm::Value *generatedValue = builder.CreateLoad(promise);
    builder.CreateStore(generatedValue, loopVar);
  }

  block = bodyBlock;
  enterLoop({/*breakBlock=*/exitBlock, /*continueBlock=*/condBlock});
  process(x->getBody());
  exitLoop();
  builder.SetInsertPoint(block);
  builder.CreateBr(condBlock);

  builder.SetInsertPoint(cleanupBlock);
  builder.CreateCall(coroDestroy, iter);
  builder.CreateBr(exitBlock);

  block = exitBlock;
}

namespace {
bool anyMatch(const types::Type *type, std::vector<const types::Type *> types) {
  if (type) {
    for (auto *t : types) {
      if (t && t->getName() == type->getName())
        return true;
    }
  } else {
    for (auto *t : types) {
      if (!t)
        return true;
    }
  }
  return false;
}
} // namespace

void LLVMVisitor::visit(TryCatchFlow *x) {
  const bool isRoot = trycatch.empty();
  const bool supportBreakAndContinue = !loops.empty();
  llvm::IRBuilder<> builder(block);
  auto *entryBlock = llvm::BasicBlock::Create(context, "trycatch.entry", func);
  builder.CreateBr(entryBlock);

  TryCatchData tc;
  tc.exceptionBlock = llvm::BasicBlock::Create(context, "trycatch.exception", func);
  tc.exceptionRouteBlock =
      llvm::BasicBlock::Create(context, "trycatch.exception_route", func);
  tc.finallyBlock = llvm::BasicBlock::Create(context, "trycatch.finally", func);

  auto *externalExcBlock =
      llvm::BasicBlock::Create(context, "trycatch.exception_external", func);
  auto *unwindResumeBlock =
      llvm::BasicBlock::Create(context, "trycatch.unwind_resume", func);
  auto *endBlock = llvm::BasicBlock::Create(context, "trycatch.end", func);

  builder.SetInsertPoint(func->getEntryBlock().getTerminator());
  auto *excStateNotThrown = builder.getInt8(TryCatchData::State::NOT_THROWN);
  auto *excStateThrown = builder.getInt8(TryCatchData::State::THROWN);
  auto *excStateCaught = builder.getInt8(TryCatchData::State::CAUGHT);
  auto *excStateReturn = builder.getInt8(TryCatchData::State::RETURN);
  auto *excStateBreak = builder.getInt8(TryCatchData::State::BREAK);
  auto *excStateContinue = builder.getInt8(TryCatchData::State::CONTINUE);

  llvm::StructType *padType = getPadType();
  llvm::StructType *unwindType =
      llvm::StructType::get(builder.getInt64Ty()); // header only
  llvm::StructType *excType =
      llvm::StructType::get(getTypeInfoType(), builder.getInt8PtrTy());

  if (isRoot) {
    tc.excFlag = builder.CreateAlloca(builder.getInt8Ty());
    tc.catchStore = builder.CreateAlloca(padType);
    tc.delegateDepth = builder.CreateAlloca(builder.getInt64Ty());
    tc.retStore = func->getReturnType()->isVoidTy()
                      ? nullptr
                      : builder.CreateAlloca(func->getReturnType());

    builder.CreateStore(excStateNotThrown, tc.excFlag);
    builder.CreateStore(llvm::ConstantAggregateZero::get(padType), tc.catchStore);
    builder.CreateStore(builder.getInt64(0), tc.delegateDepth);
  } else {
    tc.excFlag = trycatch[0].excFlag;
    tc.catchStore = trycatch[0].catchStore;
    tc.delegateDepth = trycatch[0].delegateDepth;
    tc.retStore = trycatch[0].retStore;
  }

  // codegen finally
  block = tc.finallyBlock;
  process(x->getFinally());
  auto *finallyBlock = block;
  builder.SetInsertPoint(finallyBlock);
  llvm::Value *excFlagRead = builder.CreateLoad(tc.excFlag);

  if (!isRoot) {
    llvm::Value *depthRead = builder.CreateLoad(tc.delegateDepth);
    llvm::Value *delegate = builder.CreateICmpSGT(depthRead, builder.getInt64(0));
    auto *finallyNormal =
        llvm::BasicBlock::Create(context, "trycatch.finally.normal", func);
    auto *finallyDelegate =
        llvm::BasicBlock::Create(context, "trycatch.finally.delegate", func);
    builder.CreateCondBr(delegate, finallyDelegate, finallyNormal);

    builder.SetInsertPoint(finallyDelegate);
    llvm::Value *depthNew = builder.CreateSub(depthRead, builder.getInt64(1));
    llvm::Value *delegateNew = builder.CreateICmpSGT(depthNew, builder.getInt64(0));
    builder.CreateStore(depthNew, tc.delegateDepth);
    builder.CreateCondBr(delegateNew, trycatch.back().finallyBlock,
                         trycatch.back().exceptionRouteBlock);

    finallyBlock = finallyNormal;
    builder.SetInsertPoint(finallyNormal);
  }

  builder.SetInsertPoint(finallyBlock);
  llvm::SwitchInst *theSwitch =
      builder.CreateSwitch(excFlagRead, endBlock, supportBreakAndContinue ? 5 : 3);
  theSwitch->addCase(excStateCaught, endBlock);
  theSwitch->addCase(excStateThrown, unwindResumeBlock);

  if (isRoot) {
    auto *finallyReturn =
        llvm::BasicBlock::Create(context, "trycatch.finally.return", func);
    theSwitch->addCase(excStateReturn, finallyReturn);
    builder.SetInsertPoint(finallyReturn);
    if (tc.retStore) {
      llvm::Value *retVal = builder.CreateLoad(tc.retStore);
      builder.CreateRet(retVal);
    } else {
      builder.CreateRetVoid();
    }
  } else {
    theSwitch->addCase(excStateReturn, trycatch.back().finallyBlock);
  }

  if (supportBreakAndContinue) {
    const bool outer = (getInnermostTryCatchBeforeLoop() == nullptr);
    if (outer) {
      auto *finallyBreak =
          llvm::BasicBlock::Create(context, "trycatch.finally.break", func);
      auto *finallyContinue =
          llvm::BasicBlock::Create(context, "trycatch.finally.continue", func);
      theSwitch->addCase(excStateBreak, finallyBreak);
      theSwitch->addCase(excStateContinue, finallyContinue);

      builder.SetInsertPoint(finallyBreak);
      builder.CreateStore(excStateNotThrown, tc.excFlag);
      builder.CreateBr(loops.back().breakBlock);

      builder.SetInsertPoint(finallyContinue);
      builder.CreateStore(excStateNotThrown, tc.excFlag);
      builder.CreateBr(loops.back().continueBlock);
    } else {
      assert(!isRoot);
      theSwitch->addCase(excStateBreak, trycatch.back().finallyBlock);
      theSwitch->addCase(excStateContinue, trycatch.back().finallyBlock);
    }
  }

  // try and catch codegen
  std::vector<TryCatchFlow::Catch *> catches;
  for (auto &c : *x) {
    catches.push_back(&c);
  }
  llvm::BasicBlock *catchAll = nullptr;

  for (auto *c : catches) {
    auto *catchBlock = llvm::BasicBlock::Create(context, "trycatch.catch", func);
    tc.catchTypes.push_back(c->getType());
    tc.handlers.push_back(catchBlock);

    if (!c->getType()) {
      assert(!catchAll);
      catchAll = catchBlock;
    }
  }

  // codegen try
  block = entryBlock;
  enterTryCatch(tc);
  process(x->getBody());
  exitTryCatch();

  // make sure we always get to finally block
  builder.SetInsertPoint(block);
  builder.CreateBr(tc.finallyBlock);

  // rethrow if uncaught
  builder.SetInsertPoint(unwindResumeBlock);
  builder.CreateResume(builder.CreateLoad(tc.catchStore));

  // make sure we delegate to parent try-catch if necessary
  std::vector<const types::Type *> catchTypesFull(tc.catchTypes);
  std::vector<llvm::BasicBlock *> handlersFull(tc.handlers);
  std::vector<unsigned> depths(tc.catchTypes.size(), 0);
  unsigned depth = 1;

  unsigned catchAllDepth = 0;
  for (auto it = trycatch.rbegin(); it != trycatch.rend(); ++it) {
    if (catchAll) // can't ever delegate past catch-all
      break;

    assert(it->catchTypes.size() == it->handlers.size());
    for (unsigned i = 0; i < it->catchTypes.size(); i++) {
      if (!anyMatch(it->catchTypes[i], catchTypesFull)) {
        catchTypesFull.push_back(it->catchTypes[i]);
        depths.push_back(depth);

        if (!it->catchTypes[i] && !catchAll) {
          // catch-all is in parent; set finally depth
          catchAll =
              llvm::BasicBlock::Create(context, "trycatch.fdepth_catchall", func);
          builder.SetInsertPoint(catchAll);
          builder.CreateStore(builder.getInt64(depth), tc.delegateDepth);
          builder.CreateBr(it->handlers[i]);
          handlersFull.push_back(catchAll);
          catchAllDepth = depth;
        } else {
          handlersFull.push_back(it->handlers[i]);
        }
      }
    }
    ++depth;
  }

  // exception handling
  builder.SetInsertPoint(tc.exceptionBlock);
  llvm::LandingPadInst *caughtResult =
      builder.CreateLandingPad(padType, catches.size());
  caughtResult->setCleanup(true);
  std::vector<llvm::Value *> typeIndices;

  for (auto *catchType : catchTypesFull) {
    assert(!catchType || cast<types::RefType>(catchType));
    const std::string typeVarName =
        "seq.typeidx." + (catchType ? catchType->getName() : "<all>");
    llvm::GlobalVariable *tidx = getTypeIdxVar(catchType);
    typeIndices.push_back(tidx);
    caughtResult->addClause(tidx);
  }

  llvm::Value *unwindException = builder.CreateExtractValue(caughtResult, 0);
  builder.CreateStore(caughtResult, tc.catchStore);
  builder.CreateStore(excStateThrown, tc.excFlag);
  llvm::Value *depthMax = builder.getInt64(trycatch.size());
  builder.CreateStore(depthMax, tc.delegateDepth);

  llvm::Value *unwindExceptionClass = builder.CreateLoad(builder.CreateStructGEP(
      unwindType,
      builder.CreatePointerCast(unwindException, unwindType->getPointerTo()), 0));

  // check for foreign exceptions
  builder.CreateCondBr(
      builder.CreateICmpEQ(unwindExceptionClass, builder.getInt64(seq_exc_class())),
      tc.exceptionRouteBlock, externalExcBlock);

  // external exception (currently assumed to be unreachable)
  builder.SetInsertPoint(externalExcBlock);
  builder.CreateUnreachable();

  // reroute Seq exceptions
  builder.SetInsertPoint(tc.exceptionRouteBlock);
  unwindException = builder.CreateExtractValue(builder.CreateLoad(tc.catchStore), 0);
  llvm::Value *excVal = builder.CreatePointerCast(
      builder.CreateConstGEP1_64(unwindException, (uint64_t)seq_exc_offset()),
      excType->getPointerTo());

  llvm::Value *loadedExc = builder.CreateLoad(excVal);
  llvm::Value *objType = builder.CreateExtractValue(loadedExc, 0);
  objType = builder.CreateExtractValue(objType, 0);
  llvm::Value *objPtr = builder.CreateExtractValue(loadedExc, 1);

  // set depth when catch-all entered
  auto *defaultRouteBlock = llvm::BasicBlock::Create(context, "trycatch.fdepth", func);
  builder.SetInsertPoint(defaultRouteBlock);
  if (catchAll)
    builder.CreateStore(builder.getInt64(catchAllDepth), tc.delegateDepth);
  builder.CreateBr(catchAll ? (catchAllDepth > 0 ? tc.finallyBlock : catchAll)
                            : tc.finallyBlock);

  builder.SetInsertPoint(tc.exceptionRouteBlock);
  llvm::SwitchInst *switchToCatchBlock =
      builder.CreateSwitch(objType, defaultRouteBlock, (unsigned)handlersFull.size());
  for (unsigned i = 0; i < handlersFull.size(); i++) {
    // set finally depth
    auto *depthSet = llvm::BasicBlock::Create(context, "trycatch.fdepth", func);
    builder.SetInsertPoint(depthSet);
    builder.CreateStore(builder.getInt64(depths[i]), tc.delegateDepth);
    builder.CreateBr((i < tc.handlers.size()) ? handlersFull[i] : tc.finallyBlock);

    if (catchTypesFull[i]) {
      switchToCatchBlock->addCase(
          builder.getInt32((uint64_t)getTypeIdx(catchTypesFull[i])), depthSet);
    }

    // codegen catch body if this block is ours (vs. a parent's)
    if (i < catches.size()) {
      block = handlersFull[i];
      builder.SetInsertPoint(block);
      Var *var = catches[i]->getVar();

      if (var) {
        llvm::Value *obj =
            builder.CreateBitCast(objPtr, getLLVMType(catches[i]->getType()));
        llvm::Value *varPtr = vars[var];
        assert(varPtr);
        builder.CreateStore(obj, varPtr);
      }

      builder.CreateStore(excStateCaught, tc.excFlag);
      process(catches[i]->getHandler());
      builder.SetInsertPoint(block);
      builder.CreateBr(tc.finallyBlock);
    }
  }

  block = endBlock;
}

void LLVMVisitor::visit(UnorderedFlow *x) {
  for (auto *flow : *x) {
    process(flow);
  }
}

/*
 * Instructions
 */

void LLVMVisitor::visit(AssignInstr *x) {
  llvm::Value *var = vars[x->getLhs()];
  assert(var);
  process(x->getRhs());
  llvm::IRBuilder<> builder(block);
  builder.CreateStore(value, var);
}

void LLVMVisitor::visit(ExtractInstr *x) {
  auto *memberedType = cast<types::MemberedType>(x->getVal()->getType());
  assert(memberedType);
  const int index = memberedType->getMemberIndex(x->getField());
  assert(index >= 0);

  process(x->getVal());
  llvm::IRBuilder<> builder(block);
  if (auto *refType = cast<types::RefType>(memberedType)) {
    value = builder.CreateBitCast(value,
                                  getLLVMType(refType->getContents())->getPointerTo());
    value = builder.CreateLoad(value);
  }
  value = builder.CreateExtractValue(value, index);
}

void LLVMVisitor::visit(InsertInstr *x) {
  auto *refType = cast<types::RefType>(x->getLhs()->getType());
  assert(refType);
  const int index = refType->getMemberIndex(x->getField());
  assert(index >= 0);

  process(x->getLhs());
  llvm::Value *lhs = value;
  process(x->getRhs());
  llvm::Value *rhs = value;

  llvm::IRBuilder<> builder(block);
  lhs = builder.CreateBitCast(lhs, getLLVMType(refType->getContents())->getPointerTo());
  llvm::Value *load = builder.CreateLoad(lhs);
  load = builder.CreateInsertValue(load, rhs, index);
  builder.CreateStore(load, lhs);
}

void LLVMVisitor::visit(CallInstr *x) {
  llvm::IRBuilder<> builder(block);
  process(x->getFunc());
  llvm::Value *f = value;

  std::vector<llvm::Value *> args;
  for (auto *arg : *x) {
    builder.SetInsertPoint(block);
    process(arg);
    args.push_back(value);
  }

  value = call(f, args);
}

void LLVMVisitor::visit(YieldInInstr *x) {
  llvm::IRBuilder<> builder(block);
  if (x->isSuspending()) {
    llvm::Function *coroSuspend =
        llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_suspend);
    llvm::Value *tok = llvm::ConstantTokenNone::get(context);
    llvm::Value *final = builder.getFalse();
    llvm::Value *susp = builder.CreateCall(coroSuspend, {tok, final});

    block = llvm::BasicBlock::Create(context, "yieldin.new", func);
    llvm::SwitchInst *inst = builder.CreateSwitch(susp, coro.suspend, 2);
    inst->addCase(builder.getInt8(0), block);
    inst->addCase(builder.getInt8(1), coro.cleanup);
    builder.SetInsertPoint(block);
  }
  value = builder.CreateLoad(coro.promise);
}

void LLVMVisitor::visit(StackAllocInstr *x) {
  llvm::Type *baseType = nullptr;
  if (auto *arrayType = cast<types::ArrayType>(x->getType())) {
    baseType = getLLVMType(arrayType->getBase());
  } else {
    assert(0 && "StackAllocInstr type is not an array type");
  }

  seq_int_t size = 0;
  if (auto *constSize = cast<IntConstant>(x->getCount())) {
    size = constSize->getVal();
  } else {
    assert(0 && "StackAllocInstr size is not constant");
  }

  llvm::IRBuilder<> builder(func->getEntryBlock().getTerminator());
  auto *arrType = llvm::StructType::get(builder.getInt64Ty(), baseType->getPointerTo());
  llvm::Value *len = builder.getInt64(size);
  llvm::Value *ptr = builder.CreateAlloca(baseType, len);
  llvm::Value *arr = llvm::UndefValue::get(arrType);
  arr = builder.CreateInsertValue(arr, len, 0);
  arr = builder.CreateInsertValue(arr, ptr, 1);
  value = arr;
}

void LLVMVisitor::visit(TernaryInstr *x) {
  auto *trueBlock = llvm::BasicBlock::Create(context, "ternary.true", func);
  auto *falseBlock = llvm::BasicBlock::Create(context, "ternary.false", func);
  auto *exitBlock = llvm::BasicBlock::Create(context, "ternary.exit", func);

  llvm::Type *valueType = getLLVMType(x->getType());
  process(x->getCond());
  llvm::Value *cond = value;

  llvm::IRBuilder<> builder(block);
  cond = builder.CreateTrunc(cond, builder.getInt1Ty());
  builder.CreateCondBr(cond, trueBlock, falseBlock);

  block = trueBlock;
  process(x->getTrueValue());
  llvm::Value *trueValue = value;
  trueBlock = block;
  builder.SetInsertPoint(trueBlock);
  builder.CreateBr(exitBlock);

  block = falseBlock;
  process(x->getFalseValue());
  llvm::Value *falseValue = value;
  falseBlock = block;
  builder.SetInsertPoint(falseBlock);
  builder.CreateBr(exitBlock);

  builder.SetInsertPoint(exitBlock);
  llvm::PHINode *phi = builder.CreatePHI(valueType, 2);
  phi->addIncoming(trueValue, trueBlock);
  phi->addIncoming(falseValue, falseBlock);
  value = phi;
  block = exitBlock;
}

void LLVMVisitor::visit(BreakInstr *x) {
  assert(!loops.empty());
  llvm::IRBuilder<> builder(block);
  if (auto *tc = getInnermostTryCatchBeforeLoop()) {
    auto *excStateBreak = builder.getInt8(TryCatchData::State::BREAK);
    builder.CreateStore(excStateBreak, tc->excFlag);
    builder.CreateBr(tc->finallyBlock);
  } else {
    builder.CreateBr(loops.back().breakBlock);
  }
  block = llvm::BasicBlock::Create(context, "break.new", func);
}

void LLVMVisitor::visit(ContinueInstr *x) {
  assert(!loops.empty());
  llvm::IRBuilder<> builder(block);
  if (auto *tc = getInnermostTryCatchBeforeLoop()) {
    auto *excStateContinue = builder.getInt8(TryCatchData::State::CONTINUE);
    builder.CreateStore(excStateContinue, tc->excFlag);
    builder.CreateBr(tc->finallyBlock);
  } else {
    builder.CreateBr(loops.back().continueBlock);
  }
  block = llvm::BasicBlock::Create(context, "continue.new", func);
}

void LLVMVisitor::visit(ReturnInstr *x) {
  const bool voidReturn = !bool(x->getValue());
  if (!voidReturn) {
    process(x->getValue());
  }
  llvm::IRBuilder<> builder(block);
  if (coro.exit) {
    builder.CreateBr(coro.exit);
  } else {
    if (auto *tc = getInnermostTryCatchBeforeLoop()) {
      auto *excStateReturn = builder.getInt8(TryCatchData::State::RETURN);
      builder.CreateStore(excStateReturn, tc->excFlag);
      if (tc->retStore) {
        assert(value);
        builder.CreateStore(value, tc->retStore);
      }
      builder.CreateBr(tc->finallyBlock);
    } else {
      if (!voidReturn) {
        builder.CreateRet(value);
      } else {
        builder.CreateRetVoid();
      }
    }
  }
  block = llvm::BasicBlock::Create(context, "return.new", func);
}

void LLVMVisitor::visit(YieldInstr *x) {
  process(x->getValue());
  makeYield(value);
}

void LLVMVisitor::visit(ThrowInstr *x) {
  // note: exception header should be set in the frontend
  llvm::Function *excAllocFunc = makeExcAllocFunc();
  llvm::Function *throwFunc = makeThrowFunc();
  process(x->getValue());
  llvm::IRBuilder<> builder(block);
  llvm::Value *exc = builder.CreateCall(
      excAllocFunc, {builder.getInt32(getTypeIdx(x->getValue()->getType())), value});
  call(throwFunc, exc);
}

void LLVMVisitor::visit(FlowInstr *x) {
  process(x->getFlow());
  process(x->getValue());
}

} // namespace ir
} // namespace seq
