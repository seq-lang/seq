#include "lang/seq.h"

using namespace seq;
using namespace llvm;

types::FuncType::FuncType(std::vector<types::Type *> inTypes,
                          types::Type *outType)
    : Type("function", BaseType::get()), inTypes(std::move(inTypes)),
      outType(outType) {}

unsigned types::FuncType::argCount() const { return (unsigned)inTypes.size(); }

static types::OptionalType *asOpt(types::Type *type) {
  return dynamic_cast<types::OptionalType *>(type);
}

Value *types::FuncType::call(BaseFunc *base, Value *self,
                             const std::vector<Value *> &args,
                             BasicBlock *block, BasicBlock *normal,
                             BasicBlock *unwind) {
  LLVMContext &context = block->getContext();
  std::vector<Value *> argsFixed;
  assert(args.size() == inTypes.size());
  for (unsigned i = 0; i < args.size(); i++) {
    // implicit optional conversion allows cases like foo(x, y, z, None)
    if (types::OptionalType *opt = ::asOpt(inTypes[i])) {
      Value *arg = dyn_cast<ConstantPointerNull>(args[i]) ? nullptr : args[i];
      if (arg) {
        llvm::Type *t1 = opt->getBaseType(0)->getLLVMType(context);
        llvm::Type *t2 = arg->getType();
        // this can only happen when passing a variable None as a POD optional,
        // since the type checker allows 'NoneType' arguments on any optional.
        if (t1 != t2)
          arg = nullptr;
      }
      argsFixed.push_back(opt->make(arg, block));
    } else {
      argsFixed.push_back(args[i]);
    }
  }

  IRBuilder<> builder(block);
  return normal ? (Value *)builder.CreateInvoke(self, normal, unwind, argsFixed)
                : builder.CreateCall(self, argsFixed);
}

Value *types::FuncType::defaultValue(BasicBlock *block) {
  return ConstantPointerNull::get(
      cast<PointerType>(getLLVMType(block->getContext())));
}

static Value *codegenStr(Value *self, const std::string &name,
                         BasicBlock *block) {
  LLVMContext &context = block->getContext();
  IRBuilder<> builder(block);
  Value *ptr = builder.CreateBitCast(self, builder.getInt8PtrTy());

  Func *strFunc = Func::getBuiltin("_raw_type_str");
  FuncExpr strsRealExpr(strFunc);
  ValueExpr ptrVal(types::PtrType::get(types::Byte), ptr);

  GlobalVariable *nameVar = new GlobalVariable(
      *block->getModule(),
      llvm::ArrayType::get(builder.getInt8Ty(), name.length() + 1), true,
      GlobalValue::PrivateLinkage, ConstantDataArray::getString(context, name),
      "typename_literal");
  nameVar->setAlignment(1);

  Value *str = builder.CreateBitCast(nameVar, builder.getInt8PtrTy());
  Value *len = ConstantInt::get(seqIntLLVM(context), name.length());

  ValueExpr nameVal(types::Str, types::Str->make(str, len, block));
  CallExpr strFuncCall(&strsRealExpr, {&ptrVal, &nameVal});
  strFuncCall.resolveTypes();
  return strFuncCall.codegen(nullptr, block);
}

void types::FuncType::initOps() {
  if (!vtable.magic.empty())
    return;

  vtable.magic = {
      {"__new__",
       {PtrType::get(Byte)},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateBitCast(args[0], getLLVMType(b.getContext()));
       },
       true},

      {"__str__",
       {},
       Str,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return codegenStr(self, "function", b.GetInsertBlock());
       },
       false},
  };
}

bool types::FuncType::is(Type *type) const {
  auto *fnType = dynamic_cast<FuncType *>(type);

  if (!fnType || !types::is(outType, fnType->outType) ||
      inTypes.size() != fnType->inTypes.size())
    return false;

  for (unsigned i = 0; i < inTypes.size(); i++)
    if (!types::is(inTypes[i], fnType->inTypes[i]))
      return false;

  return true;
}

unsigned types::FuncType::numBaseTypes() const { return 1 + argCount(); }

types::Type *types::FuncType::getBaseType(unsigned idx) const {
  return idx ? inTypes[idx - 1] : outType;
}

static bool compatibleArgType(types::Type *got, types::Type *exp) {
  if (::asOpt(exp))
    return got == types::RefType::none() || types::is(exp->getBaseType(0), got);
  else
    return types::is(got, exp);
}

static std::string expectedTypeName(types::Type *exp) {
  if (::asOpt(exp))
    return exp->getBaseType(0)->getName();
  else
    return exp->getName();
}

