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
  std::sort(types.begin(), types.end(),
            [](types::Type *a, types::Type *b) { return a->getName() > b->getName(); });
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
  throw exc::SeqException("union '" + getName() + "' has no type '" + type->getName() +
                          "'");
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
  int index = 0;
  for (types::Type *type : types) {
    vtable.magic.push_back(
        {"__init__",
         {type},
         this,
         [this, type](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
           return make(type, args[0], b.GetInsertBlock());
         },
         false});

    vtable.magic.push_back(
        {"__is_" + std::to_string(index) + "__",
         {},
         Bool,
         [this, type](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
           return b.CreateZExt(has(self, type, b.GetInsertBlock()),
                               Bool->getLLVMType(b.getContext()));
         },
         false});

    vtable.magic.push_back(
        {"__get_" + std::to_string(index) + "__",
         {},
         type,
         [this, type](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
           return val(self, type, b.GetInsertBlock());
         },
         false});

    index += 1;
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

types::Type *types::UnionType::getBaseType(unsigned idx) const { return types[idx]; }

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
  return module->getDataLayout().getTypeAllocSize(getLLVMType(module->getContext()));
}

types::UnionType *types::UnionType::asUnion() { return this; }

Value *types::UnionType::make(types::Type *type, Value *val, BasicBlock *block) {
  LLVMContext &context = block->getContext();
  Value *self = defaultValue(block);
  const unsigned which = indexFor(type);
  self = setMemb(self, "which", ConstantInt::get(selectorType(context), which), block);
  Value *data = interpretAs(val, getLLVMType(context)->getElementType(1), block);
  self = setMemb(self, "data", data, block);
  return self;
}

Value *types::UnionType::has(Value *self, types::Type *type, BasicBlock *block) {
  LLVMContext &context = block->getContext();
  Value *whichGot = memb(self, "which", block);
  Value *whichExp = ConstantInt::get(selectorType(context), indexFor(type));
  IRBuilder<> builder(block);
  return builder.CreateICmpEQ(whichGot, whichExp);
}

Value *types::UnionType::val(Value *self, types::Type *type, BasicBlock *block) {
  Value *data = memb(self, "data", block);
  return interpretAs(data, type->getLLVMType(block->getContext()), block);
}

types::UnionType *types::UnionType::get(std::vector<types::Type *> types) noexcept {
  return new UnionType(types);
}
