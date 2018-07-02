#include <iostream>
#include <cstdio>
#include "seq/seq.h"
#include "seq/base.h"
#include "seq/num.h"

using namespace seq;
using namespace llvm;

SEQ_FUNC void printInt(seq_int_t x)
{
	std::cout << x << std::endl;
}

SEQ_FUNC void printFloat(double n)
{
	std::cout << n << std::endl;
}

SEQ_FUNC void printBool(bool b)
{
	std::cout << (b ? "true" : "false") << std::endl;
}

types::NumberType::NumberType() : Type("Num", BaseType::get())
{
}

types::IntType::IntType() : Type("Int", NumberType::get(), SeqData::INT)
{
	vtable.print = (void *)printInt;
}

types::FloatType::FloatType() : Type("Float", NumberType::get(), SeqData::FLOAT)
{
	vtable.print = (void *)printFloat;
}

types::BoolType::BoolType() : Type("Bool", NumberType::get(), SeqData::BOOL)
{
	vtable.print = (void *)printBool;
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

void types::IntType::initOps()
{
	if (!vtable.ops.empty())
		return;

	vtable.ops = {
		// int ops
		{uop("~"), &Int, &Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateNot(lhs);
		}},

		{uop("-"), &Int, &Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateNeg(lhs);
		}},

		{uop("+"), &Int, &Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return lhs;
		}},

		// int,int ops
		{bop("*"), &Int, &Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateMul(lhs, rhs);
		}},

		{bop("/"), &Int, &Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateSDiv(lhs, rhs);
		}},

		{bop("%"), &Int, &Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateSRem(lhs, rhs);
		}},

		{bop("+"), &Int, &Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateAdd(lhs, rhs);
		}},

		{bop("-"), &Int, &Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateSub(lhs, rhs);
		}},

		{bop("<<"), &Int, &Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateShl(lhs, rhs);
		}},

		{bop(">>"), &Int, &Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateAShr(lhs, rhs);
		}},

		{bop("<"), &Int, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpSLT(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop(">"), &Int, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpSGT(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop("<="), &Int, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpSLE(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop(">="), &Int, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpSGE(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop("=="), &Int, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpEQ(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop("!="), &Int, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpNE(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop("&"), &Int, &Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateAnd(lhs, rhs);
		}},

		{bop("^"), &Int, &Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateXor(lhs, rhs);
		}},

		{bop("|"), &Int, &Int, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateOr(lhs, rhs);
		}},

		// int,float ops
		{bop("*"), &Float, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFMul(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs);
		}},

		{bop("/"), &Float, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFDiv(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs);
		}},

		{bop("%"), &Float, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFRem(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs);
		}},

		{bop("+"), &Float, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFAdd(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs);
		}},

		{bop("-"), &Float, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFSub(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs);
		}},

		{bop("<"), &Float, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOLT(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop(">"), &Float, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOGT(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop("<="), &Float, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOLE(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop(">="), &Float, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOGE(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop("=="), &Float, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOEQ(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop("!="), &Float, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpONE(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs), Bool.getLLVMType(b.getContext()));
		}},
	};
}

void types::FloatType::initOps()
{
	if (!vtable.ops.empty())
		return;

	vtable.ops = {
		// float ops
		{uop("-"), &Float, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFNeg(lhs);
		}},

		{uop("+"), &Float, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return lhs;
		}},

		// float,float ops
		{bop("*"), &Float, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFMul(lhs, rhs);
		}},

		{bop("/"), &Float, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFDiv(lhs, rhs);
		}},

		{bop("%"), &Float, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFRem(lhs, rhs);
		}},

		{bop("+"), &Float, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFAdd(lhs, rhs);
		}},

		{bop("-"), &Float, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFSub(lhs, rhs);
		}},

		{bop("<"), &Float, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOLT(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop(">"), &Float, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOGT(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop("<="), &Float, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOLE(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop(">="), &Float, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOGE(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop("=="), &Float, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOEQ(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop("!="), &Float, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpONE(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		// float,int ops
		{bop("*"), &Int, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFMul(lhs, b.CreateSIToFP(rhs, Float.getLLVMType(b.getContext())));
		}},

		{bop("/"), &Int, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFDiv(lhs, b.CreateSIToFP(rhs, Float.getLLVMType(b.getContext())));
		}},

		{bop("%"), &Int, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFRem(lhs, b.CreateSIToFP(rhs, Float.getLLVMType(b.getContext())));
		}},

		{bop("+"), &Int, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFAdd(lhs, b.CreateSIToFP(rhs, Float.getLLVMType(b.getContext())));
		}},

		{bop("-"), &Int, &Float, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateFSub(lhs, b.CreateSIToFP(rhs, Float.getLLVMType(b.getContext())));
		}},

		{bop("<"), &Int, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOLT(lhs, b.CreateSIToFP(rhs, Float.getLLVMType(b.getContext()))), Bool.getLLVMType(b.getContext()));
		}},

		{bop(">"), &Int, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOGT(lhs, b.CreateSIToFP(rhs, Float.getLLVMType(b.getContext()))), Bool.getLLVMType(b.getContext()));
		}},

		{bop("<="), &Int, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOLE(lhs, b.CreateSIToFP(rhs, Float.getLLVMType(b.getContext()))), Bool.getLLVMType(b.getContext()));
		}},

		{bop(">="), &Int, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOGE(lhs, b.CreateSIToFP(rhs, Float.getLLVMType(b.getContext()))), Bool.getLLVMType(b.getContext()));
		}},

		{bop("=="), &Int, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpOEQ(lhs, b.CreateSIToFP(rhs, Float.getLLVMType(b.getContext()))), Bool.getLLVMType(b.getContext()));
		}},

		{bop("!="), &Int, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateFCmpONE(lhs, b.CreateSIToFP(rhs, Float.getLLVMType(b.getContext()))), Bool.getLLVMType(b.getContext()));
		}},
	};
}

void types::BoolType::initOps()
{
	if (!vtable.ops.empty())
		return;

	vtable.ops = {
		// bool ops
		{uop("!"), &Bool, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateNot(b.CreateTrunc(lhs, IntegerType::getInt1Ty(b.getContext()))), Bool.getLLVMType(b.getContext()));
		}},

		// bool,bool ops
		{bop("=="), &Bool, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpEQ(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop("!="), &Bool, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateICmpNE(lhs, rhs), Bool.getLLVMType(b.getContext()));
		}},

		{bop("&"), &Bool, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateAnd(lhs, rhs);
		}},

		{bop("^"), &Bool, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateXor(lhs, rhs);
		}},

		{bop("|"), &Bool, &Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateOr(lhs, rhs);
		}},
	};
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

types::NumberType *types::NumberType::get()
{
	static NumberType instance;
	return &instance;
}

types::IntType *types::IntType::get()
{
	static IntType instance;
	return &instance;
}

types::FloatType *types::FloatType::get()
{
	static FloatType instance;
	return &instance;
}

types::BoolType *types::BoolType::get()
{
	static BoolType instance;
	return &instance;
}
