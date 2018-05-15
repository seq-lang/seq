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

Value *types::IntType::checkEq(BaseFunc *base,
                               ValMap ins1,
                               ValMap ins2,
                               BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *n1 = builder.CreateLoad(getSafe(ins1, SeqData::INT));
	Value *n2 = builder.CreateLoad(getSafe(ins2, SeqData::INT));

	return builder.CreateICmpEQ(n1, n2);
}

Value *types::FloatType::checkEq(BaseFunc *base,
                                 ValMap ins1,
                                 ValMap ins2,
                                 BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *f1 = builder.CreateLoad(getSafe(ins1, SeqData::FLOAT));
	Value *f2 = builder.CreateLoad(getSafe(ins2, SeqData::FLOAT));

	return builder.CreateFCmpOEQ(f1, f2);
}

Value *types::BoolType::checkEq(BaseFunc *base,
                                ValMap ins1,
                                ValMap ins2,
                                BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *b1 = builder.CreateLoad(getSafe(ins1, SeqData::BOOL));
	Value *b2 = builder.CreateLoad(getSafe(ins2, SeqData::BOOL));

	return builder.CreateICmpEQ(b1, b2);
}

void types::IntType::initOps()
{
	if (!vtable.ops.empty())
		return;

	vtable.ops = {
		// int ops
		{uop("~"), &Int, &Int, [](Value *lhs, llvm::Value *rhs, llvm::IRBuilder<>& b) {
			return b.CreateNot(lhs);
		}},

		{uop("-"), &Int, &Int, [](Value *lhs, llvm::Value *rhs, llvm::IRBuilder<>& b) {
			return b.CreateNeg(lhs);
		}},

		// int,int ops
		{bop("*"), &Int, &Int, [](Value *lhs, llvm::Value *rhs, llvm::IRBuilder<>& b) {
			return b.CreateMul(lhs, rhs);
		}},

		{bop("/"), &Int, &Int, [](Value *lhs, llvm::Value *rhs, llvm::IRBuilder<>& b) {
			return b.CreateSDiv(lhs, rhs);
		}},

		{bop("%"), &Int, &Int, [](Value *lhs, llvm::Value *rhs, llvm::IRBuilder<>& b) {
			return b.CreateSRem(lhs, rhs);
		}},

		{bop("+"), &Int, &Int, [](Value *lhs, llvm::Value *rhs, llvm::IRBuilder<>& b) {
			return b.CreateAdd(lhs, rhs);
		}},

		{bop("-"), &Int, &Int, [](Value *lhs, llvm::Value *rhs, llvm::IRBuilder<>& b) {
			return b.CreateSub(lhs, rhs);
		}},

		// int,float ops
		{bop("*"), &Float, &Float, [](Value *lhs, llvm::Value *rhs, llvm::IRBuilder<>& b) {
			return b.CreateMul(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs);
		}},

		{bop("/"), &Float, &Float, [](Value *lhs, llvm::Value *rhs, llvm::IRBuilder<>& b) {
			return b.CreateSDiv(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs);
		}},

		{bop("%"), &Float, &Float, [](Value *lhs, llvm::Value *rhs, llvm::IRBuilder<>& b) {
			return b.CreateSRem(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs);
		}},

		{bop("+"), &Float, &Float, [](Value *lhs, llvm::Value *rhs, llvm::IRBuilder<>& b) {
			return b.CreateAdd(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs);
		}},

		{bop("-"), &Float, &Float, [](Value *lhs, llvm::Value *rhs, llvm::IRBuilder<>& b) {
			return b.CreateSub(b.CreateSIToFP(lhs, Float.getLLVMType(b.getContext())), rhs);
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