types::Type *types::FuncType::getCallType(const std::vector<Type *> &inTypes) {
  if (this->inTypes.size() != inTypes.size())
    throw exc::SeqException("expected " + std::to_string(this->inTypes.size()) +
                            " argument(s), but got " +
                            std::to_string(inTypes.size()));

  for (unsigned i = 0; i < inTypes.size(); i++)
    if (!compatibleArgType(inTypes[i], this->inTypes[i]))
      throw exc::SeqException("expected function input type '" +
                              expectedTypeName(this->inTypes[i]) +
                              "', but got '" + inTypes[i]->getName() + "'");

  return outType;
}

Type *types::FuncType::getLLVMType(LLVMContext &context) const {
  std::vector<llvm::Type *> types;
  for (auto *type : inTypes)
    types.push_back(type->getLLVMType(context));

  return PointerType::get(
      FunctionType::get(outType->getLLVMType(context), types, false), 0);
}

size_t types::FuncType::size(Module *module) const {
  return module->getDataLayout().getTypeAllocSize(
      getLLVMType(module->getContext()));
}

types::FuncType *types::FuncType::get(std::vector<Type *> inTypes,
                                      Type *outType) {
  return new FuncType(std::move(inTypes), outType);
}

types::FuncType *types::FuncType::clone(Generic *ref) {
  std::vector<Type *> inTypesCloned;
  for (auto *type : inTypes)
    inTypesCloned.push_back(type->clone(ref));
  return get(inTypesCloned, outType->clone(ref));
}

types::GenType::GenType(Type *outType, GenTypeKind kind)
    : Type("generator", BaseType::get()), outType(outType), kind(kind),
      alnParams() {}

bool types::GenType::isAtomic() const { return false; }

Value *types::GenType::defaultValue(BasicBlock *block) {
  return ConstantPointerNull::get(
      cast<PointerType>(getLLVMType(block->getContext())));
}

Value *types::GenType::done(Value *self, BasicBlock *block) {
  Function *doneFn =
      Intrinsic::getDeclaration(block->getModule(), Intrinsic::coro_done);
  IRBuilder<> builder(block);
  return builder.CreateCall(doneFn, self);
}

void types::GenType::resume(Value *self, BasicBlock *block, BasicBlock *normal,
                            BasicBlock *unwind) {
  Function *resFn =
      Intrinsic::getDeclaration(block->getModule(), Intrinsic::coro_resume);
  IRBuilder<> builder(block);
  if (normal || unwind)
    builder.CreateInvoke(resFn, normal, unwind, self);
  else
    builder.CreateCall(resFn, self);
}

Value *types::GenType::promise(Value *self, BasicBlock *block, bool returnPtr) {
  if (outType->is(types::Void))
    return nullptr;

  LLVMContext &context = block->getContext();
  IRBuilder<> builder(block);

  Function *promFn =
      Intrinsic::getDeclaration(block->getModule(), Intrinsic::coro_promise);

  Value *aln =
      ConstantInt::get(IntegerType::getInt32Ty(context),
                       block->getModule()->getDataLayout().getPrefTypeAlignment(
                           outType->getLLVMType(context)));
  Value *from = ConstantInt::get(IntegerType::getInt1Ty(context), 0);

  Value *ptr = builder.CreateCall(promFn, {self, aln, from});
  ptr = builder.CreateBitCast(
      ptr, PointerType::get(outType->getLLVMType(context), 0));
  return returnPtr ? ptr : builder.CreateLoad(ptr);
}

void types::GenType::send(Value *self, Value *val, BasicBlock *block) {
  Value *promisePtr = promise(self, block, /*returnPtr=*/true);
  if (!promisePtr)
    throw exc::SeqException("cannot send value to void generator");
  IRBuilder<> builder(block);
  builder.CreateStore(val, promisePtr);
}

void types::GenType::destroy(Value *self, BasicBlock *block) {
  Function *destFn =
      Intrinsic::getDeclaration(block->getModule(), Intrinsic::coro_destroy);
  IRBuilder<> builder(block);
  builder.CreateCall(destFn, self);
}

bool types::GenType::fromPrefetch() { return kind == GenTypeKind::PREFETCH; }

bool types::GenType::fromInterAlign() {
  return kind == GenTypeKind::INTERALIGN;
}

void types::GenType::setAlignParams(GenType::InterAlignParams alnParams) {
  if (!fromInterAlign())
    throw exc::SeqException(
        "inter-sequence alignment functions must be marked '@interalign'");
  this->alnParams = alnParams;
}

types::GenType::InterAlignParams types::GenType::getAlignParams() {
  if (!fromInterAlign())
    throw exc::SeqException(
        "inter-sequence alignment functions must be marked '@interalign'");
  return alnParams;
}

