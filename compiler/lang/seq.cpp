#include "lang/seq.h"
#include "parser/common.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <system_error>

using namespace seq;
using namespace llvm;

#if LLVM_VERSION_MAJOR >= 7
#include "llvm/CodeGen/CommandFlags.inc"
#else
using namespace llvm::orc;
#include "llvm/CodeGen/CommandFlags.def"
#endif

config::Config::Config() : context(), debug(false), profile(false) {}

config::Config &seq::config::config() {
  static Config config;
  return config;
}

SeqModule::SeqModule()
    : BaseFunc(), scope(new Block()),
      argVar(new Var(types::ArrayType::get(types::Str))), initFunc(nullptr),
      strlenFunc(nullptr) {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();

  module = new Module("seq", config::config().context);
  module->setTargetTriple(EngineBuilder().selectTarget()->getTargetTriple().str());
  module->setDataLayout(EngineBuilder().selectTarget()->createDataLayout());
  argVar->setGlobal();
}

Block *SeqModule::getBlock() { return scope; }

Var *SeqModule::getArgVar() { return argVar; }

void SeqModule::setFileName(std::string file) { module->setSourceFileName(file); }

static void invokeMain(Function *main, BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  Function *func = block->getParent();
  Module *module = func->getParent();
  BasicBlock *normal = BasicBlock::Create(context, "normal", func);
  BasicBlock *unwind = BasicBlock::Create(context, "unwind", func);
  IRBuilder<> builder(block);
  builder.CreateInvoke(main, normal, unwind);

  builder.SetInsertPoint(unwind);
  Function *term = makeTerminateFunc(module);
  LandingPadInst *caughtResult =
      builder.CreateLandingPad(TryCatch::getPadType(context), 1);
  caughtResult->setCleanup(true);
  caughtResult->addClause(TryCatch::getTypeIdxVar(module, nullptr));
  Value *unwindException = builder.CreateExtractValue(caughtResult, 0);
  builder.CreateCall(term, unwindException);
  builder.CreateUnreachable();

  block = normal;
}

