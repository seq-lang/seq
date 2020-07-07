#include "lang/seq.h"
#include <cassert>

using namespace seq;
using namespace llvm;

types::RefType::RefType(std::string name)
    : Type(std::move(name), BaseType::get(), false, true), contents(nullptr) {}

void types::RefType::setContents(types::RecordType *contents) {
  this->contents = contents;
}

std::string types::RefType::getName() const { return name; }

std::string types::RefType::genericName() { return getName(); }

static void codegenNotNoneCheck(Value *self, const std::string &name,
                                BasicBlock *block) {
  if (config::config().debug) {
    LLVMContext &context = block->getContext();
    Module *module = block->getModule();
    IRBuilder<> builder(block);
    self = builder.CreateBitCast(self, builder.getInt8PtrTy());

    GlobalVariable *strVar = new GlobalVariable(
        *module,
        llvm::ArrayType::get(IntegerType::getInt8Ty(context),
                             name.length() + 1),
        true, GlobalValue::PrivateLinkage,
        ConstantDataArray::getString(context, name), "str_memb");
    strVar->setAlignment(1);

    Value *str =
        builder.CreateBitCast(strVar, IntegerType::getInt8PtrTy(context));
    Value *len = ConstantInt::get(seqIntLLVM(context), name.length());
    Value *membVal = types::Str->make(str, len, builder.GetInsertBlock());
    Function *ensureNotNone =
        Func::getBuiltin("_ensure_not_none")->getFunc(module);
    builder.CreateCall(ensureNotNone, {self, membVal});
  }
}

Value *types::RefType::memb(Value *self, const std::string &name,
                            BasicBlock *block) {
  initFields();
  initOps();
  if (contents->numBaseTypes() > 0)
    codegenNotNoneCheck(self, name, block);

  // Defer to contained tuple, unless this is a method reference.
  if (!contents || contents->hasMethod(name) || Type::hasMethod(name))
    return Type::memb(self, name, block);

  LLVMContext &context = block->getContext();
  IRBuilder<> builder(block);
  self = builder.CreateBitCast(self, getStructPointerType(context));
  Value *x = builder.CreateLoad(self);

  try {
    return contents->memb(x, name, block);
  } catch (exc::SeqException &) {
    throw exc::SeqException("type '" + getName() + "' has no member '" + name +
                            "'");
  }
}

types::Type *types::RefType::membType(const std::string &name) {
  initFields();
  initOps();

  try {
    return Type::membType(name);
  } catch (exc::SeqException &) {
  }

  // Defer to contained tuple, unless this is a method reference.
  if (contents && !contents->hasMethod(name)) {
    try {
      return contents->membType(name);
    } catch (exc::SeqException &) {
    }
  }

  throw exc::SeqException("type '" + getName() + "' has no member '" + name +
                          "'");
}

Value *types::RefType::setMemb(Value *self, const std::string &name, Value *val,
                               BasicBlock *block) {
  initFields();
  if (contents->numBaseTypes() > 0)
    codegenNotNoneCheck(self, name, block);
  LLVMContext &context = block->getContext();
  IRBuilder<> builder(block);
  self = builder.CreateBitCast(self, getStructPointerType(context));
  Value *x = builder.CreateLoad(self);
  x = contents->setMemb(x, name, val, block);
  builder.CreateStore(x, self);
  return self;
}

Value *types::RefType::defaultValue(BasicBlock *block) {
  return ConstantPointerNull::get(
      cast<PointerType>(getLLVMType(block->getContext())));
}

void types::RefType::initOps() {
  if (!vtable.magic.empty())
    return;

  vtable.magic = {
      {"__new__",
       {},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         assert(contents);
         self = contents->alloc(nullptr, b.GetInsertBlock());
         self = b.CreateBitCast(self, getLLVMType(b.getContext()));
         return self;
       },
       true},

      {"__bool__",
       {},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateIsNotNull(self),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__pickle__",
       {PtrType::get(Byte)},
       Void,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         assert(contents);
         Value *tuple = b.CreateBitCast(
             self, contents->getLLVMType(b.getContext())->getPointerTo());
         tuple = b.CreateLoad(tuple);
         BasicBlock *block = b.GetInsertBlock();
         contents->callMagic("__pickle__", {PtrType::get(Byte)}, tuple,
                             {args[0]}, block, nullptr);
         return (Value *)nullptr;
       },
       false},

      {"__unpickle__",
       {PtrType::get(Byte)},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         assert(contents);
         BasicBlock *block = b.GetInsertBlock();
         Value *tuple =
             contents->callMagic("__unpickle__", {PtrType::get(Byte)}, nullptr,
                                 {args[0]}, block, nullptr);
         self = contents->alloc(nullptr, block);
         b.CreateStore(tuple, self);
         self = b.CreateBitCast(self, getLLVMType(b.getContext()));
         return self;
       },
       true},

      {"__raw__",
       {},
       PtrType::get(Byte),
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},
  };

  if (contents) {
    vtable.magic.push_back(
        {"__init__", contents->getTypes(), this,
         [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
           self = b.CreateBitCast(self, getStructPointerType(b.getContext()));
           for (unsigned i = 0; i < args.size(); i++)
             self = setMemb(self, std::to_string(i + 1), args[i],
                            b.GetInsertBlock());
           self = b.CreateBitCast(self, getLLVMType(b.getContext()));
           return self;
         }});
  }
}