void types::GenType::initOps() {
  if (!vtable.magic.empty())
    return;

  vtable.magic = {
      {"__iter__",
       {},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},

      {"__raw__",
       {},
       PtrType::get(Byte),
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},

      {"__done__",
       {},
       Bool,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(done(self, b.GetInsertBlock()),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__promise__",
       {},
       PtrType::get(getBaseType(0)),
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return promise(self, b.GetInsertBlock(), /*returnPtr=*/true);
       },
       false},

      {"__resume__",
       {},
       Void,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         resume(self, b.GetInsertBlock(), nullptr, nullptr);
         return (Value *)nullptr;
       },
       false},

      {"__str__",
       {},
       Str,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return codegenStr(self, "generator", b.GetInsertBlock());
       },
       false},
  };

  addMethod(
      "next",
      new BaseFuncLite(
          {this}, outType,
          [this](Module *module) {
            const std::string name = "seq." + getName() + ".next";
            Function *func = module->getFunction(name);

            if (!func) {
              LLVMContext &context = module->getContext();
              func = cast<Function>(module->getOrInsertFunction(
                  name, outType->getLLVMType(context), getLLVMType(context)));
              func->setLinkage(GlobalValue::PrivateLinkage);
              func->setDoesNotThrow();
              func->setPersonalityFn(makePersonalityFunc(module));
              func->addFnAttr(Attribute::AlwaysInline);

              Value *arg = func->arg_begin();
              BasicBlock *entry = BasicBlock::Create(context, "entry", func);
              Value *val = promise(arg, entry);
              IRBuilder<> builder(entry);
              builder.CreateRet(val);
            }

            return func;
          }),
      true);

  addMethod(
      "done",
      new BaseFuncLite(
          {this}, Bool,
          [this](Module *module) {
            const std::string name = "seq." + getName() + ".done";
            Function *func = module->getFunction(name);

            if (!func) {
              LLVMContext &context = module->getContext();
              func = cast<Function>(module->getOrInsertFunction(
                  name, Bool->getLLVMType(context), getLLVMType(context)));
              func->setLinkage(GlobalValue::PrivateLinkage);
              func->addFnAttr(Attribute::AlwaysInline);

              Value *arg = func->arg_begin();
              BasicBlock *entry = BasicBlock::Create(context, "entry", func);
              resume(arg, entry, nullptr, nullptr);
              Value *val = done(arg, entry);
              IRBuilder<> builder(entry);
              val = builder.CreateZExt(val, Bool->getLLVMType(context));
              builder.CreateRet(val);
            }

            return func;
          }),
      true);

  addMethod("send",
            new BaseFuncLite(
                {this, outType}, outType,
                [this](Module *module) {
                  const std::string name = "seq." + getName() + ".send";
                  Function *func = module->getFunction(name);

                  if (!func) {
                    LLVMContext &context = module->getContext();
                    func = cast<Function>(module->getOrInsertFunction(
                        name, outType->getLLVMType(context),
                        getLLVMType(context), outType->getLLVMType(context)));
                    func->setLinkage(GlobalValue::PrivateLinkage);
                    func->setDoesNotThrow();
                    func->addFnAttr(Attribute::AlwaysInline);

                    auto iter = func->arg_begin();
                    Value *self = iter++;
                    Value *val = iter;
                    BasicBlock *entry =
                        BasicBlock::Create(context, "entry", func);
                    send(self, val, entry);
                    resume(self, entry, nullptr, nullptr);
                    IRBuilder<> builder(entry);
                    builder.CreateRet(promise(self, entry));
                  }

                  return func;
                }),
            true);

  addMethod(
      "destroy",
      new BaseFuncLite(
          {this}, Void,
          [this](Module *module) {
            const std::string name = "seq." + getName() + ".destroy";
            Function *func = module->getFunction(name);

            if (!func) {
              LLVMContext &context = module->getContext();
              func = cast<Function>(module->getOrInsertFunction(
                  name, llvm::Type::getVoidTy(context), getLLVMType(context)));
              func->setLinkage(GlobalValue::PrivateLinkage);
              func->setDoesNotThrow();
              func->addFnAttr(Attribute::AlwaysInline);

              Value *arg = func->arg_begin();
              BasicBlock *entry = BasicBlock::Create(context, "entry", func);
              destroy(arg, entry);
              IRBuilder<> builder(entry);
              builder.CreateRetVoid();
            }

            return func;
          }),
      true);
}

bool types::GenType::is(Type *type) const {
  auto *genType = dynamic_cast<GenType *>(type);
  return genType && types::is(outType, genType->outType);
}

unsigned types::GenType::numBaseTypes() const { return 1; }

