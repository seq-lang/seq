#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::PtrType::PtrType(Type *baseType) :
    Type("ptr[" + baseType->getName() + "]", BaseType::get()), baseType(baseType)
{
}

Value *types::PtrType::defaultValue(BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	return ConstantPointerNull::get(PointerType::get(getBaseType(0)->getLLVMType(context), 0));
}

void types::PtrType::initOps()
{
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__init__", {Int}, this, SEQ_MAGIC_CAPT(self, args, b) {
			return getBaseType(0)->alloc(args[0], b.GetInsertBlock());
		}},

		{"__copy__", {}, this, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		{"__bool__", {}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateIsNotNull(self), Bool->getLLVMType(b.getContext()));
		}},

		{"__getitem__", {Int}, getBaseType(0), SEQ_MAGIC(self, args, b) {
			Value *ptr = b.CreateGEP(self, args[0]);
			return b.CreateLoad(ptr);
		}},

		{"__setitem__", {Int, getBaseType(0)}, Void, SEQ_MAGIC(self, args, b) {
			Value *ptr = b.CreateGEP(self, args[0]);
			b.CreateStore(args[1], ptr);
			return (Value *)nullptr;
		}},

		{"__add__", {Int}, this, SEQ_MAGIC(self, args, b) {
			return b.CreateGEP(self, args[0]);
		}},

		{"__sub__", {this}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreatePtrDiff(self, args[0]);
		}},

		{"__eq__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpEQ(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ne__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpNE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__lt__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpSLT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__gt__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpSGT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__le__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpSLE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ge__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpSGE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},
	};
}

bool types::PtrType::isAtomic() const
{
	return false;
}

bool types::PtrType::is(types::Type *type) const
{
	return isGeneric(type) && types::is(getBaseType(0), type->getBaseType(0));
}

unsigned types::PtrType::numBaseTypes() const
{
	return 1;
}

types::Type *types::PtrType::getBaseType(unsigned idx) const
{
	return baseType;
}

Type *types::PtrType::getLLVMType(LLVMContext& context) const
{
	return PointerType::get(getBaseType(0)->getLLVMType(context), 0);
}

size_t types::PtrType::size(Module *module) const
{
	return module->getDataLayout().getTypeAllocSize(getLLVMType(module->getContext()));
}

types::PtrType *types::PtrType::get(Type *baseType) noexcept
{
	return new PtrType(baseType);
}

types::PtrType *types::PtrType::get() noexcept
{
	return new PtrType(types::BaseType::get());
}

types::PtrType *types::PtrType::clone(Generic *ref)
{
	return get(getBaseType(0)->clone(ref));
}