Function *SeqModule::makeCanonicalMainFunc(Function *realMain) {
#define LLVM_I32() IntegerType::getInt32Ty(context)
  LLVMContext &context = realMain->getContext();
  Module *module = realMain->getParent();

  types::ArrayType *arrType = types::ArrayType::get(types::Str);

  auto *func = cast<Function>(module->getOrInsertFunction(
      "main", LLVM_I32(), LLVM_I32(),
      PointerType::get(IntegerType::getInt8PtrTy(context), 0)));

  func->setPersonalityFn(makePersonalityFunc(module));
  auto argiter = func->arg_begin();
  Value *argc = argiter++;
  Value *argv = argiter;
  argc->setName("argc");
  argv->setName("argv");

  BasicBlock *entry = BasicBlock::Create(context, "entry", func);
  BasicBlock *loop = BasicBlock::Create(context, "loop", func);

  IRBuilder<> builder(entry);
  Value *len = builder.CreateZExt(argc, seqIntLLVM(context));
  Value *ptr = types::Str->alloc(len, entry);
  Value *arr = arrType->make(ptr, len, entry);
  builder.CreateBr(loop);

  builder.SetInsertPoint(loop);
  PHINode *control = builder.CreatePHI(LLVM_I32(), 2, "i");
  Value *next = builder.CreateAdd(control, ConstantInt::get(LLVM_I32(), 1), "next");
  Value *cond = builder.CreateICmpSLT(control, argc);

  BasicBlock *body = BasicBlock::Create(context, "body", func);
  BranchInst *branch =
      builder.CreateCondBr(cond, body, body); // we set false-branch below

  builder.SetInsertPoint(body);
  Value *arg = builder.CreateLoad(builder.CreateGEP(argv, control));
  Value *argLen = builder.CreateZExtOrTrunc(builder.CreateCall(strlenFunc, arg),
                                            seqIntLLVM(context));
  Value *str = types::Str->make(arg, argLen, body);
  Value *idx = builder.CreateZExt(control, types::Int->getLLVMType(context));
  arrType->callMagic("__setitem__", {types::Int, types::Str}, arr, {idx, str}, body,
                     nullptr);
  builder.CreateBr(loop);

  control->addIncoming(ConstantInt::get(LLVM_I32(), 0), entry);
  control->addIncoming(next, body);

  BasicBlock *exit = BasicBlock::Create(context, "exit", func);
  branch->setSuccessor(1, exit);
  getArgVar()->store(this, arr, exit);
  builder.SetInsertPoint(exit);
  builder.CreateCall(initFunc);

#if SEQ_HAS_TAPIR
  /*
   * Put the entire program in a parallel+single region
   */
  {
    getOrCreateKmpc_MicroTy(context);
    getOrCreateIdentTy(module);
    getOrCreateDefaultLocation(module);

    auto *IdentTyPtrTy = getIdentTyPointerTy();

    Type *forkParams[] = {IdentTyPtrTy, LLVM_I32(),
                          getKmpc_MicroPointerTy(module->getContext())};
    FunctionType *forkFnTy =
        FunctionType::get(Type::getVoidTy(context), forkParams, true);
    auto *forkFunc =
        cast<Function>(module->getOrInsertFunction("__kmpc_fork_call", forkFnTy));

    Type *singleParams[] = {IdentTyPtrTy, LLVM_I32()};
    FunctionType *singleFnTy = FunctionType::get(LLVM_I32(), singleParams, false);
    auto *singleFunc =
        cast<Function>(module->getOrInsertFunction("__kmpc_single", singleFnTy));

    Type *singleEndParams[] = {IdentTyPtrTy, LLVM_I32()};
    FunctionType *singleEndFnTy =
        FunctionType::get(Type::getVoidTy(context), singleEndParams, false);
    auto *singleEndFunc =
        cast<Function>(module->getOrInsertFunction("__kmpc_end_single", singleEndFnTy));

    // make the proxy main function that will be called by __kmpc_fork_call:
    std::vector<Type *> proxyArgs = {PointerType::get(LLVM_I32(), 0),
                                     PointerType::get(LLVM_I32(), 0)};
    auto *proxyMainTy = FunctionType::get(Type::getVoidTy(context), proxyArgs, false);
    auto *proxyMain =
        cast<Function>(module->getOrInsertFunction("seq.proxy_main", proxyMainTy));
    proxyMain->setLinkage(GlobalValue::PrivateLinkage);
    proxyMain->setPersonalityFn(makePersonalityFunc(module));
    BasicBlock *proxyBlockEntry = BasicBlock::Create(context, "entry", proxyMain);
    BasicBlock *proxyBlockMain = BasicBlock::Create(context, "main", proxyMain);
    BasicBlock *proxyBlockExit = BasicBlock::Create(context, "exit", proxyMain);
    builder.SetInsertPoint(proxyBlockEntry);

    Value *tid = proxyMain->arg_begin();
    tid = builder.CreateLoad(tid);
    Value *singleCall = builder.CreateCall(singleFunc, {DefaultOpenMPLocation, tid});
    Value *shouldExit = builder.CreateICmpEQ(singleCall, builder.getInt32(0));
    builder.CreateCondBr(shouldExit, proxyBlockExit, proxyBlockMain);

    builder.SetInsertPoint(proxyBlockExit);
    builder.CreateRetVoid();

    invokeMain(realMain, proxyBlockMain);
    builder.SetInsertPoint(proxyBlockMain);
    builder.CreateCall(singleEndFunc, {DefaultOpenMPLocation, tid});
    builder.CreateRetVoid();

    // actually make the fork call:
    std::vector<Value *> forkArgs = {
        DefaultOpenMPLocation, builder.getInt32(0),
        builder.CreateBitCast(proxyMain, getKmpc_MicroPointerTy(context))};
    builder.SetInsertPoint(exit);
    builder.CreateCall(forkFunc, forkArgs);

    // finally, tell Tapir to NOT create its own parallel regions, as we've done
    // it here:
    fastOpenMP.setValue(true);
  }
#else
  invokeMain(realMain, exit);
#endif

  builder.SetInsertPoint(exit);
  builder.CreateRet(ConstantInt::get(LLVM_I32(), 0));
  return func;
#undef LLVM_I32
}

