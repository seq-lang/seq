#include "lang/seq.h"

using namespace seq;
using namespace llvm;

BaseFunc::BaseFunc()
    : parentType(nullptr), module(nullptr), preambleBlock(nullptr),
      func(nullptr) {}

bool BaseFunc::isGen() { return false; }

void BaseFunc::resolveTypes() {}

LLVMContext &BaseFunc::getContext() {
  assert(module);
  return module->getContext();
}

BasicBlock *BaseFunc::getPreamble() const {
  assert(preambleBlock);
  return preambleBlock;
}

types::FuncType *BaseFunc::getFuncType() {
  resolveTypes();
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

BaseFunc *BaseFunc::clone(Generic *ref) { return this; }

Func::Func()
    : BaseFunc(), Generic(), SrcObject(), external(false), name(), inTypes(),
      outType(types::Void), outType0(types::Void), defaultArgs(),
      scope(new Block()), argNames(), argVars(), attributes(),
      parentFunc(nullptr), ret(nullptr), yield(nullptr), prefetch(false),
      interAlign(false), resolved(false), cache(), gen(false), promise(nullptr),
      handle(nullptr), cleanup(nullptr), suspend(nullptr) {
  if (!this->argNames.empty())
    assert(this->argNames.size() == this->inTypes.size());
}

Block *Func::getBlock() { return scope; }

std::string Func::genericName() { return name; }

void Func::addCachedRealized(std::vector<types::Type *> types,
                             seq::Generic *x) {
  cache.add(types, dynamic_cast<Func *>(x));
}

Func *Func::realize(std::vector<types::Type *> types) {
  Func *cached = cache.find(types);

  if (cached)
    return cached;

  Generic *x = realizeGeneric(types);
  auto *func = dynamic_cast<Func *>(x);
  assert(func);
  addCachedRealized(types, func);
  func->resolveTypes();
  return func;
}

std::vector<types::Type *>
Func::deduceTypesFromArgTypes(std::vector<types::Type *> argTypes) {
  return Generic::deduceTypesFromArgTypes(inTypes, argTypes);
}

std::vector<Expr *> Func::rectifyCallArgs(std::vector<Expr *> args,
                                          std::vector<std::string> names,
                                          bool methodCall) {
#define ENSURE_NO_DUP(i)                                                       \
  if (argsFixed[i] || partials[i])                                             \
  throw exc::SeqException("multiple values given for '" + argNames[i] +        \
                          "' parameter")

  const unsigned offset = methodCall ? 1 : 0; // handle 'self'
  const unsigned size = inTypes.size();
  assert(defaultArgs.empty() || defaultArgs.size() == size);
  assert(argNames.size() == size);
  if (names.empty())
    names = std::vector<std::string>(args.size(), "");
  assert(args.size() == names.size());

  bool sawName = false;
  for (unsigned i = 0; i < args.size(); i++) {
    // disallow unnamed args after named args
    if (names[i].empty()) {
      assert(!sawName);
    } else {
      sawName = true;
    }
  }

  const unsigned argsGot = args.size() + offset;
  if (argsGot > size) {
    throw exc::SeqException("expected " + std::to_string(size) +
                            " argument(s), but got " + std::to_string(argsGot));
  }

  bool hasDefaults = false;
  for (auto *e : defaultArgs) {
    if (e) {
      hasDefaults = true;
      break;
    }
  }

  std::vector<Expr *> argsFixed(size, nullptr);
  // explicit partial args ("...") are given as null arguments; they are denoted
  // here
  std::vector<bool> partials(size, false);

  // first, deal with named args:
  for (unsigned i = 0; i < args.size(); i++) {
    if (!names[i].empty()) {
      unsigned name_idx = 0;
      for (unsigned j = 0; j < size; j++) {
        if (argNames[j] == names[i]) {
          name_idx = j;
          break;
        }
      }

      if (argNames[name_idx] == names[i]) {
        ENSURE_NO_DUP(name_idx);
        argsFixed[name_idx] = args[i];
        if (!args[i])
          partials[name_idx] = true;
      } else {
        throw exc::SeqException("no function argument named '" + names[i] +
                                "'");
      }
    }
  }

  // now fill in regular args:
  if (hasDefaults) {
    // left to right for functions with defaults
    unsigned next = offset;
    for (unsigned i = 0; i < args.size(); i++) {
      if (!names[i].empty())
        continue;

      assert(next < size);
      ENSURE_NO_DUP(next);
      argsFixed[next++] = args[i];
    }
  } else {
    // right to left otherwise, to support implicit partials
    int j = (int)args.size() - 1;
    while (j >= 0 && !names[j].empty())
      --j;
    for (int i = (int)size - 1; i >= 0; i--) {
      if (j < 0)
        break;

      if (!argsFixed[i] && !partials[i])
        argsFixed[i] = args[j--];
    }
    assert(j < 0); // i.e. no unused args
  }

  // fill in defaults:
  if (!defaultArgs.empty()) {
    for (unsigned i = offset; i < size; i++) {
      // the second condition checks for explicit nulls, which indicate a
      // partial call; we don't want to override these with the default argument
      if (!argsFixed[i] && !partials[i]) {
        argsFixed[i] = defaultArgs[i];
      }
    }
  }

  // implicitly convert generator arguments:
  for (unsigned i = 0; i < size; i++) {
    if (argsFixed[i] && inTypes[i]->asGen() &&
        argsFixed[i]->getType()->hasMethod("__iter__"))
      argsFixed[i] =
          new CallExpr(new GetElemExpr(argsFixed[i], "__iter__"), {});
  }

  if (offset)
    argsFixed = std::vector<Expr *>(argsFixed.begin() + 1, argsFixed.end());

  return argsFixed;
#undef ENSURE_NO_DUP
}

void Func::setEnclosingFunc(Func *parentFunc) { this->parentFunc = parentFunc; }

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
  outType = types::GenType::get(outType);
  outType0 = types::GenType::get(outType0);
}

