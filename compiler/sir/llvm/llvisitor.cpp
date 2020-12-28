#include "sir/llvm/llvisitor.h"

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

namespace seq {
namespace ir {
namespace {
// Our custom GC allocators
llvm::Function *makeAllocFunc(llvm::Module *module, bool atomic) {
  llvm::LLVMContext &context = module->getContext();
  auto *f = llvm::cast<llvm::Function>(module->getOrInsertFunction(
      atomic ? "seq_alloc_atomic" : "seq_alloc", llvm::Type::getInt8PtrTy(context),
      llvm::Type::getInt64Ty(context)));
  f->setDoesNotThrow();
  f->setReturnDoesNotAlias();
  f->setOnlyAccessesInaccessibleMemory();
  return f;
}

// Seq personality function for exception handling
llvm::Function *makePersonalityFunc(llvm::Module *module) {
  llvm::LLVMContext &context = module->getContext();
  return llvm::cast<llvm::Function>(module->getOrInsertFunction(
      "seq_personality", llvm::Type::getInt32Ty(context),
      llvm::Type::getInt32Ty(context), llvm::Type::getInt32Ty(context),
      llvm::Type::getInt64Ty(context), llvm::Type::getInt8PtrTy(context),
      llvm::Type::getInt8PtrTy(context)));
}

// Seq exception allocation function
llvm::Function *makeExcAllocFunc(llvm::Module *module) {
  llvm::LLVMContext &context = module->getContext();
  auto *f = llvm::cast<llvm::Function>(module->getOrInsertFunction(
      "seq_alloc_exc", llvm::Type::getInt8PtrTy(context),
      llvm::Type::getInt32Ty(context), llvm::Type::getInt8PtrTy(context)));
  f->setDoesNotThrow();
  return f;
}

// Seq exception throw function
llvm::Function *makeThrowFunc(llvm::Module *module) {
  llvm::LLVMContext &context = module->getContext();
  auto *f = llvm::cast<llvm::Function>(module->getOrInsertFunction(
      "seq_throw", llvm::Type::getVoidTy(context), llvm::Type::getInt8PtrTy(context)));
  f->setDoesNotReturn();
  return f;
}

// Seq program termination function
llvm::Function *makeTerminateFunc(llvm::Module *module) {
  llvm::LLVMContext &context = module->getContext();
  auto *f = llvm::cast<llvm::Function>(
      module->getOrInsertFunction("seq_terminate", llvm::Type::getVoidTy(context),
                                  llvm::Type::getInt8PtrTy(context)));
  f->setDoesNotReturn();
  return f;
}

void resetOMPABI() {
  IdentTy = nullptr;
  Kmpc_MicroTy = nullptr;
  DefaultOpenMPPSource = nullptr;
  DefaultOpenMPLocation = nullptr;
  KmpRoutineEntryPtrTy = nullptr;
}
} // namespace

LLVMVisitor::LLVMVisitor(bool debug)
    : util::SIRVisitor(), context(), module(), func(nullptr), block(nullptr),
      value(nullptr), type(nullptr), vars(), funcs(), coro(), loops(), internalFuncs(),
      debug(debug) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  resetOMPABI();
  initInternalFuncs();
}

void LLVMVisitor::validate() {
  auto fo = fopen("_dump.ll", "w");
  llvm::raw_fd_ostream fout(fileno(fo), true);
  fout << *module.get();
  fout.close();
  const bool broken = llvm::verifyModule(*module.get(), &llvm::errs());
  assert(!broken);
}

void LLVMVisitor::initInternalFuncs() {
  internalFuncs = {
      {"__elemsize__",
       [this](types::Type *parentType, std::vector<llvm::Value *> args) {
         assert(parentType->is<types::PointerType>());
         llvm::IRBuilder<> builder(block);
         return builder.getInt64(
             module->getDataLayout().getTypeAllocSize(getLLVMType(parentType)));
       }},

      {"__atomic__",
       [this](types::Type *parentType, std::vector<llvm::Value *> args) {
         assert(parentType->is<types::PointerType>());
         llvm::IRBuilder<> builder(block);
         return builder.getInt8(parentType->isAtomic() ? 1 : 0);
       }},

      {"__new__",
       [this](types::Type *parentType, std::vector<llvm::Value *> args) {
         assert(parentType->is<types::RefType>());
         llvm::IRBuilder<> builder(block);
         auto *allocFunc = makeAllocFunc(module.get(), parentType->isAtomic());
         llvm::Value *size = builder.getInt64(
             module->getDataLayout().getTypeAllocSize(getLLVMType(parentType)));
         return builder.CreateCall(allocFunc, size);
       }},
  };
}

llvm::Type *LLVMVisitor::getLLVMType(types::Type *x) {
  process(x);
  return type;
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

llvm::GlobalVariable *LLVMVisitor::getTypeIdxVar(const std::string &name) {
  auto *typeInfoType = getTypeInfoType();
  const std::string typeVarName = "seq.typeidx." + (name.empty() ? "<all>" : name);
  llvm::GlobalVariable *tidx = module->getGlobalVariable(typeVarName);
  int idx = 0; // types::Type::getID(name); // TODO
  if (!tidx)
    tidx = new llvm::GlobalVariable(
        *module.get(), typeInfoType, true, llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantStruct::get(
            typeInfoType, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context),
                                                 (uint64_t)idx, false)),
        typeVarName);
  return tidx;
}

