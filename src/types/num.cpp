#include <iostream>
#include <cstdio>
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::NumberType::NumberType() : Type("Num", BaseType::get())
{
}

types::IntType::IntType() : Type("Int", NumberType::get())
{
	SEQ_ASSIGN_VTABLE_FIELD(print, seq_print_int);
}

types::FloatType::FloatType() : Type("Float", NumberType::get())
{
	SEQ_ASSIGN_VTABLE_FIELD(print, seq_print_float);
}

types::BoolType::BoolType() : Type("Bool", NumberType::get())
{
	SEQ_ASSIGN_VTABLE_FIELD(print, seq_print_bool);
}

types::ByteType::ByteType() : Type("Byte", NumberType::get())
{
	SEQ_ASSIGN_VTABLE_FIELD(print, seq_print_byte);
}

Value *types::IntType::eq(BaseFunc *base,
                          Value *self,
                          Value *other,
                          BasicBlock *block)
{
	IRBuilder<> builder(block);
	return builder.CreateICmpEQ(self, other);
}

Value *types::FloatType::eq(BaseFunc *base,
                            Value *self,
                            Value *other,
                            BasicBlock *block)
{
	IRBuilder<> builder(block);
	return builder.CreateFCmpOEQ(self, other);
}

Value *types::BoolType::eq(BaseFunc *base,
                           Value *self,
                           Value *other,
                           BasicBlock *block)
{
	IRBuilder<> builder(block);
	return builder.CreateICmpEQ(self, other);
}

Value *types::ByteType::eq(BaseFunc *base,
                           Value *self,
                           Value *other,
                           BasicBlock *block)
{
	IRBuilder<> builder(block);
	return builder.CreateICmpEQ(self, other);
}

Value *types::IntType::defaultValue(BasicBlock *block)
{
	return ConstantInt::get(getLLVMType(block->getContext()), 0);
}

Value *types::FloatType::defaultValue(BasicBlock *block)
{
	return ConstantFP::get(getLLVMType(block->getContext()), 0.0);
}

Value *types::BoolType::defaultValue(BasicBlock *block)
{
	return ConstantInt::get(getLLVMType(block->getContext()), 0);
}

Value *types::ByteType::defaultValue(BasicBlock *block)
{
	return ConstantInt::get(getLLVMType(block->getContext()), 0);
}

Value *types::ByteType::construct(BaseFunc *base,
                                  const std::vector<Value *>& args,
                                  BasicBlock *block)
{
	assert(args.size() == 1);
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);
	return builder.CreateTrunc(args[0], getLLVMType(context));
}

void types::IntType::initOps()
{
	if (!vtable.ops.empty())
		return;

	vtable.ops = {
		// int ops
		{uop("~"), Int, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateNot(lhs);
		}},

		{uop("-"), Int, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateNeg(lhs);
		}},

		{uop("+"), Int, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return lhs;
		}},

		// int,int ops
		{bop("*"), Int, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateMul(lhs, rhs);
		}},

		{bop("/"), Int, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateSDiv(lhs, rhs);
		}},

		{bop("%"), Int, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateSRem(lhs, rhs);
		}},

		{bop("+"), Int, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateAdd(lhs, rhs);
		}},

		{bop("-"), Int, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateSub(lhs, rhs);
		}},

		{bop("<<"), Int, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateShl(lhs, rhs);
		}},

		{bop(">>"), Int, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateAShr(lhs, rhs);
		}},

		{bop("<"), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpSLT(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop(">"), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpSGT(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("<="), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpSLE(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop(">="), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpSGE(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("=="), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpEQ(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("!="), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpNE(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("&"), Int, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateAnd(lhs, rhs);
		}},

		{bop("^"), Int, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateXor(lhs, rhs);
		}},

		{bop("|"), Int, Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateOr(lhs, rhs);
		}},

		// int,float ops
		{bop("*"), Float, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFMul(b.CreateSIToFP(lhs, Float->getLLVMType(b.getContext())), rhs);
		}},

		{bop("/"), Float, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFDiv(b.CreateSIToFP(lhs, Float->getLLVMType(b.getContext())), rhs);
		}},

		{bop("%"), Float, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFRem(b.CreateSIToFP(lhs, Float->getLLVMType(b.getContext())), rhs);
		}},

		{bop("+"), Float, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFAdd(b.CreateSIToFP(lhs, Float->getLLVMType(b.getContext())), rhs);
		}},

		{bop("-"), Float, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFSub(b.CreateSIToFP(lhs, Float->getLLVMType(b.getContext())), rhs);
		}},

		{bop("<"), Float, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOLT(b.CreateSIToFP(lhs, Float->getLLVMType(b.getContext())), rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop(">"), Float, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOGT(b.CreateSIToFP(lhs, Float->getLLVMType(b.getContext())), rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("<="), Float, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOLE(b.CreateSIToFP(lhs, Float->getLLVMType(b.getContext())), rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop(">="), Float, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOGE(b.CreateSIToFP(lhs, Float->getLLVMType(b.getContext())), rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("=="), Float, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOEQ(b.CreateSIToFP(lhs, Float->getLLVMType(b.getContext())), rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("!="), Float, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpONE(b.CreateSIToFP(lhs, Float->getLLVMType(b.getContext())), rhs), Bool->getLLVMType(b.getContext()));
		}},
	};
}

void types::FloatType::initOps()
{
	if (!vtable.ops.empty())
		return;

	vtable.ops = {
		// float ops
		{uop("-"), Float, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFNeg(lhs);
		}},

		{uop("+"), Float, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return lhs;
		}},

		// float,float ops
		{bop("*"), Float, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFMul(lhs, rhs);
		}},

		{bop("/"), Float, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFDiv(lhs, rhs);
		}},

		{bop("%"), Float, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFRem(lhs, rhs);
		}},

		{bop("+"), Float, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFAdd(lhs, rhs);
		}},

		{bop("-"), Float, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFSub(lhs, rhs);
		}},

		{bop("<"), Float, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOLT(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop(">"), Float, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOGT(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("<="), Float, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOLE(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop(">="), Float, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOGE(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("=="), Float, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOEQ(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("!="), Float, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpONE(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		// float,int ops
		{bop("*"), Int, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFMul(lhs, b.CreateSIToFP(rhs, Float->getLLVMType(b.getContext())));
		}},

		{bop("/"), Int, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFDiv(lhs, b.CreateSIToFP(rhs, Float->getLLVMType(b.getContext())));
		}},

		{bop("%"), Int, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFRem(lhs, b.CreateSIToFP(rhs, Float->getLLVMType(b.getContext())));
		}},

		{bop("+"), Int, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFAdd(lhs, b.CreateSIToFP(rhs, Float->getLLVMType(b.getContext())));
		}},

		{bop("-"), Int, Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFSub(lhs, b.CreateSIToFP(rhs, Float->getLLVMType(b.getContext())));
		}},

		{bop("<"), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOLT(lhs, b.CreateSIToFP(rhs, Float->getLLVMType(b.getContext()))), Bool->getLLVMType(b.getContext()));
		}},

		{bop(">"), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOGT(lhs, b.CreateSIToFP(rhs, Float->getLLVMType(b.getContext()))), Bool->getLLVMType(b.getContext()));
		}},

		{bop("<="), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOLE(lhs, b.CreateSIToFP(rhs, Float->getLLVMType(b.getContext()))), Bool->getLLVMType(b.getContext()));
		}},

		{bop(">="), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOGE(lhs, b.CreateSIToFP(rhs, Float->getLLVMType(b.getContext()))), Bool->getLLVMType(b.getContext()));
		}},

		{bop("=="), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOEQ(lhs, b.CreateSIToFP(rhs, Float->getLLVMType(b.getContext()))), Bool->getLLVMType(b.getContext()));
		}},

		{bop("!="), Int, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpONE(lhs, b.CreateSIToFP(rhs, Float->getLLVMType(b.getContext()))), Bool->getLLVMType(b.getContext()));
		}},
	};
}

