#include "lang/seq.h"

using namespace seq;
using namespace llvm;

types::OptionalType::OptionalType(seq::types::Type *baseType)
    : Type("Optional", BaseType::get()), baseType(baseType) {}

/*
 * Reference types are special-cased since their empty value can just be the
 * null pointer.
 */
bool types::OptionalType::isRefOpt() const { return (baseType->asRef() != nullptr); }

Value *types::OptionalType::defaultValue(BasicBlock *block) {
  return make(nullptr, block);
}

void types::OptionalType::initFields() {
  if (isRefOpt() || !vtable.fields.empty())
    return;

  vtable.fields = {{"has", {0, Void}}, {"val", {1, Void}}};
}

void types::OptionalType::initOps() {
  if (!vtable.magic.empty())
    return;

  vtable.magic = {
      {"__new__",
       {getBaseType(0)},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return make(args[0], b.GetInsertBlock());
       },
       true},

      {"__new__",
       {},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return make(nullptr, b.GetInsertBlock());
       },
       true},

      {"__bool__",
       {},
       Bool,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(has(self, b.GetInsertBlock()),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__invert__",
       {},
       getBaseType(0),
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return val(self, b.GetInsertBlock());
       },
       false},
  };
}

bool types::OptionalType::isAtomic() const { return baseType->isAtomic(); }

bool types::OptionalType::is(types::Type *type) const {
  return isGeneric(type) && types::is(getBaseType(0), type->getBaseType(0));
}

unsigned types::OptionalType::numBaseTypes() const { return 1; }

types::Type *types::OptionalType::getBaseType(unsigned idx) const { return baseType; }

Type *types::OptionalType::getLLVMType(LLVMContext &context) const {
  if (isRefOpt())
    return baseType->getLLVMType(context);
  else
    return StructType::get(IntegerType::getInt1Ty(context),
                           baseType->getLLVMType(context));
}

size_t types::OptionalType::size(Module *module) const {
  return module->getDataLayout().getTypeAllocSize(getLLVMType(module->getContext()));
}

types::OptionalType *types::OptionalType::asOpt() { return this; }

Value *types::OptionalType::make(Value *val, BasicBlock *block) {
  LLVMContext &context = block->getContext();

  if (isRefOpt())
    return val ? val
               : ConstantPointerNull::get(cast<PointerType>(getLLVMType(context)));
  else {
    IRBuilder<> builder(block);
    Value *self = UndefValue::get(getLLVMType(context));
    self =
        setMemb(self, "has",
                ConstantInt::get(IntegerType::getInt1Ty(context), val ? 1 : 0), block);
    if (val)
      self = setMemb(self, "val", val, block);
    return self;
  }
}

Value *types::OptionalType::has(Value *self, BasicBlock *block) {
  if (isRefOpt()) {
    LLVMContext &context = block->getContext();
    IRBuilder<> builder(block);
    return builder.CreateICmpNE(
        self, ConstantPointerNull::get(cast<PointerType>(getLLVMType(context))));
  } else {
    return memb(self, "has", block);
  }
}

Value *types::OptionalType::val(Value *self, BasicBlock *block) {
  return isRefOpt() ? self : memb(self, "val", block);
}

types::OptionalType *types::OptionalType::get(types::Type *baseType) noexcept {
  return new OptionalType(baseType);
}

types::OptionalType *types::OptionalType::get() { return get(types::BaseType::get()); }