llvm::GlobalVariable *LLVMVisitor::getTypeIdxVar(types::Type *catchType) {
  return getTypeIdxVar(catchType ? catchType->getName() : "");
}

llvm::Value *LLVMVisitor::call(llvm::Value *callee,
                               llvm::ArrayRef<llvm::Value *> args) {
  // TODO: check for invoke
  llvm::IRBuilder<> builder(block);
  return builder.CreateCall(callee, args);
}

void LLVMVisitor::enterLoop(llvm::BasicBlock *breakBlock,
                            llvm::BasicBlock *continueBlock) {
  loops.push({breakBlock, continueBlock});
}

void LLVMVisitor::exitLoop() {
  assert(!loops.empty());
  loops.pop();
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
  Var *argVar = x->getArgVar().get();
  llvm::Type *argVarType = getLLVMType(argVar->getType());
  auto *argStorage = new llvm::GlobalVariable(
      *module.get(), argVarType, false, llvm::GlobalValue::PrivateLinkage,
      llvm::Constant::getNullValue(argVarType), argVar->getName());
  vars.insert(argVar, argStorage);

  // set up global variables and initialize functions
  for (auto &var : *x) {
    if (auto *f = var->as<Func>()) {
      makeLLVMFunction(f);
      funcs.insert(f, func);
    } else {
      llvm::Type *llvmType = getLLVMType(var->getType());
      auto *storage = new llvm::GlobalVariable(
          *module.get(), llvmType, false, llvm::GlobalValue::PrivateLinkage,
          llvm::Constant::getNullValue(llvmType), var->getName());
      vars.insert(var.get(), storage);
    }
  }

  // process functions
  for (auto &var : *x) {
    if (auto *f = var->as<Func>()) {
      process(f);
    }
  }

  Func *main = x->getMainFunc().get();
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

  canonicalMainFunc->setPersonalityFn(makePersonalityFunc(module.get()));
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
  auto *allocFunc = makeAllocFunc(module.get(), /*atomic=*/false);
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
    proxyMain->setPersonalityFn(makePersonalityFunc(module.get()));
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
    builder.CreateCall(makeTerminateFunc(module.get()), unwindException);
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
  if (auto *llvmFunc = x->as<LLVMFunc>()) {
    process(llvmFunc);
    return;
  }

  auto *funcType = llvm::cast<llvm::FunctionType>(getLLVMType(x->getType()));
  assert(!module->getFunction(x->referenceString()));
  func = llvm::cast<llvm::Function>(
      module->getOrInsertFunction(x->referenceString(), funcType));
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
  func = module->getFunction(x->referenceString()); // inserted during module visit
  assert(func);
  func->setDoesNotThrow();
}