types::Type *types::GenType::getBaseType(unsigned idx) const { return outType; }

Type *types::GenType::getLLVMType(LLVMContext &context) const {
  return IntegerType::getInt8PtrTy(context);
}

size_t types::GenType::size(Module *module) const {
  return module->getDataLayout().getTypeAllocSize(
      getLLVMType(module->getContext()));
}

types::GenType *types::GenType::asGen() { return this; }

types::GenType *types::GenType::get(Type *outType, GenTypeKind kind) noexcept {
  return new GenType(outType, kind);
}

types::GenType *types::GenType::get(GenTypeKind kind) noexcept {
  return get(types::BaseType::get(), kind);
}

types::GenType *types::GenType::clone(Generic *ref) {
  return get(outType->clone(ref), kind);
}

types::PartialFuncType::PartialFuncType(types::Type *callee,
                                        std::vector<types::Type *> callTypes)
    : Type("partial", BaseType::get()), callee(callee),
      callTypes(std::move(callTypes)) {
  std::vector<types::Type *> types;
  types.push_back(this->callee);
  for (auto *type : this->callTypes) {
    if (type)
      types.push_back(type);
  }
  contents = types::RecordType::get(types);
}

std::vector<types::Type *> types::PartialFuncType::getCallTypes() const {
  return callTypes;
}

bool types::PartialFuncType::isAtomic() const { return contents->isAtomic(); }

Value *types::PartialFuncType::call(BaseFunc *base, Value *self,
                                    const std::vector<Value *> &args,
                                    BasicBlock *block, BasicBlock *normal,
                                    BasicBlock *unwind) {
  IRBuilder<> builder(block);
  std::vector<Value *> argsFull;
  Value *func = contents->memb(self, "1", block);

  unsigned next1 = 2, next2 = 0;
  for (auto *type : callTypes) {
    if (type) {
      argsFull.push_back(contents->memb(self, std::to_string(next1++), block));
    } else {
      assert(next2 < args.size());
      argsFull.push_back(args[next2++]);
    }
  }

  return callee->call(base, func, argsFull, block, normal, unwind);
}

Value *types::PartialFuncType::defaultValue(BasicBlock *block) {
  return contents->defaultValue(block);
}

template <typename T>
static bool nullMatch(std::vector<T *> v1, std::vector<T *> v2) {
  if (v1.size() != v2.size())
    return false;

  for (unsigned i = 0; i < v1.size(); i++) {
    if ((v1[i] == nullptr) ^ (v2[i] == nullptr))
      return false;
  }

  return true;
}

bool types::PartialFuncType::is(types::Type *type) const {
  auto *p = dynamic_cast<types::PartialFuncType *>(type);
  return p && nullMatch(callTypes, p->callTypes) &&
         types::is(contents, p->contents);
}

unsigned types::PartialFuncType::numBaseTypes() const {
  return contents->numBaseTypes();
}

types::Type *types::PartialFuncType::getBaseType(unsigned idx) const {
  return contents->getBaseType(idx);
}

types::Type *
types::PartialFuncType::getCallType(const std::vector<types::Type *> &inTypes) {
  std::vector<types::Type *> types(callTypes);
  unsigned next = 0;
  for (auto *&type : types) {
    if (!type) {
      if (next >= inTypes.size())
        throw exc::SeqException(
            "too few arguments passed to partial function call");
      type = inTypes[next++];
    }
  }

  if (next < inTypes.size())
    throw exc::SeqException(
        "too many arguments passed to partial function call");

  return callee->getCallType(types);
}

Type *types::PartialFuncType::getLLVMType(LLVMContext &context) const {
  return contents->getLLVMType(context);
}

size_t types::PartialFuncType::size(Module *module) const {
  return contents->size(module);
}

types::PartialFuncType *
types::PartialFuncType::get(types::Type *callee,
                            std::vector<types::Type *> callTypes) {
  return new types::PartialFuncType(callee, std::move(callTypes));
}

Value *types::PartialFuncType::make(Value *func, std::vector<Value *> args,
                                    BasicBlock *block) {
  Value *self = contents->defaultValue(block);
  IRBuilder<> builder(block);
  self = builder.CreateInsertValue(self, func, 0);
  for (unsigned i = 0; i < args.size(); i++)
    self = builder.CreateInsertValue(self, args[i], i + 1);
  return self;
}

types::PartialFuncType *types::PartialFuncType::clone(Generic *ref) {
  std::vector<types::Type *> callTypesCloned;
  for (auto *type : callTypes)
    callTypesCloned.push_back(type ? type->clone(ref) : nullptr);
  return get(callee->clone(ref), callTypesCloned);
}