void SeqModule::codegen(Module *module) {
  assert(module);

  if (func)
    return;

  LLVMContext &context = module->getContext();
  this->module = module;

  func =
      cast<Function>(module->getOrInsertFunction("seq.main", Type::getVoidTy(context)));

  func->setLinkage(GlobalValue::PrivateLinkage);
  func->setPersonalityFn(makePersonalityFunc(module));

  /* preamble */
  preambleBlock = BasicBlock::Create(context, "preamble", func);
  IRBuilder<> builder(preambleBlock);

  initFunc =
      cast<Function>(module->getOrInsertFunction("seq_init", Type::getVoidTy(context)));
  initFunc->setCallingConv(CallingConv::C);

  strlenFunc = cast<Function>(module->getOrInsertFunction(
      "strlen", seqIntLLVM(context), IntegerType::getInt8PtrTy(context)));
  strlenFunc->setCallingConv(CallingConv::C);

  BasicBlock *entry = BasicBlock::Create(context, "entry", func);
  BasicBlock *block = entry;

  scope->codegen(block);

  builder.SetInsertPoint(block);
  builder.CreateRetVoid();

  builder.SetInsertPoint(preambleBlock);
  builder.CreateBr(entry);

  func = makeCanonicalMainFunc(func);
}

static void verifyModuleFailFast(Module &module) {
  if (verifyModule(module, &errs())) {
    auto fo = fopen("llvm.dump", "w");
    raw_fd_ostream fout(fileno(fo), true);
    fout << module;
    fout.close();
    assert(0);
  }
}

void SeqModule::verify() { verifyModuleFailFast(*module); }

static TargetMachine *getTargetMachine(Triple triple, StringRef cpuStr,
                                       StringRef featuresStr,
                                       const TargetOptions &options) {
  std::string err;
  const Target *target = TargetRegistry::lookupTarget(MArch, triple, err);

  if (!target)
    return nullptr;

  return target->createTargetMachine(triple.getTriple(), cpuStr, featuresStr, options,
                                     getRelocModel(), getCodeModel(),
                                     CodeGenOpt::Aggressive);
}

static void applyDebugTransformations(Module *module) {
  if (!config::config().debug && !config::config().profile)
    return;
  // remove tail calls and fix linkage for stack traces
  for (Function &f : *module) {
    f.setLinkage(GlobalValue::ExternalLinkage);
    if (f.hasFnAttribute(Attribute::AttrKind::AlwaysInline))
      f.removeFnAttr(Attribute::AttrKind::AlwaysInline);
    f.addFnAttr(Attribute::AttrKind::NoInline);
    f.setHasUWTable();
    f.addFnAttr("no-frame-pointer-elim", "true");
    f.addFnAttr("no-frame-pointer-elim-non-leaf");
    f.addFnAttr("no-jump-tables", "false");

    for (BasicBlock &block : f.getBasicBlockList()) {
      for (Instruction &inst : block) {
        if (CallInst *call = dyn_cast<CallInst>(&inst)) {
          call->setTailCall(false);
        }
      }
    }
  }
}

static void applyGCTransformations(Module *module) {
  LLVMContext &context = module->getContext();
  auto *addRoots = cast<Function>(module->getOrInsertFunction(
      "seq_gc_add_roots", Type::getVoidTy(context), IntegerType::getInt8PtrTy(context),
      IntegerType::getInt8PtrTy(context)));
  addRoots->setDoesNotThrow();

  // insert add_roots calls where needed
  for (Function &f : *module) {
    for (BasicBlock &block : f.getBasicBlockList()) {
      for (Instruction &inst : block) {
        if (CallInst *call = dyn_cast<CallInst>(&inst)) {
          if (Function *g = call->getCalledFunction()) {
            // tell GC about OpenMP's allocation
            if (g->getName() == "__kmpc_omp_task_alloc") {
              Value *taskSize = call->getArgOperand(3);
              Value *sharedSize = call->getArgOperand(4);
              IRBuilder<> builder(call->getNextNode());
              Value *baseOffset = builder.CreateSub(taskSize, sharedSize);
              Value *ptr = builder.CreateBitCast(call, builder.getInt8PtrTy());
              Value *lo = builder.CreateGEP(ptr, baseOffset);
              Value *hi = builder.CreateGEP(ptr, taskSize);
              builder.CreateCall(addRoots, {lo, hi});
            }
          }
        }
      }
    }
  }
}

