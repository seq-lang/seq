#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::PtrType::PtrType(Type *baseType) :
    Type(baseType->getName() + "*", BaseType::get()), baseType(baseType)
{
}

Value *types::PtrType::indexLoad(BaseFunc *base,
                                 Value *self,
                                 Value *idx,
                                 BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *ptr = builder.CreateGEP(self, idx);
	return builder.CreateLoad(ptr);
}

void types::PtrType::indexStore(BaseFunc *base,
                                Value *self,
                                Value *idx,
                                Value *val,
                                BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *ptr = builder.CreateGEP(self, idx);
	builder.CreateStore(val, ptr);
}

types::Type *types::PtrType::indexType() const
{
	return baseType;
}

types::Type *types::PtrType::subscriptType() const
{
	return types::Int;
}

Value *types::PtrType::defaultValue(BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	return ConstantPointerNull::get(PointerType::get(indexType()->getLLVMType(context), 0));
}

Value *types::PtrType::construct(BaseFunc *base,
                                 const std::vector<Value *>& args,
                                 BasicBlock *block)
{
	ValueExpr count(types::Int, args[0]);
	ArrayExpr e(getBaseType(0), &count);
	Value *arr = e.codegen(base, block);
	return e.getType()->memb(arr, "ptr", block);
}

void types::PtrType::initOps()
{
	if (!vtable.ops.empty())
		return;

	vtable.ops = {
		{uop("!"), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateIsNull(lhs), Bool->getLLVMType(b.getContext()));
		}},

		{uop("+"), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateIsNotNull(lhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("+"), Int, this, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateGEP(lhs, rhs);
		}},

		{bop("-"), this, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreatePtrDiff(lhs, rhs);
		}},

		{bop("<"), this, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpSLT(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop(">"), this, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpSGT(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("<="), this, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpSLE(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop(">="), this, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpSGE(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("=="), this, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpEQ(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("!="), this, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpNE(lhs, rhs), Bool->getLLVMType(b.getContext()));
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

types::Type *types::PtrType::getConstructType(const std::vector<types::Type *>& inTypes)
{
	if (inTypes.size() != 1 || !types::is(inTypes[0], types::Int))
		throw exc::SeqException("pointer constructor takes exactly 1 integer argument");

	return this;
}

Type *types::PtrType::getLLVMType(LLVMContext& context) const
{
	return PointerType::get(indexType()->getLLVMType(context), 0);
}

seq_int_t types::PtrType::size(Module *module) const
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
	return get(indexType()->clone(ref));
}
