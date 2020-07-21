#include "lang/seq.h"

using namespace seq;
using namespace llvm;

BaseFunc::BaseFunc()
    : parentType(nullptr), module(nullptr), preambleBlock(nullptr),
      func(nullptr) {}

bool BaseFunc::isGen() { return false; }

LLVMContext &BaseFunc::getContext() {
  assert(module);
  return module->getContext();
}

BasicBlock *BaseFunc::getPreamble() const {
  assert(preambleBlock);
  return preambleBlock;
}

types::FuncType *BaseFunc::getFuncType() {
  return types::FuncType::get({}, types::Void);
}

Function *BaseFunc::getFunc(Module *module) {
  codegen(module);
  assert(func);
  return func;
}

void BaseFunc::setEnclosingClass(types::Type *parentType) {
  this->parentType = parentType;
}

Func::Func()
    : BaseFunc(), SrcObject(), external(false), name(), inTypes(),
      outType(types::Void), outType0(types::Void), defaultArgs(),
      scope(new Block()), argNames(), argVars(), attributes(),
      parentFunc(nullptr), ret(nullptr), yield(nullptr), prefetch(false),
      interAlign(false), gen(false), promise(nullptr), handle(nullptr),
      cleanup(nullptr), suspend(nullptr) {
  if (!this->argNames.empty())
    assert(this->argNames.size() == this->inTypes.size());
}

Block *Func::getBlock() { return scope; }

std::string Func::genericName() { return name; }

void Func::setEnclosingFunc(BaseFunc *parentFunc) {
  auto p = dynamic_cast<seq::Func *>(parentFunc);
  assert(p);
  this->parentFunc = p;
}

void Func::sawReturn(Return *ret) {
  if (interAlign && ret->getExpr())
    throw exc::SeqException(
        "functions performing inter-sequence alignment cannot return a value",
        getSrcInfo());
  if (this->ret)
    return;

  this->ret = ret;
}

void Func::sawYield(Yield *yield) {
  if (interAlign)
    throw exc::SeqException(
        "functions performing inter-sequence alignment cannot be generators",
        getSrcInfo());
  if (this->yield)
    return;

  this->yield = yield;
  gen = true;
  // outType = types::GenType::get(outType);
  // outType0 = types::GenType::get(outType0);
}

void Func::addAttribute(std::string attr) {
  attributes.push_back(attr);

  if (attr == "builtin") {
    auto name = genericName();
    auto i = name.find('['); // chop off realization part
    if (i != std::string::npos)
      name = name.substr(0, i);
    builtins[name] = this;
  } else if (attr == "prefetch") {
    if (prefetch)
      return;
    if (interAlign)
      throw exc::SeqException(
          "function cannot perform both prefetch and inter-sequence alignment",
          getSrcInfo());

    prefetch = true;
    gen = true;
    outType =
        types::GenType::get(outType, types::GenType::GenTypeKind::PREFETCH);
    outType0 =
        types::GenType::get(outType0, types::GenType::GenTypeKind::PREFETCH);
  } else if (attr == "inter_align") {
    if (interAlign)
      return;
    if (prefetch)
      throw exc::SeqException(
          "function cannot perform both prefetch and inter-sequence alignment",
          getSrcInfo());
    if (!(outType->is(types::Void) && outType0->is(types::Void)))
      throw exc::SeqException("functions performing inter-sequence alignment "
                              "cannot return a value",
                              getSrcInfo());
    interAlign = true;
    gen = true;
    types::RecordType *yieldType = PipeExpr::getInterAlignYieldType();
    outType =
        types::GenType::get(yieldType, types::GenType::GenTypeKind::INTERALIGN);
    outType0 =
        types::GenType::get(yieldType, types::GenType::GenTypeKind::INTERALIGN);
  }
}

std::vector<std::string> Func::getAttributes() { return attributes; }

bool Func::hasAttribute(const std::string &attr) {
  for (const std::string &a : attributes) {
    if (a == attr)
      return true;
  }
  return false;
}

/*
 * Mangling rules:
 *   - Base function name is mangled as "<name>[<generic type 1>,<generic type
 * 2>,(...),<generic type N>]" or simply "<name>" if function is not generic.
 *   - If function is nested in function g, "<mangled name of g>::" is prepended
 * to the name.
 *   - If function is method of class C, "<type name of C>::" is prepended to
 * the name.
 *   - ".<out type>.<arg type 1>.<arg type 2>.(...).<arg type N>" is appended to
 * the name.
 */