void LLVMVisitor::visit(InternalFunc *x) {
  func = module->getFunction(x->referenceString()); // inserted during module visit
  assert(func);
  func->addFnAttr(llvm::Attribute::AttrKind::AlwaysInline);
  block = llvm::BasicBlock::Create(context, "entry", func);
  llvm::IRBuilder<> builder(block);
  builder.CreateUnreachable();
  // TODO
  /*
  for (auto &internalFunc : internalFuncs) {
    if (internalFunc.name == x->getName()) {
      block = llvm::BasicBlock::Create(context, "entry", func);

      std::vector<llvm::Value *> args;
      for (auto it = func->arg_begin(); it != func->arg_end(); ++it) {
        args.push_back(it);
      }
      llvm::Value *result = internalFunc.codegen(x->getParentType(), args);

      llvm::IRBuilder<> builder(block);
      if (result) {
        builder.CreateRet(result);
      } else {
        builder.CreateRetVoid();
      }
      return;
    }
  }
  assert(0 && "internal function not found");
  */
}

void LLVMVisitor::visit(LLVMFunc *x) {
  func = module->getFunction(x->referenceString());
  if (func)
    return;
  auto *funcType = x->getType()->as<types::FuncType>();
  assert(funcType);

  // format body
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
  std::string body = fmt::vformat(x->getLLVMBody(), store);

  // build code
  std::string code;
  {
    std::string bufStr;
    llvm::raw_string_ostream buf(bufStr);

    buf << x->getLLVMDeclarations() << "\ndefine ";
    getLLVMType(funcType->getReturnType())->print(buf);
    buf << " @\"" << x->referenceString() << "\"(";

    const int numArgs = std::distance(x->arg_begin(), x->arg_end());
    int argIndex = 0;
    for (auto it = x->arg_begin(); it != x->arg_end(); ++it) {
      getLLVMType((*it)->getType())->print(buf);
      buf << " %" << (*it)->getName();
      if (argIndex < numArgs - 1)
        buf << ", ";
      ++argIndex;
    }

    buf << ") {\n" << body << "\n}";
    code = buf.str();
  }

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
  func = module->getFunction(x->referenceString());
  assert(func);
  func->addFnAttr(llvm::Attribute::AttrKind::AlwaysInline);
}

