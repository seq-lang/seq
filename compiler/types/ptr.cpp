#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::PtrType::PtrType(Type *baseType) :
    Type("ptr", BaseType::get()), baseType(baseType)
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
		{"__init__", {}, this, SEQ_MAGIC_CAPT(self, args, b) {
			return ConstantPointerNull::get(getBaseType(0)->getLLVMType(b.getContext())->getPointerTo(0));
		}},

		{"__init__", {Int}, this, SEQ_MAGIC_CAPT(self, args, b) {
			return getBaseType(0)->alloc(args[0], b.GetInsertBlock());
		}},

		{"__init__", {getBaseType(0)}, this, SEQ_MAGIC_CAPT(self, args, b) {
			Value *p = getBaseType(0)->alloc(nullptr, b.GetInsertBlock());
			b.CreateStore(args[0], p);
			return p;
		}},

		{"__init__", {PtrType::get(Base)}, this, SEQ_MAGIC_CAPT(self, args, b) {
			return b.CreateBitCast(args[0], getLLVMType(b.getContext()));
		}},

		{"__init__", {PtrType::get(Byte)}, this, SEQ_MAGIC_CAPT(self, args, b) {
			return b.CreateBitCast(args[0], getLLVMType(b.getContext()));
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

		/*
		 * Prefetch magics are labeled [rw][0123] representing read/write and locality.
		 * Instruction cache prefetch is not supported.
		 * https://llvm.org/docs/LangRef.html#llvm-prefetch-intrinsic
		 */

		{"__prefetch_r0__", {}, Void, SEQ_MAGIC(self, args, b) {
			self = b.CreateBitCast(self, b.getInt8PtrTy());
			Function *prefetch = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
			b.CreateCall(prefetch, {self, b.getInt32(0), b.getInt32(0), b.getInt32(1)});
			return (Value *)nullptr;
		}},

		{"__prefetch_r1__", {}, Void, SEQ_MAGIC(self, args, b) {
			self = b.CreateBitCast(self, b.getInt8PtrTy());
			Function *prefetch = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
			b.CreateCall(prefetch, {self, b.getInt32(0), b.getInt32(1), b.getInt32(1)});
			return (Value *)nullptr;
		}},

		{"__prefetch_r2__", {}, Void, SEQ_MAGIC(self, args, b) {
			self = b.CreateBitCast(self, b.getInt8PtrTy());
			Function *prefetch = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
			b.CreateCall(prefetch, {self, b.getInt32(0), b.getInt32(2), b.getInt32(1)});
			return (Value *)nullptr;
		}},

		{"__prefetch_r3__", {}, Void, SEQ_MAGIC(self, args, b) {
			self = b.CreateBitCast(self, b.getInt8PtrTy());
			Function *prefetch = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
			b.CreateCall(prefetch, {self, b.getInt32(0), b.getInt32(3), b.getInt32(1)});
			return (Value *)nullptr;
		}},

		{"__prefetch_w0__", {}, Void, SEQ_MAGIC(self, args, b) {
			self = b.CreateBitCast(self, b.getInt8PtrTy());
			Function *prefetch = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
			b.CreateCall(prefetch, {self, b.getInt32(1), b.getInt32(0), b.getInt32(1)});
			return (Value *)nullptr;
		}},

		{"__prefetch_w1__", {}, Void, SEQ_MAGIC(self, args, b) {
			self = b.CreateBitCast(self, b.getInt8PtrTy());
			Function *prefetch = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
			b.CreateCall(prefetch, {self, b.getInt32(1), b.getInt32(1), b.getInt32(1)});
			return (Value *)nullptr;
		}},

		{"__prefetch_w2__", {}, Void, SEQ_MAGIC(self, args, b) {
			self = b.CreateBitCast(self, b.getInt8PtrTy());
			Function *prefetch = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
			b.CreateCall(prefetch, {self, b.getInt32(1), b.getInt32(2), b.getInt32(1)});
			return (Value *)nullptr;
		}},

		{"__prefetch_w3__", {}, Void, SEQ_MAGIC(self, args, b) {
			self = b.CreateBitCast(self, b.getInt8PtrTy());
			Function *prefetch = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
			b.CreateCall(prefetch, {self, b.getInt32(1), b.getInt32(3), b.getInt32(1)});
			return (Value *)nullptr;
		}},
	};

	addMethod("raw", new BaseFuncLite({this}, PtrType::get(Byte), [this](Module *module) {
		const std::string name = "seq." + getName() + ".raw";
		Function *func = module->getFunction(name);

		if (!func) {
			LLVMContext& context = module->getContext();
			func = cast<Function>(module->getOrInsertFunction(name,
			                                                  IntegerType::getInt8PtrTy(context),
			                                                  getLLVMType(context)));
			func->setDoesNotThrow();
			func->setLinkage(GlobalValue::PrivateLinkage);
			func->addFnAttr(Attribute::AlwaysInline);
			Value *self = func->arg_begin();
			BasicBlock *block = BasicBlock::Create(context, "entry", func);
			IRBuilder<> builder(block);
			Value *raw = builder.CreateBitCast(self, builder.getInt8PtrTy());
			builder.CreateRet(raw);
		}

		return func;
	}), true);

}

bool types::PtrType::isAtomic() const
{
	return false;
}

bool types::PtrType::is(types::Type *type) const
{
	return isGeneric(type) && (getBaseType(0)->is(types::Base) ||
	                           type->getBaseType(0)->is(types::Base) ||
	                           types::is(getBaseType(0), type->getBaseType(0)));
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