std::string Func::getMangledFuncName() {
  // don't mangle external, built-in ("seq."-prefixed) or exported functions:
  if (external || name.rfind("seq.", 0) == 0 || hasAttribute("export"))
    return name;

  // a nested function can't be a class method:
  assert(!(parentType && parentFunc));
  std::string mangled = name;

  if (parentFunc)
    mangled = parentFunc->getMangledFuncName() + "::" + mangled;

  if (parentType)
    mangled = parentType->getName() + "::" + mangled;

  types::FuncType *funcType = getFuncType();
  for (unsigned i = 0; i < funcType->numBaseTypes(); i++)
    mangled += "." + funcType->getBaseType(i)->getName();

  return mangled;
}

void Func::codegen(Module *module) {
  if (this->module != module) {
    func = nullptr;
    this->module = module;
  }

  if (func)
    return;

  LLVMContext &context = module->getContext();
  std::vector<Type *> types;
  for (auto *type : inTypes)
    types.push_back(type->getLLVMType(context));

  std::string mangledName = getMangledFuncName();
  auto *cached = module->getFunction(mangledName);
  if (cached) {
    func = cast<Function>(cached);
    return;
  }

  FunctionType *funcTypeLLVM =
      FunctionType::get(outType->getLLVMType(context), types, false);
  func = cast<Function>(module->getOrInsertFunction(mangledName, funcTypeLLVM));

  if (external)
    return;

  if (hasAttribute("export")) {
    if (parentType || parentFunc)
      throw exc::SeqException("can only export top-level functions",
                              getSrcInfo());
    func->setLinkage(GlobalValue::ExternalLinkage);
  } else {
    func->setLinkage(GlobalValue::PrivateLinkage);
  }
  if (hasAttribute("inline")) {
    if (hasAttribute("noinline"))
      throw exc::SeqException(
          "function cannot be marked 'inline' and 'noinline'", getSrcInfo());
    func->addFnAttr(Attribute::AttrKind::AlwaysInline);
  }
  if (hasAttribute("noinline")) {
    func->addFnAttr(Attribute::AttrKind::NoInline);
  }
  if (config::config().profile) {
    func->addFnAttr("xray-instruction-threshold", "200");
  }
  func->setPersonalityFn(makePersonalityFunc(module));
  preambleBlock = BasicBlock::Create(context, "preamble", func);
  IRBuilder<> builder(preambleBlock);

  /*
   * Set up general generator intrinsics, if indeed a generator
   */
  Value *id = nullptr;
  if (gen) {
    Function *idFn = Intrinsic::getDeclaration(module, Intrinsic::coro_id);
    Value *nullPtr =
        ConstantPointerNull::get(IntegerType::getInt8PtrTy(context));

    if (!outType->getBaseType(0)->is(types::Void)) {
      promise = makeAlloca(outType->getBaseType(0)->getLLVMType(context),
                           preambleBlock);
      promise->setName("promise");
      Value *promiseRaw =
          builder.CreateBitCast(promise, IntegerType::getInt8PtrTy(context));
      id = builder.CreateCall(
          idFn, {ConstantInt::get(IntegerType::getInt32Ty(context), 0),
                 promiseRaw, nullPtr, nullPtr});
    } else {
      id = builder.CreateCall(
          idFn, {ConstantInt::get(IntegerType::getInt32Ty(context), 0), nullPtr,
                 nullPtr, nullPtr});
    }
    id->setName("id");
  }

  assert(argNames.empty() || argNames.size() == inTypes.size());
  auto argsIter = func->arg_begin();
  for (auto &argName : argNames) {
    auto iter = argVars.find(argName);
    assert(iter != argVars.end());
    iter->second->store(this, argsIter, preambleBlock);
    ++argsIter;
  }

  BasicBlock *allocBlock = nullptr;
  Value *alloc = nullptr;
  if (gen) {
    allocBlock = BasicBlock::Create(context, "alloc", func);
    builder.SetInsertPoint(allocBlock);
    Function *sizeFn = Intrinsic::getDeclaration(module, Intrinsic::coro_size,
                                                 {seqIntLLVM(context)});
    Value *size = builder.CreateCall(sizeFn);
    auto *allocFunc = makeAllocFunc(module, false);
    alloc = builder.CreateCall(allocFunc, size);
  }

  BasicBlock *entry = BasicBlock::Create(context, "entry", func);
  BasicBlock *entryActual = entry;
  BasicBlock *dynFree = nullptr;

  if (gen) {
    builder.CreateBr(entry);
    builder.SetInsertPoint(entry);
    PHINode *phi = builder.CreatePHI(IntegerType::getInt8PtrTy(context), 2);
    phi->addIncoming(
        ConstantPointerNull::get(IntegerType::getInt8PtrTy(context)),
        preambleBlock);
    phi->addIncoming(alloc, allocBlock);

    Function *beginFn =
        Intrinsic::getDeclaration(module, Intrinsic::coro_begin);
    handle = builder.CreateCall(beginFn, {id, phi});
    handle->setName("hdl");

    /*
     * Cleanup code
     */
    cleanup = BasicBlock::Create(context, "cleanup", func);
    dynFree = BasicBlock::Create(context, "dyn_free", func);
    builder.SetInsertPoint(cleanup);
    Function *freeFn = Intrinsic::getDeclaration(module, Intrinsic::coro_free);
    Value *mem = builder.CreateCall(freeFn, {id, handle});
    Value *needDynFree = builder.CreateIsNotNull(mem);

    suspend = BasicBlock::Create(context, "suspend", func);
    builder.CreateCondBr(needDynFree, dynFree, suspend);

    builder.SetInsertPoint(dynFree);
    builder.CreateBr(suspend);

    builder.SetInsertPoint(suspend);
    Function *endFn = Intrinsic::getDeclaration(module, Intrinsic::coro_end);
    builder.CreateCall(
        endFn, {handle, ConstantInt::get(IntegerType::getInt1Ty(context), 0)});
    builder.CreateRet(handle);

    exit = BasicBlock::Create(context, "final", func);
  }

  builder.SetInsertPoint(entry);

  if (gen) {
    // make sure the generator is initially suspended:
    codegenYield(nullptr, outType->getBaseType(0), entry);
  }

  BasicBlock *block = entry;
  scope->codegen(block);

  BasicBlock *exitBlock = block;
  builder.SetInsertPoint(exitBlock);

  if (gen) {
    builder.CreateBr(exit);
    codegenYield(nullptr, nullptr, exit); // final yield
  } else {
    if (outType->is(types::Void)) {
      builder.CreateRetVoid();
    } else {
      // i.e. if there isn't already a return at the end
      if (scope->stmts.empty() ||
          !dynamic_cast<Return *>(scope->stmts.back())) {
        builder.CreateRet(outType->defaultValue(exitBlock));
      } else {
        builder.CreateUnreachable();
      }
    }
  }

  builder.SetInsertPoint(preambleBlock);
  if (gen) {
    Function *allocFn =
        Intrinsic::getDeclaration(module, Intrinsic::coro_alloc);
    Value *needAlloc = builder.CreateCall(allocFn, id);
    builder.CreateCondBr(needAlloc, allocBlock, entryActual);

    exit->moveAfter(&func->getBasicBlockList().back());
    cleanup->moveAfter(exit);
    dynFree->moveAfter(cleanup);
    suspend->moveAfter(dynFree);
  } else {
    builder.CreateBr(entry);
  }
}