void types::RefType::initFields() {
  if (contents)
    contents->initFields();
}

bool types::RefType::isAtomic() const { return false; }

bool types::RefType::is(types::Type *type) const {
  types::RefType *ref = type->asRef();
  return ref && (ref == none() || this == none() || name == ref->name);
}

unsigned types::RefType::numBaseTypes() const {
  return contents ? contents->numBaseTypes() : 0;
}

types::Type *types::RefType::getBaseType(unsigned idx) const {
  return contents ? contents->getBaseType(idx) : nullptr;
}

Type *types::RefType::getStructPointerType(LLVMContext &context) const {
  assert(contents);
  StructType *structType = StructType::create(context, name);
  contents->addLLVMTypesToStruct(structType);
  return PointerType::get(structType, 0);
}

Type *types::RefType::getLLVMType(LLVMContext &context) const {
  return IntegerType::getInt8PtrTy(context);
}

size_t types::RefType::size(Module *module) const { return sizeof(void *); }

types::RefType *types::RefType::asRef() { return this; }

Value *types::RefType::make(BasicBlock *block, std::vector<Value *> vals) {
  assert(contents);
  LLVMContext &context = block->getContext();
  Value *val = contents->defaultValue(block);
  Value *ref = contents->alloc(nullptr, block);
  IRBuilder<> builder(block);
  llvm::Type *type = getStructPointerType(context);
  ref = builder.CreateBitCast(ref, type);
  val = builder.CreateBitCast(val, cast<PointerType>(type)->getElementType());
  builder.CreateStore(val, ref);

  for (unsigned i = 0; i < vals.size(); i++)
    ref = setMemb(ref, std::to_string(i + 1), vals[i], block);

  ref = builder.CreateBitCast(ref, getLLVMType(context));
  return ref;
}

types::RefType *types::RefType::get(std::string name) {
  return new RefType(std::move(name));
}

types::RefType *types::RefType::none() {
  static RefType *noneRef = RefType::get("<None>");
  return noneRef;
}

types::MethodType::MethodType(types::Type *self, FuncType *func)
    : RecordType({self, func}, {"self", "func"}), self(self), func(func) {}

Value *types::MethodType::call(BaseFunc *base, Value *self,
                               const std::vector<Value *> &args,
                               BasicBlock *block, BasicBlock *normal,
                               BasicBlock *unwind) {
  Value *x = memb(self, "self", block);
  Value *f = memb(self, "func", block);
  std::vector<Value *> argsFull(args);
  argsFull.insert(argsFull.begin(), x);
  return func->call(base, f, argsFull, block, normal, unwind);
}

bool types::MethodType::is(types::Type *type) const {
  return isGeneric(type) && types::is(getBaseType(0), type->getBaseType(0)) &&
         types::is(getBaseType(1), type->getBaseType(1));
}

unsigned types::MethodType::numBaseTypes() const { return 2; }

types::Type *types::MethodType::getBaseType(unsigned idx) const {
  return idx ? self : func;
}

types::Type *
types::MethodType::getCallType(const std::vector<Type *> &inTypes) {
  std::vector<Type *> inTypesFull(inTypes);
  inTypesFull.insert(inTypesFull.begin(), self);
  return func->getCallType(inTypesFull);
}

Value *types::MethodType::make(Value *self, Value *func, BasicBlock *block) {
  LLVMContext &context = self->getContext();
  Value *method = UndefValue::get(getLLVMType(context));
  method = setMemb(method, "self", self, block);
  method = setMemb(method, "func", func, block);
  return method;
}

types::MethodType *types::MethodType::get(types::Type *self,
                                          types::FuncType *func) {
  return new MethodType(self, func);
}