void types::BoolType::initOps()
{
	if (!vtable.ops.empty())
		return;

	vtable.ops = {
		// bool ops
		{uop("!"), Bool, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateNot(b.CreateTrunc(lhs, IntegerType::getInt1Ty(b.getContext()))), Bool->getLLVMType(b.getContext()));
		}},

		// bool,bool ops
		{bop("=="), Bool, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpEQ(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("!="), Bool, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpNE(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("&"), Bool, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateAnd(lhs, rhs);
		}},

		{bop("^"), Bool, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateXor(lhs, rhs);
		}},

		{bop("|"), Bool, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateOr(lhs, rhs);
		}},
	};
}

void types::ByteType::initOps()
{
	if (!vtable.ops.empty())
		return;

	vtable.ops = {
		// bool ops
		{uop("!"), Byte, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			Value *zero = ConstantInt::get(Byte->getLLVMType(b.getContext()), 0);
			return b.CreateZExt(b.CreateICmpEQ(lhs, zero), Bool->getLLVMType(b.getContext()));
		}},

		// bool,bool ops
		{bop("=="), Byte, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpEQ(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},

		{bop("!="), Byte, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpNE(lhs, rhs), Bool->getLLVMType(b.getContext()));
		}},
	};
}

types::Type *types::ByteType::getConstructType(const std::vector<types::Type *>& inTypes)
{
	if (inTypes.size() != 1 || !inTypes[0]->is(types::Int))
		throw exc::SeqException("byte constructor takes a single int argument");

	return this;
}

Type *types::IntType::getLLVMType(LLVMContext& context) const
{
	return seqIntLLVM(context);
}

Type *types::FloatType::getLLVMType(LLVMContext& context) const
{
	return llvm::Type::getDoubleTy(context);
}

Type *types::BoolType::getLLVMType(LLVMContext& context) const
{
	return IntegerType::getInt8Ty(context);
}

Type *types::ByteType::getLLVMType(LLVMContext& context) const
{
	return IntegerType::getInt8Ty(context);
}

seq_int_t types::IntType::size(Module *module) const
{
	return sizeof(seq_int_t);
}

seq_int_t types::FloatType::size(Module *module) const
{
	return sizeof(double);
}

seq_int_t types::BoolType::size(Module *module) const
{
	return sizeof(bool);
}

seq_int_t types::ByteType::size(Module *module) const
{
	return 1;
}

types::NumberType *types::NumberType::get() noexcept
{
	static NumberType instance;
	return &instance;
}

types::IntType *types::IntType::get() noexcept
{
	static IntType instance;
	return &instance;
}

types::FloatType *types::FloatType::get() noexcept
{
	static FloatType instance;
	return &instance;
}

types::BoolType *types::BoolType::get() noexcept
{
	static BoolType instance;
	return &instance;
}

types::ByteType *types::ByteType::get() noexcept
{
	static ByteType instance;
	return &instance;
}