void Func::codegenReturn(Value *val, types::Type *type, BasicBlock *&block,
                         bool dryrun) {
  if (prefetch || interAlign) {
    codegenYield(val, type, block, false, dryrun);
  } else {
    if (gen) {
      if (val)
        throw exc::SeqException("cannot return value from generator");
    } else {
      if ((val && type && !types::is(type, outType)) ||
          (!val && !outType->is(types::Void)))
        throw exc::SeqException("cannot return '" + type->getName() +
                                "' from function returning '" +
                                outType->getName() + "'");

      if (val && type && type->is(types::Void))
        throw exc::SeqException("cannot return void value from function");
    }
  }

  if (dryrun)
    return;

  IRBuilder<> builder(block);

  if (gen) {
    builder.CreateBr(exit);
  } else {
    if (val) {
      builder.CreateRet(val);
    } else {
      builder.CreateRetVoid();
    }
  }

  /*
   * Can't have anything after the `ret` instruction we just added,
   * so make a new block and return that to the caller.
   */
  block = BasicBlock::Create(block->getContext(), "", block->getParent());
}

// type = nullptr means final yield; empty yields used internally only in
// prefetch transformations
void Func::codegenYield(Value *val, types::Type *type, BasicBlock *&block,
                        bool empty, bool dryrun) {
  if (!gen)
    throw exc::SeqException("cannot yield from a non-generator");

  if (!empty && type && !types::is(type, outType->getBaseType(0)))
    throw exc::SeqException("cannot yield '" + type->getName() +
                            "' from generator yielding '" +
                            outType->getBaseType(0)->getName() + "'");

  if (!empty && val && type && type->is(types::Void))
    throw exc::SeqException("cannot yield void value from generator");

  if (dryrun)
    return;

  LLVMContext &context = block->getContext();
  IRBuilder<> builder(block);

  if (val) {
    assert(promise);
    builder.CreateStore(val, promise);
  }

  Function *suspFn = Intrinsic::getDeclaration(module, Intrinsic::coro_suspend);
  Value *tok = ConstantTokenNone::get(context);
  Value *final = ConstantInt::get(IntegerType::getInt1Ty(context),
                                  (empty || type) ? 0 : 1);
  Value *susp = builder.CreateCall(suspFn, {tok, final});

  /*
   * Can't have anything after the `ret` instruction we just added,
   * so make a new block and return that to the caller.
   */
  block = BasicBlock::Create(block->getContext(), "", block->getParent());

  SwitchInst *inst = builder.CreateSwitch(susp, suspend, 2);
  inst->addCase(ConstantInt::get(IntegerType::getInt8Ty(context), 0), block);
  inst->addCase(ConstantInt::get(IntegerType::getInt8Ty(context), 1), cleanup);

  if (!empty && !type) {
    builder.SetInsertPoint(block);
    builder.CreateUnreachable();
  }
}