static void optimizeModule(Module *module) {
  const bool debug = config::config().debug;
  applyDebugTransformations(module);
  std::unique_ptr<legacy::PassManager> pm(new legacy::PassManager());
  std::unique_ptr<legacy::FunctionPassManager> fpm(
      new legacy::FunctionPassManager(module));

  Triple moduleTriple(module->getTargetTriple());
  std::string cpuStr, featuresStr;
  TargetMachine *machine = nullptr;
  const TargetOptions options = InitTargetOptionsFromCodeGenFlags();
  TargetLibraryInfoImpl tlii(moduleTriple);
  pm->add(new TargetLibraryInfoWrapperPass(tlii));

  if (moduleTriple.getArch()) {
    cpuStr = getCPUStr();
    featuresStr = getFeaturesStr();
    machine = getTargetMachine(moduleTriple, cpuStr, featuresStr, options);
  }

  std::unique_ptr<TargetMachine> tm(machine);
  setFunctionAttributes(cpuStr, featuresStr, *module);
  pm->add(createTargetTransformInfoWrapperPass(tm ? tm->getTargetIRAnalysis()
                                                  : TargetIRAnalysis()));
  fpm->add(createTargetTransformInfoWrapperPass(tm ? tm->getTargetIRAnalysis()
                                                   : TargetIRAnalysis()));

  if (tm) {
    auto &ltm = dynamic_cast<LLVMTargetMachine &>(*tm);
    Pass *tpc = ltm.createPassConfig(*pm);
    pm->add(tpc);
  }

  unsigned optLevel = 3;
  unsigned sizeLevel = 0;
  PassManagerBuilder builder;

#if SEQ_HAS_TAPIR
  static OpenMPABI omp;
  builder.tapirTarget = &omp;
#endif

  if (!debug) {
    builder.OptLevel = optLevel;
    builder.SizeLevel = sizeLevel;
    builder.Inliner = createFunctionInliningPass(optLevel, sizeLevel, false);
    builder.DisableUnitAtATime = false;
    builder.DisableUnrollLoops = false;
    builder.LoopVectorize = true;
    builder.SLPVectorize = true;
  }

  if (tm)
    tm->adjustPassManager(builder);

  addCoroutinePassesToExtensionPoints(builder);
  builder.populateModulePassManager(*pm);
  builder.populateFunctionPassManager(*fpm);

  fpm->doInitialization();
  for (Function &f : *module)
    fpm->run(f);
  fpm->doFinalization();
  pm->run(*module);
  applyDebugTransformations(module);
}

void SeqModule::optimize() { optimizeModule(module); }

void SeqModule::runCodegenPipeline() {
  codegen(module);
  verify();
  optimize();
  applyGCTransformations(module);
  verify();
  optimize();
  verify();
#if SEQ_HAS_TAPIR
  tapir::resetOMPABI();
#endif
}

void SeqModule::compile(const std::string &out) {
  runCodegenPipeline();
  std::error_code err;
  raw_fd_ostream stream(out, err, llvm::sys::fs::F_None);

#if LLVM_VERSION_MAJOR >= 7
  WriteBitcodeToFile(*module, stream);
#else
  WriteBitcodeToFile(module, stream);
#endif

  module = nullptr;

  if (err) {
    std::cerr << "error: " << err.message() << std::endl;
    exit(err.value());
  }
}

extern "C" void seq_gc_add_roots(void *start, void *end);
extern "C" void seq_gc_remove_roots(void *start, void *end);
extern "C" void seq_add_symbol(void *addr, const std::string &symbol);
namespace {
/**
 * Simple extension of LLVM's SectionMemoryManager which catches data section
 * allocations and registers them with the GC. This allows the GC to know not
 * to collect globals even in JIT mode.
 */
class BoehmGCMemoryManager : public SectionMemoryManager {
private:
  /// Vector of (start, end) address pairs registered with GC.
  std::vector<std::pair<void *, void *>> roots;