void LLVMVisitor::visit(BodiedFunc *x) {
  func = module->getFunction(x->referenceString()); // inserted during module visit
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
  func->setPersonalityFn(makePersonalityFunc(module.get()));

  auto *funcType = x->getType()->as<types::FuncType>();
  auto *returnType = funcType->getReturnType();
  assert(funcType);
  auto *entryBlock = llvm::BasicBlock::Create(context, "entry", func);
  llvm::IRBuilder<> builder(entryBlock);
  coro = {};

  // set up arguments and other symbols
  assert(std::distance(func->arg_begin(), func->arg_end()) ==
         std::distance(x->arg_begin(), x->arg_end()));
  auto argIter = func->arg_begin();
  for (auto varIter = x->arg_begin(); varIter != x->arg_end(); ++varIter) {
    Var *var = varIter->get();
    llvm::Value *storage = builder.CreateAlloca(getLLVMType(var->getType()));
    builder.CreateStore(argIter, storage);
    vars.insert(var, storage);
    ++argIter;
  }

  for (auto &symbol : *x) {
    llvm::Value *storage = builder.CreateAlloca(getLLVMType(symbol->getType()));
    vars.insert(symbol.get(), storage);
  }

  auto *startBlock = llvm::BasicBlock::Create(context, "start", func);

  if (x->isGenerator()) {
    auto *generatorType = returnType->as<types::GeneratorType>();
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
    if (!generatorType->getBase()->is<types::VoidType>()) {
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
    auto *allocFunc = makeAllocFunc(module.get(), /*atomic=*/false);
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
    if (returnType->is<types::VoidType>()) {
      builder.CreateRetVoid();
    } else {
      builder.CreateRet(llvm::Constant::getNullValue(getLLVMType(returnType)));
    }
  }
}

void LLVMVisitor::visit(Var *x) { assert(0); }

void LLVMVisitor::visit(VarValue *x) {
  if (auto *f = x->getVar()->as<Func>()) {
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

void LLVMVisitor::visit(types::IntType *x) { type = llvm::Type::getInt64Ty(context); }

void LLVMVisitor::visit(types::FloatType *x) {
  type = llvm::Type::getDoubleTy(context);
}

void LLVMVisitor::visit(types::BoolType *x) { type = llvm::Type::getInt8Ty(context); }

void LLVMVisitor::visit(types::ByteType *x) { type = llvm::Type::getInt8Ty(context); }

void LLVMVisitor::visit(types::VoidType *x) { type = llvm::Type::getVoidTy(context); }

void LLVMVisitor::visit(types::RecordType *x) {
  std::vector<llvm::Type *> body;
  for (const auto &field : *x) {
    body.push_back(getLLVMType(field.type));
  }
  type = llvm::StructType::get(context, body);
}

void LLVMVisitor::visit(types::RefType *x) { type = llvm::Type::getInt8PtrTy(context); }

void LLVMVisitor::visit(types::FuncType *x) {
  llvm::Type *returnType = getLLVMType(x->getReturnType());
  std::vector<llvm::Type *> argTypes;
  for (const auto &argType : *x) {
    argTypes.push_back(getLLVMType(argType));
  }
  type = llvm::FunctionType::get(returnType, argTypes, /*isVarArg=*/false);
}

void LLVMVisitor::visit(types::OptionalType *x) {
  if (x->getBase()->is<types::RefType>()) {
    type = llvm::Type::getInt8PtrTy(context);
  } else {
    type = llvm::StructType::get(llvm::Type::getInt1Ty(context),
                                 getLLVMType(x->getBase()));
  }
}

void LLVMVisitor::visit(types::ArrayType *x) {
  type = llvm::StructType::get(llvm::Type::getInt64Ty(context),
                               getLLVMType(x->getBase())->getPointerTo());
}

void LLVMVisitor::visit(types::PointerType *x) {
  type = getLLVMType(x->getBase())->getPointerTo();
}

void LLVMVisitor::visit(types::GeneratorType *x) {
  type = llvm::Type::getInt8PtrTy(context);
}

void LLVMVisitor::visit(types::IntNType *x) {
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
  for (const auto &value : *x) {
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
  enterLoop(/*breakBlock=*/exitBlock, /*continueBlock=*/condBlock);
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
  enterLoop(/*breakBlock=*/exitBlock, /*continueBlock=*/condBlock);
  process(x->getBody());
  exitLoop();
  builder.SetInsertPoint(block);
  builder.CreateBr(condBlock);

  builder.SetInsertPoint(cleanupBlock);
  builder.CreateCall(coroDestroy, iter);
  builder.CreateBr(exitBlock);

  block = exitBlock;
}

void LLVMVisitor::visit(TryCatchFlow *x) {
  // TODO
  process(x->getBody());
  process(x->getFinally());
}

void LLVMVisitor::visit(UnorderedFlow *x) {
  for (auto &flow : *x) {
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
  auto *memberedType = x->getVal()->getType()->as<types::MemberedType>();
  assert(memberedType);
  const int index = memberedType->getMemberIndex(x->getField());
  assert(index >= 0);

  process(x->getVal());
  llvm::IRBuilder<> builder(block);
  if (auto *refType = memberedType->as<types::RefType>()) {
    value = builder.CreateBitCast(value,
                                  getLLVMType(refType->getContents())->getPointerTo());
    value = builder.CreateLoad(value);
  }
  value = builder.CreateExtractValue(value, index);
}

void LLVMVisitor::visit(InsertInstr *x) {
  auto *refType = x->getLhs()->getType()->as<types::RefType>();
  assert(refType);
  const int index = refType->getMemberIndex(x->getField());
  assert(index >= 0);

  process(x->getLhs());
  llvm::Value *lhs = value;
  process(x->getRhs());
  llvm::Value *rhs = value;

  llvm::IRBuilder<> builder(block);
  llvm::Value *load = builder.CreateLoad(lhs);
  load = builder.CreateInsertValue(load, rhs, index);
  builder.CreateStore(load, lhs);
}

void LLVMVisitor::visit(CallInstr *x) {
  llvm::IRBuilder<> builder(block);
  process(x->getFunc());
  llvm::Value *func = value;

  std::vector<llvm::Value *> args;
  for (const ValuePtr &arg : *x) {
    builder.SetInsertPoint(block);
    process(arg);
    args.push_back(value);
  }

  value = call(func, args);
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
  if (auto *arrayType = x->getType()->as<types::ArrayType>()) {
    baseType = getLLVMType(arrayType->getBase());
  } else {
    assert(0 && "StackAllocInstr type is not an array type");
  }

  seq_int_t size = 0;
  if (auto *constSize = x->getCount()->as<IntConstant>()) {
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
  // TODO: check for finally block
  assert(!loops.empty());
  llvm::IRBuilder<> builder(block);
  builder.CreateBr(loops.top().breakBlock);
  block = llvm::BasicBlock::Create(context, "break.new", func);
}

void LLVMVisitor::visit(ContinueInstr *x) {
  // TODO: check for finally block
  assert(!loops.empty());
  llvm::IRBuilder<> builder(block);
  builder.CreateBr(loops.top().continueBlock);
  block = llvm::BasicBlock::Create(context, "continue.new", func);
}

void LLVMVisitor::visit(ReturnInstr *x) {
  // TODO: check for finally block
  const bool voidReturn = !bool(x->getValue());
  if (!voidReturn) {
    process(x->getValue());
  }
  llvm::IRBuilder<> builder(block);
  if (!voidReturn) {
    builder.CreateRet(value);
  } else {
    builder.CreateRetVoid();
  }
  block = llvm::BasicBlock::Create(context, "return.new", func);
}

void LLVMVisitor::visit(YieldInstr *x) {
  process(x->getValue());
  makeYield(value);
}

void LLVMVisitor::visit(ThrowInstr *x) {
  /* TODO
    LLVMContext &context = block->getContext();
    Module *module = block->getModule();
    Function *excAllocFunc = makeExcAllocFunc(module);
    Function *throwFunc = makeThrowFunc(module);

    Value *obj = expr->codegen(getBase(), block);
    IRBuilder<> builder(block);

    // add meta-info like func/file/line/col
    Type *hdrType = excHeader->getLLVMType(context);
    Value *hdrPtr = builder.CreateBitCast(obj, hdrType->getPointerTo());
    Value *hdr = builder.CreateLoad(hdrPtr);

    std::string funcNameStr = "<main>";
    if (auto *func = dynamic_cast<Func *>(getBase())) {
      funcNameStr = func->genericName();
    }
    std::string fileNameStr = getSrcInfo().file;
    seq_int_t fileLine = getSrcInfo().line;
    seq_int_t fileCol = getSrcInfo().col;

    StrExpr funcNameExpr(funcNameStr);
    Value *funcNameVal = funcNameExpr.codegen(getBase(), block);

    StrExpr fileNameExpr(fileNameStr);
    Value *fileNameVal = fileNameExpr.codegen(getBase(), block);

    Value *fileLineVal = ConstantInt::get(seqIntLLVM(context), fileLine, true);
    Value *fileColVal = ConstantInt::get(seqIntLLVM(context), fileCol, true);

    hdr = excHeader->setMemb(hdr, "3", funcNameVal, block);
    hdr = excHeader->setMemb(hdr, "4", fileNameVal, block);
    hdr = excHeader->setMemb(hdr, "5", fileLineVal, block);
    hdr = excHeader->setMemb(hdr, "6", fileColVal, block);
    builder.SetInsertPoint(block);
    builder.CreateStore(hdr, hdrPtr);

    Value *exc = builder.CreateCall(excAllocFunc,
                                    {ConstantInt::get(Type::getInt32Ty(context),
                                                      (uint64_t)type->getID(), true),
                                     obj});

    if (getTryCatch()) {
      Function *parent = block->getParent();
      BasicBlock *unwind = getTryCatch()->getExceptionBlock();
      BasicBlock *normal = BasicBlock::Create(context, "normal", parent);
      builder.CreateInvoke(throwFunc, normal, unwind, exc);
      block = normal;
    } else {
      builder.CreateCall(throwFunc, exc);
    }
  */
}

void LLVMVisitor::visit(AssertInstr *x) {
  // TODO
}

void LLVMVisitor::visit(FlowInstr *x) {
  process(x->getFlow());
  process(x->getValue());
}

} // namespace ir
} // namespace seq