Value *Func::codegenYieldExpr(BasicBlock *&block, bool suspend) {
  if (!gen)
    throw exc::SeqException("yield expression in non-generator");

  IRBuilder<> builder(block);
  if (suspend) {
    LLVMContext &context = block->getContext();
    Function *suspFn =
        Intrinsic::getDeclaration(module, Intrinsic::coro_suspend);
    Value *tok = ConstantTokenNone::get(context);
    Value *final = ConstantInt::get(IntegerType::getInt1Ty(context), 0);
    Value *susp = builder.CreateCall(suspFn, {tok, final});

    block = BasicBlock::Create(block->getContext(), "", block->getParent());

    SwitchInst *inst = builder.CreateSwitch(susp, this->suspend, 2);
    inst->addCase(ConstantInt::get(IntegerType::getInt8Ty(context), 0), block);
    inst->addCase(ConstantInt::get(IntegerType::getInt8Ty(context), 1),
                  cleanup);
  }

  builder.SetInsertPoint(block);
  return builder.CreateLoad(promise);
}

bool Func::isGen() { return yield != nullptr; }

std::vector<std::string> Func::getArgNames() { return argNames; }

Var *Func::getArgVar(std::string name) {
  auto iter = argVars.find(name);
  assert(iter != argVars.end());
  return iter->second;
}

types::FuncType *Func::getFuncType() {
  return types::FuncType::get(inTypes, outType);
}

void Func::setExternal() { external = true; }

void Func::setIns(std::vector<types::Type *> inTypes) {
  this->inTypes = std::move(inTypes);
}

void Func::setOut(types::Type *outType) {
  if (interAlign && !outType->is(types::Void))
    throw exc::SeqException(
        "functions performing inter-sequence alignment cannot return a value",
        getSrcInfo());
  this->outType = outType0 = outType;
}

void Func::setDefaults(std::vector<Expr *> defaultArgs) {
  this->defaultArgs = std::move(defaultArgs);
}

void Func::setName(std::string name) { this->name = std::move(name); }

void Func::setArgNames(std::vector<std::string> argNames) {
  this->argNames = std::move(argNames);
  assert(inTypes.size() == this->argNames.size());

  argVars.clear();
  for (unsigned i = 0; i < this->argNames.size(); i++)
    argVars.insert({this->argNames[i], new Var(inTypes[i])});
}

std::unordered_map<std::string, Func *> Func::builtins = {};
Func *Func::getBuiltin(const std::string &name) {
  auto itr = builtins.find(name);

  if (itr == builtins.end()) {
    LOG("[ariya] can't find builtin {}", name);
    assert(false);
  }
  return itr->second;
}

BaseFuncLite::BaseFuncLite(
    std::vector<types::Type *> inTypes, types::Type *outType,
    std::function<llvm::Function *(llvm::Module *)> codegenLambda)
    : BaseFunc(), inTypes(std::move(inTypes)), outType(outType),
      codegenLambda(std::move(codegenLambda)) {}

void BaseFuncLite::codegen(Module *module) {
  func = codegenLambda(module);
  preambleBlock = &*func->getBasicBlockList().begin();
  module = preambleBlock->getModule();
}

types::FuncType *BaseFuncLite::getFuncType() {
  return types::FuncType::get(inTypes, outType);
}