void Func::addAttribute(std::string attr) {
  attributes.push_back(attr);

  if (attr == "builtin") {
    builtins[genericName()] = this;
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

  resolveTypes();
  std::string mangled = name;

  if (numGenerics() > 0) {
    mangled += "[";
    for (unsigned i = 0; i < numGenerics(); i++) {
      mangled += getGeneric(i)->getName();
      if (i < numGenerics() - 1)
        mangled += ",";
    }
    mangled += "]";
  }

  if (parentFunc)
    mangled = parentFunc->getMangledFuncName() + "::" + mangled;

  if (parentType)
    mangled = parentType->getName() + "::" + mangled;

  types::FuncType *funcType = getFuncType();
  for (unsigned i = 0; i < funcType->numBaseTypes(); i++)
    mangled += "." + funcType->getBaseType(i)->getName();

  return mangled;
}

void Func::resolveTypes() {
  if ((prefetch || interAlign) && yield)
    throw exc::SeqException("prefetch statement cannot be used in generator");

  if (external || resolved)
    return;

  resolved = true;

  try {
    for (Expr *defaultArg : defaultArgs)
      if (defaultArg)
        defaultArg->resolveTypes();
    scope->resolveTypes();

    // return type deduction
    if ((outType->is(types::Void) ||
         outType->is(types::GenType::get(types::Void))) &&
        (yield || (ret && ret->getExpr()))) {
      if (yield) {
        outType = types::GenType::get(
            yield->getExpr() ? yield->getExpr()->getType() : types::Void);
      } else if (ret) {
        outType = ret->getExpr() ? ret->getExpr()->getType() : types::Void;

        if (prefetch)
          outType = types::GenType::get(outType,
                                        types::GenType::GenTypeKind::PREFETCH);
      } else {
        assert(0);
      }
    }
  } catch (exc::SeqException &) {
    /*
     * Function had some generic types which could not be resolved yet; not a
     * real issue though, since these will be resolved whenever the generics are
     * instantiated, so we catch this exception and ignore it.
     */
    resolved = false;
  }
}

void Func::codegen(Module *module) {
  if (this->module != module) {
    func = nullptr;
    this->module = module;
  }

  if (func)
    return;

  resolveTypes();
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
    if (numGenerics() > 0)
      throw exc::SeqException("cannot export generic function", getSrcInfo());
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
  resolveTypes();
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

Func *Func::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (Func *)ref->getClone(this);

  auto *x = new Func();
  ref->addClone(this, x);
  setCloneBase(x, ref);

  std::vector<types::Type *> inTypesCloned;
  for (auto *type : inTypes)
    inTypesCloned.push_back(type->clone(ref));

  std::vector<Expr *> defaultArgsCloned;
  for (auto *expr : defaultArgs)
    defaultArgsCloned.push_back(expr ? expr->clone(ref) : nullptr);

  x->external = external;
  x->name = name;
  x->argNames = argNames;
  x->inTypes = inTypesCloned;
  x->outType = x->outType0 = outType0->clone(ref);
  x->defaultArgs = defaultArgsCloned;
  x->scope = scope->clone(ref);
  x->attributes = attributes;

  std::map<std::string, Var *> argVarsCloned;
  for (auto &e : argVars)
    argVarsCloned.insert({e.first, e.second->clone(ref)});
  x->argVars = argVarsCloned;

  if (parentType)
    x->parentType = parentType->clone(ref);
  if (parentFunc)
    x->parentFunc = parentFunc->clone(ref);
  if (ret)
    x->ret = ret->clone(ref);
  if (yield)
    x->yield = yield->clone(ref);
  x->prefetch = prefetch;
  x->interAlign = interAlign;
  x->gen = gen;
  x->setSrcInfo(getSrcInfo());
  return x;
}

std::unordered_map<std::string, Func *> Func::builtins = {};
Func *Func::getBuiltin(const std::string &name) {
  auto itr = builtins.find(name);
  assert(itr != builtins.end());
  return itr->second;
}

BaseFuncLite::BaseFuncLite(
    std::vector<types::Type *> inTypes, types::Type *outType,
    std::function<llvm::Function *(llvm::Module *)> codegenLambda)
    : BaseFunc(), inTypes(std::move(inTypes)), outType(outType),
      codegenLambda(std::move(codegenLambda)) {}

void BaseFuncLite::codegen(Module *module) {
  resolveTypes();
  func = codegenLambda(module);
  preambleBlock = &*func->getBasicBlockList().begin();
  module = preambleBlock->getModule();
}

types::FuncType *BaseFuncLite::getFuncType() {
  resolveTypes();
  return types::FuncType::get(inTypes, outType);
}

BaseFuncLite *BaseFuncLite::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (BaseFuncLite *)ref->getClone(this);

  std::vector<types::Type *> inTypesCloned;

  for (auto *type : inTypes)
    inTypesCloned.push_back(type->clone(ref));

  auto *x = new BaseFuncLite(inTypesCloned, outType->clone(ref), codegenLambda);
  ref->addClone(this, x);

  return x;
}
