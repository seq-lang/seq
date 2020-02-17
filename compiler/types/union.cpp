#include "lang/seq.h"
#include <algorithm>
#include <functional>

using namespace seq;
using namespace llvm;

types::UnionType::UnionType(std::vector<types::Type *> types)
    : Type("union", BaseType::get()), types(std::move(types)) {
  assert(!this->types.empty());
}

static void sortTypes(std::vector<types::Type *> &types) {
  std::sort(types.begin(), types.end(), [](types::Type *a, types::Type *b) {
    return a->getName() > b->getName();
  });
}

std::vector<types::Type *> types::UnionType::sortedTypes() const {
  std::vector<types::Type *> ourTypes(types);
  sortTypes(ourTypes);
  return ourTypes;
}

IntegerType *types::UnionType::selectorType(LLVMContext &context) const {
  unsigned bits = 1;
  while ((1 << bits) < types.size())
    ++bits;
  return IntegerType::get(context, bits);
}

unsigned types::UnionType::indexFor(types::Type *type) const {
  std::vector<types::Type *> ourTypes = sortedTypes();
  for (unsigned i = 0; i < ourTypes.size(); i++) {
    if (types::is(ourTypes[i], type))
      return i;
  }
  throw exc::SeqException("union '" + getName() + "' has no type '" +
                          type->getName() + "'");
}

Value *types::UnionType::interpretAs(Value *data, llvm::Type *type,
                                     BasicBlock *block) const {
  LLVMContext &context = block->getContext();
  Module *module = block->getModule();

  const std::string globalName = getName() + ".reinterpret";
  GlobalVariable *gv = block->getModule()->getGlobalVariable(globalName);
  if (!gv) {
    llvm::Type *gvType = getLLVMType(context)->getElementType(1);
    gv = new GlobalVariable(*module, gvType, false, GlobalValue::PrivateLinkage,
                            Constant::getNullValue(gvType), globalName);
  }

  IRBuilder<> builder(block);
  Value *ptr = builder.CreateBitCast(gv, data->getType()->getPointerTo());
  builder.CreateStore(data, ptr);
  ptr = builder.CreateBitCast(gv, type->getPointerTo());
  return builder.CreateLoad(ptr);
}

Value *types::UnionType::defaultValue(BasicBlock *block) {
  return make(types[0], types[0]->defaultValue(block), block);
}

void types::UnionType::initFields() {
  if (!vtable.fields.empty())
    return;

  vtable.fields = {{"which", {0, Void}}, {"data", {1, Void}}};
}

void types::UnionType::initOps() {
  if (!vtable.magic.empty())
    return;

  // add an __init__ for each type
  for (types::Type *type : types) {
    vtable.magic.push_back(
        {"__init__",
         {type},
         this,
         [this, type](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
           return make(type, args[0], b.GetInsertBlock());
         },
         false});
  }
}

bool types::UnionType::isAtomic() const {
  for (types::Type *type : types) {
    if (!type->isAtomic())
      return false;
  }
  return true;
}

bool types::UnionType::is(types::Type *type) const {
  if (!isGeneric(type) || numBaseTypes() != type->numBaseTypes())
    return false;

  std::vector<types::Type *> ourTypes = sortedTypes();
  std::vector<types::Type *> otherTypes;
  for (unsigned i = 0; i < type->numBaseTypes(); i++) {
    otherTypes.push_back(type->getBaseType(i));
  }
  sortTypes(otherTypes);

  assert(ourTypes.size() == otherTypes.size());
  for (unsigned i = 0; i < numBaseTypes(); i++) {
    if (!types::is(ourTypes[i], otherTypes[i]))
      return false;
  }
  return true;
}

unsigned types::UnionType::numBaseTypes() const { return types.size(); }

types::Type *types::UnionType::getBaseType(unsigned idx) const {
  return types[idx];
}

StructType *types::UnionType::getLLVMType(LLVMContext &context) const {
  // TODO: We create a new DataLayout since we don't have access to module here.
  const DataLayout layout = EngineBuilder().selectTarget()->createDataLayout();
  unsigned maxSize = 0;
  for (types::Type *type : types) {
    const unsigned size = layout.getTypeAllocSize(type->getLLVMType(context));
    if (size > maxSize)
      maxSize = size;
  }
  return StructType::get(selectorType(context),
                         IntegerType::getIntNTy(context, 8 * maxSize));
}

size_t types::UnionType::size(Module *module) const {
  return module->getDataLayout().getTypeAllocSize(
      getLLVMType(module->getContext()));
}

types::UnionType *types::UnionType::asUnion() { return this; }

Value *types::UnionType::make(types::Type *type, Value *val,
                              BasicBlock *block) {
  LLVMContext &context = block->getContext();
  Value *self = defaultValue(block);
  const unsigned which = indexFor(type);
  self = setMemb(self, "which", ConstantInt::get(selectorType(context), which),
                 block);
  Value *data =
      interpretAs(val, getLLVMType(context)->getElementType(1), block);
  self = setMemb(self, "data", data, block);
  return self;
}

Value *types::UnionType::has(Value *self, types::Type *type,
                             BasicBlock *block) {
  LLVMContext &context = block->getContext();
  Value *whichGot = memb(self, "which", block);
  Value *whichExp = ConstantInt::get(selectorType(context), indexFor(type));
  IRBuilder<> builder(block);
  return builder.CreateICmpEQ(whichGot, whichExp);
}

Value *types::UnionType::val(Value *self, types::Type *type,
                             BasicBlock *block) {
  Value *data = memb(self, "data", block);
  return interpretAs(data, type->getLLVMType(block->getContext()), block);
}

types::UnionType *
types::UnionType::get(std::vector<types::Type *> types) noexcept {
  return new UnionType(types);
}

types::UnionType *types::UnionType::clone(Generic *ref) {
  std::vector<types::Type *> typesCloned;
  for (types::Type *type : types)
    typesCloned.push_back(type->clone(ref));
  return get(typesCloned);
}

types::OptionalType::OptionalType(seq::types::Type *baseType)
    : Type("optional", BaseType::get()), baseType(baseType) {}

/*
 * Reference types are special-cased since their empty value can just be the
 * null pointer.
 */
bool types::OptionalType::isRefOpt() const {
  return (baseType->asRef() != nullptr);
}

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

types::Type *types::OptionalType::getBaseType(unsigned idx) const {
  return baseType;
}

Type *types::OptionalType::getLLVMType(LLVMContext &context) const {
  if (isRefOpt())
    return baseType->getLLVMType(context);
  else
    return StructType::get(IntegerType::getInt1Ty(context),
                           baseType->getLLVMType(context));
}

size_t types::OptionalType::size(Module *module) const {
  return module->getDataLayout().getTypeAllocSize(
      getLLVMType(module->getContext()));
}

types::OptionalType *types::OptionalType::asOpt() { return this; }

Value *types::OptionalType::make(Value *val, BasicBlock *block) {
  LLVMContext &context = block->getContext();

  if (isRefOpt())
    return val ? val
               : ConstantPointerNull::get(
                     cast<PointerType>(getLLVMType(context)));
  else {
    IRBuilder<> builder(block);
    Value *self = UndefValue::get(getLLVMType(context));
    self = setMemb(
        self, "has",
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
        self,
        ConstantPointerNull::get(cast<PointerType>(getLLVMType(context))));
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

types::OptionalType *types::OptionalType::get() {
  return get(types::BaseType::get());
}

types::OptionalType *types::OptionalType::clone(Generic *ref) {
  return get(baseType->clone(ref));
}
