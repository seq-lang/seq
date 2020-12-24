#include "sir/sir.h"
#include "util/llvm.h"
#include <stack>
#include <unordered_map>

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
} // namespace

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
  /// Whether we are compiling in debug mode
  bool debug;

  template <typename T> void process(const T &x) { x->accept(*this); }
  llvm::Type *getLLVMType(types::Type *x);
  llvm::Value *call(llvm::Value *callee, llvm::ArrayRef<llvm::Value *> args);
  void processLLVMFunc(Func *);
  void makeLLVMFunction(Func *);
  void makeYield(llvm::Value *value = nullptr, bool finalYield = false);
  void enterLoop(llvm::BasicBlock *breakBlock, llvm::BasicBlock *continueBlock);
  void exitLoop();

public:
  LLVMVisitor(bool debug = false);

  // void visit(IRModule *) override;
  void visit(Func *) override;
  // void visit(Var *) override;
  void visit(VarValue *) override;
  void visit(PointerValue *) override;
  // void visit(ValueProxy *) override;

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

LLVMVisitor::LLVMVisitor(bool debug)
    : util::SIRVisitor(), context(), module(), func(nullptr), block(nullptr),
      value(nullptr), type(nullptr), vars(), funcs(), coro(), loops(), debug(debug) {
  (void)(this->debug);
}

llvm::Type *LLVMVisitor::getLLVMType(types::Type *x) {
  process(x);
  return type;
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
 * General values, functions, vars
 */

void LLVMVisitor::processLLVMFunc(Func *x) {
  assert(x->isLLVM());

  auto *funcType = x->getType()->as<types::FuncType>();
  assert(funcType);
  llvm::Type *returnType = getLLVMType(funcType->getReturnType());

  std::vector<llvm::Type *> argTypes;
  for (auto *argType : *funcType) {
    argTypes.push_back(getLLVMType(argType));
  }

  std::string code;
  {
    std::string bufStr;
    llvm::raw_string_ostream buf(bufStr);

    buf << x->getLLVMDeclarations() << "\ndefine ";
    returnType->print(buf);
    buf << " @\"" << x->getName() << "\"(";

    assert(std::distance(x->arg_begin(), x->arg_end()) == argTypes.size());

    auto argTypeIter = argTypes.begin();
    auto argVarIter = x->arg_begin();

    while (argTypeIter != argTypes.end()) {
      (*argTypeIter)->print(buf);
      buf << " %" << (*argVarIter)->getName();
      if (argTypeIter < argTypes.end() - 1)
        buf << ", ";
      ++argTypeIter;
      ++argVarIter;
    }

    buf << ") {\n" << x->getLLVMBody() << "\n}";
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
  func = module->getFunction(x->getName());
  assert(func);
  func->addFnAttr(llvm::Attribute::AttrKind::AlwaysInline);
  funcs.insert(x, func);
}

void LLVMVisitor::makeLLVMFunction(Func *x) {
  auto *funcType = llvm::cast<llvm::FunctionType>(getLLVMType(x->getType()));
  func =
      llvm::cast<llvm::Function>(module->getOrInsertFunction(x->getName(), funcType));

  if (x->isExternal()) {
    func->setDoesNotThrow();
  } else {
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
  }
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

  block = llvm::BasicBlock::Create(context, "coro.continue", func);

  llvm::SwitchInst *inst = builder.CreateSwitch(suspendResult, coro.suspend, 2);
  inst->addCase(builder.getInt8(0), block);
  inst->addCase(builder.getInt8(1), coro.cleanup);
}

void LLVMVisitor::visit(Func *x) {
  if (auto *cached = module->getFunction(x->getName())) {
    assert(funcs[x]);
    func = llvm::cast<llvm::Function>(cached);
    return;
  }

  if (x->isLLVM()) {
    processLLVMFunc(x);
    return;
  }

  makeLLVMFunction(x);
  funcs.insert(x, func);

  if (x->isExternal()) {
    return;
  }

  if (x->isInternal()) {
    // TODO
    return;
  }

  auto *funcType = x->getType()->as<types::FuncType>();
  auto *returnType = funcType->getReturnType();
  assert(funcType);
  auto *entryBlock = llvm::BasicBlock::Create(context, "entry", func);
  llvm::IRBuilder<> builder(entryBlock);
  coro = {};

  // set up arguments and other symbols
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

  auto *start = llvm::BasicBlock::Create(context, "start", func);

  if (x->isGenerator()) {
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
    if (!returnType->is<types::VoidType>()) {
      coro.promise = makeAlloca(getLLVMType(returnType), entryBlock);
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
    builder.CreateCondBr(needAlloc, allocBlock, start);

    // coro alloc
    builder.SetInsertPoint(allocBlock);
    llvm::Value *size = builder.CreateCall(coroSize);
    auto *allocFunc = makeAllocFunc(module.get(), /*atomic=*/false);
    llvm::Value *alloc = builder.CreateCall(allocFunc, size);
    builder.CreateBr(start);

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

    // coro start
    builder.SetInsertPoint(start);
    llvm::PHINode *phi = builder.CreatePHI(builder.getInt8PtrTy(), 2);
    phi->addIncoming(nullPtr, entryBlock);
    phi->addIncoming(alloc, allocBlock);
    coro.handle = builder.CreateCall(coroBegin, {id, phi});
    coro.handle->setName("coro.handle");
    block = start;
    makeYield(); // coroutine will be initially suspended
  } else {
    builder.CreateBr(start);
    block = start;
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

void LLVMVisitor::visit(VarValue *x) {
  llvm::Value *var = vars[x->getVar()];
  assert(var);
  llvm::IRBuilder<> builder(block);
  value = builder.CreateLoad(var);
}

void LLVMVisitor::visit(PointerValue *x) {
  llvm::Value *var = vars[x->getVar()];
  assert(var);
  value = var; // note: we don't load the pointer
}

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

void LLVMVisitor::visit(types::OptionalType *x) {}

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
  str = builder.CreateInsertValue(str, ptr, 0);
  str = builder.CreateInsertValue(str, len, 1);
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
  process(x->getTrueBranch());
  builder.SetInsertPoint(block);
  builder.CreateBr(exitBlock);

  block = falseBlock;
  process(x->getFalseBranch());
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
  if (memberedType->is<types::RefType>()) {
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
  // TODO
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

  llvm::IRBuilder<> builder(&func->getEntryBlock());
  auto *arrType = llvm::StructType::get(builder.getInt64Ty(), baseType->getPointerTo());
  llvm::Value *len = builder.getInt64(size);
  llvm::Value *ptr = builder.CreateAlloca(baseType, len);
  llvm::Value *arr = llvm::UndefValue::get(arrType);
  arr = builder.CreateInsertValue(arr, ptr, 0);
  arr = builder.CreateInsertValue(arr, len, 1);
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
  builder.SetInsertPoint(block);
  builder.CreateBr(exitBlock);

  block = falseBlock;
  process(x->getFalseValue());
  llvm::Value *falseValue = value;
  builder.SetInsertPoint(block);
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
  const bool voidReturn = bool(x->getValue());
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
  /*
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

void LLVMVisitor::visit(FlowInstr *x) {
  process(x->getFlow());
  process(x->getValue());
}

} // namespace ir
} // namespace seq