  uint8_t *allocateDataSection(uintptr_t size, unsigned alignment, unsigned sectionID,
                               StringRef sectionName, bool isReadOnly) override {
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

void SeqModule::execute(const std::vector<std::string> &args,
                        const std::vector<std::string> &libs) {
  const bool debug = config::config().debug;
  runCodegenPipeline();
  std::vector<std::string> functionNames;
  if (debug) {
    for (Function &f : *module) {
      functionNames.push_back(f.getName());
    }
  }

  std::unique_ptr<Module> owner(module);
  module = nullptr;
  EngineBuilder EB(std::move(owner));
  EB.setMCJITMemoryManager(make_unique<BoehmGCMemoryManager>());
  EB.setUseOrcMCJITReplacement(true);
  ExecutionEngine *eng = EB.create();

  assert(initFunc);
  assert(strlenFunc);
  eng->addGlobalMapping(initFunc, (void *)seq_init);
  eng->addGlobalMapping(strlenFunc, (void *)strlen);

  std::string err;
  for (auto &lib : libs) {
    if (sys::DynamicLibrary::LoadLibraryPermanently(lib.c_str(), &err)) {
      std::cerr << "error: " << err << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  if (debug) {
    for (const std::string &name : functionNames) {
      void *addr = eng->getPointerToNamedFunction(name, /*AbortOnFailure=*/false);
      if (addr)
        seq_add_symbol(addr, name);
    }
  }

  eng->runFunctionAsMain(func, args, nullptr);
  delete eng;
}

#if SEQ_HAS_TAPIR
void tapir::resetOMPABI() {
  IdentTy = nullptr;
  Kmpc_MicroTy = nullptr;
  DefaultOpenMPPSource = nullptr;
  DefaultOpenMPLocation = nullptr;
  KmpRoutineEntryPtrTy = nullptr;
}
#endif

/*
 * JIT
 */
#if LLVM_VERSION_MAJOR == 6
static std::shared_ptr<Module> optimizeModule(std::shared_ptr<Module> module) {
  optimizeModule(module.get());
  verifyModuleFailFast(*module);
  return module;
}

SeqJIT::SeqJIT()
    : target(EngineBuilder().selectTarget()), layout(target->createDataLayout()),
      objLayer([]() { return std::make_shared<BoehmGCMemoryManager>(); }),
      comLayer(objLayer, SimpleCompiler(*target)),
      optLayer(comLayer,
               [](std::shared_ptr<Module> M) { return optimizeModule(std::move(M)); }),
      globals(), inputNum(0) {
  sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
}

void SeqJIT::init() {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
}

std::unique_ptr<Module> SeqJIT::makeModule() {
  auto module = llvm::make_unique<Module>("seq." + std::to_string(inputNum),
                                          config::config().context);
  module->setTargetTriple(target->getTargetTriple().str());
  module->setDataLayout(target->createDataLayout());
  return module;
}

SeqJIT::ModuleHandle SeqJIT::addModule(std::unique_ptr<Module> module) {
  auto resolver = createLambdaResolver(
      [&](const std::string &name) {
        if (auto sym = optLayer.findSymbol(name, false))
          return sym;
        return JITSymbol(nullptr);
      },
      [](const std::string &name) {
        if (auto symAddr = RTDyldMemoryManager::getSymbolAddressInProcess(name))
          return JITSymbol(symAddr, JITSymbolFlags::Exported);
        return JITSymbol(nullptr);
      });
  return cantFail(optLayer.addModule(std::move(module), std::move(resolver)));
}

JITSymbol SeqJIT::findSymbol(std::string name) {
  std::string mangledName;
  raw_string_ostream mangledNameStream(mangledName);
  Mangler::getNameWithPrefix(mangledNameStream, name, layout);
  return optLayer.findSymbol(mangledNameStream.str(), false);
}

void SeqJIT::removeModule(SeqJIT::ModuleHandle handle) {
  cantFail(optLayer.removeModule(handle));
}

Func *SeqJIT::makeFunc() {
  auto func = new Func();
  func->setName("seq.repl.input." + std::to_string(inputNum));
  func->setIns({});
  func->setOut(types::Void);
  return func;
}

void SeqJIT::exec(Func *func, std::unique_ptr<Module> module) {
  LLVMContext &context = config::config().context;
  for (auto *var : globals)
    var->reset();
  Function *f = func->getFunc(module.get());
  f->setLinkage(GlobalValue::ExternalLinkage);

  // expose globals to the new function:
  IRBuilder<> builder(context);
  builder.SetInsertPoint(&*(*f->getBasicBlockList().begin()).begin());
  for (auto *var : globals) {
    Value *ptr = var->getPtr(func);
    auto sym = findSymbol(var->getName());
    auto addr = (uint64_t)cantFail(sym.getAddress());
    Value *addrVal = ConstantInt::get(seqIntLLVM(context), addr);
    Value *ptrVal = builder.CreateIntToPtr(
        addrVal, var->getType()->getLLVMType(context)->getPointerTo());
    builder.CreateStore(ptrVal, ptr);
  }

  verifyModuleFailFast(*module);
  addModule(std::move(module));
  auto sym = findSymbol(func->genericName());
  void (*fn)() = (void (*)())cantFail(sym.getAddress());
  fn();
}

void SeqJIT::addFunc(Func *func) {
  auto module = makeModule();
  func->setName("seq.repl.input." + std::to_string(inputNum));
  exec(func, std::move(module));
  ++inputNum;
}

void SeqJIT::addExpr(Expr *expr, bool print) {
  auto module = makeModule();
  auto func = makeFunc();
  if (print) {
    auto *p1 = new Print(expr);
    auto *p2 = new Print(new StrExpr("\n"));
    p1->setBase(func);
    p2->setBase(func);
    func->getBlock()->add(p1);
    func->getBlock()->add(p2);
  } else {
    auto *e = new ExprStmt(expr);
    e->setBase(func);
    func->getBlock()->add(e);
  }

  exec(func, std::move(module));
  ++inputNum;
}

Var *SeqJIT::addVar(Expr *expr) {
  auto module = makeModule();
  auto func = makeFunc();
  auto *v = new VarStmt(expr);
  Var *var = v->getVar();
  var->setGlobal();
  var->setExternal();
  v->setBase(func);
  func->getBlock()->add(v);

  exec(func, std::move(module));
  var->setREPL();
  globals.push_back(var);
  ++inputNum;
  return var;
}

void SeqJIT::delVar(Var *var) {
  auto it = std::find(globals.begin(), globals.end(), var);
  if (it != globals.end())
    globals.erase(it);
}
#endif

void seq::compilationMessage(const std::string &header, const std::string &msg,
                             const std::string &file, int line, int col) {
  assert(!(file.empty() && (line > 0 || col > 0)));
  assert(!(col > 0 && line <= 0));
  std::cerr << "\033[1m";
  if (!file.empty())
    std::cerr << file; //.substr(file.rfind('/') + 1);
  if (line > 0)
    std::cerr << ":" << line;
  if (col > 0)
    std::cerr << ":" << col;
  if (!file.empty())
    std::cerr << ": ";
  std::cerr << header << "\033[1m " << msg << "\033[0m" << std::endl;
}

void seq::compilationError(const std::string &msg, const std::string &file, int line,
                           int col) {
  compilationMessage("\033[1;31merror:\033[0m", msg, file, line, col);
  exit(EXIT_FAILURE);
}

void seq::compilationWarning(const std::string &msg, const std::string &file, int line,
                             int col) {
  compilationMessage("\033[1;33mwarning:\033[0m", msg, file, line, col);
}

seq_int_t seq::translateIndex(seq_int_t idx, seq_int_t len, bool clamp) {
  if (idx < 0)
    idx += len;

  if (clamp) {
    if (idx < 0)
      idx = 0;
    if (idx > len)
      idx = len;
  } else if (idx < 0 || idx >= len) {
    throw exc::SeqException("tuple index " + std::to_string(idx) +
                            " out of bounds (len: " + std::to_string(len) + ")");
  }

  return idx;
}

// adapted from Python's PySlice_AdjustIndices
seq_int_t seq::sliceAdjustIndices(seq_int_t length, seq_int_t *start, seq_int_t *stop,
                                  seq_int_t step) {
  if (step == 0)
    throw exc::SeqException("slice step cannot be 0");

  if (*start < 0) {
    *start += length;
    if (*start < 0) {
      *start = (step < 0) ? -1 : 0;
    }
  } else if (*start >= length) {
    *start = (step < 0) ? length - 1 : length;
  }

  if (*stop < 0) {
    *stop += length;
    if (*stop < 0) {
      *stop = (step < 0) ? -1 : 0;
    }
  } else if (*stop >= length) {
    *stop = (step < 0) ? length - 1 : length;
  }

  if (step < 0) {
    if (*stop < *start) {
      return (*start - *stop - 1) / (-step) + 1;
    }
  } else {
    if (*start < *stop) {
      return (*stop - *start - 1) / step + 1;
    }
  }
  return 0;
}
