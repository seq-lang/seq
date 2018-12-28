#include <iostream>
#include <cstdio>
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::NumberType::NumberType() : Type("num", BaseType::get())
{
}

types::IntType::IntType() : Type("int", NumberType::get())
{
}

types::FloatType::FloatType() : Type("float", NumberType::get())
{
}

types::BoolType::BoolType() : Type("bool", NumberType::get())
{
}

types::ByteType::ByteType() : Type("byte", NumberType::get())
{
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

void types::IntType::initOps()
{
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__init__", {Int}, Int, SEQ_MAGIC(self, args, b) {
			return args[0];
		}},

		{"__init__", {Float}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateFPToSI(args[0], Int->getLLVMType(b.getContext()));
		}},

		{"__init__", {Byte}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(args[0], Int->getLLVMType(b.getContext()));
		}},

		{"__print__", {}, Void, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			Module *module = b.GetInsertBlock()->getModule();
			auto *printFunc = cast<Function>(
			                    module->getOrInsertFunction(
			                      "seq_print_int",
			                      llvm::Type::getVoidTy(context),
			                      getLLVMType(context)));
			printFunc->setDoesNotThrow();
			b.CreateCall(printFunc, self);
			return (Value *)nullptr;
		}},

		{"__copy__", {}, Int, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		// int unary
		{"__bool__", {}, Bool, SEQ_MAGIC(self, args, b) {
			Value *zero = ConstantInt::get(Int->getLLVMType(b.getContext()), 0);
			return b.CreateZExt(b.CreateICmpNE(self, zero), Bool->getLLVMType(b.getContext()));
		}},

		{"__pos__", {}, Int, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		{"__neg__", {}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateNeg(self);
		}},

		{"__invert__", {}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateNot(self);
		}},

		// int,int binary
		{"__add__", {Int}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateAdd(self, args[0]);
		}},

		{"__sub__", {Int}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateSub(self, args[0]);
		}},

		{"__mul__", {Int}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateMul(self, args[0]);
		}},

		{"__div__", {Int}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateSDiv(self, args[0]);
		}},

		{"__mod__", {Int}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateSRem(self, args[0]);
		}},

		{"__lshift__", {Int}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateShl(self, args[0]);
		}},

		{"__rshift__", {Int}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateAShr(self, args[0]);
		}},

		{"__eq__", {Int}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpEQ(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ne__", {Int}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpNE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__lt__", {Int}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpSLT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__gt__", {Int}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpSGT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__le__", {Int}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpSLE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ge__", {Int}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpSGE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__and__", {Int}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateAnd(self, args[0]);
		}},

		{"__or__", {Int}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateOr(self, args[0]);
		}},

		{"__xor__", {Int}, Int, SEQ_MAGIC(self, args, b) {
			return b.CreateXor(self, args[0]);
		}},

		// int,float binary
		{"__add__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
			return b.CreateFAdd(self, args[0]);
		}},

		{"__sub__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
			return b.CreateFSub(self, args[0]);
		}},

		{"__mul__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
			return b.CreateFMul(self, args[0]);
		}},

		{"__div__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
			return b.CreateFDiv(self, args[0]);
		}},

		{"__mod__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
			return b.CreateFRem(self, args[0]);
		}},

		{"__eq__", {Float}, Bool, SEQ_MAGIC(self, args, b) {
			self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
			return b.CreateZExt(b.CreateFCmpOEQ(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ne__", {Float}, Bool, SEQ_MAGIC(self, args, b) {
			self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
			return b.CreateZExt(b.CreateFCmpONE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__lt__", {Float}, Bool, SEQ_MAGIC(self, args, b) {
			self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
			return b.CreateZExt(b.CreateFCmpOLT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__gt__", {Float}, Bool, SEQ_MAGIC(self, args, b) {
			self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
			return b.CreateZExt(b.CreateFCmpOGT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__le__", {Float}, Bool, SEQ_MAGIC(self, args, b) {
			self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
			return b.CreateZExt(b.CreateFCmpOLE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ge__", {Float}, Bool, SEQ_MAGIC(self, args, b) {
			self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
			return b.CreateZExt(b.CreateFCmpOGE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},
	};
}

void types::FloatType::initOps()
{
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__init__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			return args[0];
		}},

		{"__init__", {Int}, Float, SEQ_MAGIC(self, args, b) {
			return b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
		}},

		{"__print__", {}, Void, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			Module *module = b.GetInsertBlock()->getModule();
			auto *printFunc = cast<Function>(
			                    module->getOrInsertFunction(
			                      "seq_print_float",
			                      llvm::Type::getVoidTy(context),
			                      getLLVMType(context)));
			printFunc->setDoesNotThrow();
			b.CreateCall(printFunc, self);
			return (Value *)nullptr;
		}},

		{"__copy__", {}, Float, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		// float unary
		{"__bool__", {}, Bool, SEQ_MAGIC(self, args, b) {
			Value *zero = ConstantFP::get(Float->getLLVMType(b.getContext()), 0.0);
			return b.CreateZExt(b.CreateFCmpONE(self, zero), Bool->getLLVMType(b.getContext()));
		}},

		{"__pos__", {}, Float, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		{"__neg__", {}, Float, SEQ_MAGIC(self, args, b) {
			return b.CreateFNeg(self);
		}},

		// float,float binary
		{"__add__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			return b.CreateFAdd(self, args[0]);
		}},

		{"__sub__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			return b.CreateFSub(self, args[0]);
		}},

		{"__mul__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			return b.CreateFMul(self, args[0]);
		}},

		{"__div__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			return b.CreateFDiv(self, args[0]);
		}},

		{"__mod__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			return b.CreateFRem(self, args[0]);
		}},

		{"__eq__", {Float}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateFCmpOEQ(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ne__", {Float}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateFCmpONE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__lt__", {Float}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateFCmpOLT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__gt__", {Float}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateFCmpOGT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__le__", {Float}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateFCmpOLE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ge__", {Float}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateFCmpOGE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		// float,int binary
		{"__add__", {Int}, Float, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateFAdd(self, args[0]);
		}},

		{"__sub__", {Int}, Float, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateFSub(self, args[0]);
		}},

		{"__mul__", {Int}, Float, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateFMul(self, args[0]);
		}},

		{"__div__", {Int}, Float, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateFDiv(self, args[0]);
		}},

		{"__mod__", {Int}, Float, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateFRem(self, args[0]);
		}},

		{"__eq__", {Int}, Bool, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateZExt(b.CreateFCmpOEQ(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ne__", {Int}, Bool, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateZExt(b.CreateFCmpONE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__lt__", {Int}, Bool, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateZExt(b.CreateFCmpOLT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__gt__", {Int}, Bool, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateZExt(b.CreateFCmpOGT(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__le__", {Int}, Bool, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateZExt(b.CreateFCmpOLE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ge__", {Int}, Bool, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateZExt(b.CreateFCmpOGE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},
	};
}

void types::BoolType::initOps()
{
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__print__", {}, Void, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			Module *module = b.GetInsertBlock()->getModule();
			auto *printFunc = cast<Function>(
			                    module->getOrInsertFunction(
			                      "seq_print_bool",
			                      llvm::Type::getVoidTy(context),
			                      getLLVMType(context)));
			printFunc->setDoesNotThrow();
			b.CreateCall(printFunc, self);
			return (Value *)nullptr;
		}},

		{"__copy__", {}, Bool, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		{"__bool__", {}, Bool, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		{"__invert__", {}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateNot(b.CreateTrunc(self, IntegerType::getInt1Ty(b.getContext()))), Bool->getLLVMType(b.getContext()));
		}},

		{"__eq__", {Bool}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpEQ(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ne__", {Bool}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpNE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__and__", {Bool}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateAnd(self, args[0]);
		}},

		{"__or__", {Bool}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateOr(self, args[0]);
		}},

		{"__xor__", {Bool}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateXor(self, args[0]);
		}},
	};
}

void types::ByteType::initOps()
{
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__init__", {Byte}, Byte, SEQ_MAGIC(self, args, b) {
			return args[0];
		}},

		{"__init__", {Int}, Byte, SEQ_MAGIC(self, args, b) {
			return b.CreateTrunc(args[0], Byte->getLLVMType(b.getContext()));
		}},

		{"__print__", {}, Void, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			Module *module = b.GetInsertBlock()->getModule();
			auto *printFunc = cast<Function>(
			                    module->getOrInsertFunction(
			                      "seq_print_byte",
			                      llvm::Type::getVoidTy(context),
			                      getLLVMType(context)));
			printFunc->setDoesNotThrow();
			b.CreateCall(printFunc, self);
			return (Value *)nullptr;
		}},

		{"__copy__", {}, Byte, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		{"__bool__", {}, Bool, SEQ_MAGIC(self, args, b) {
			Value *zero = ConstantInt::get(Byte->getLLVMType(b.getContext()), 0);
			return b.CreateZExt(b.CreateICmpNE(self, zero), Bool->getLLVMType(b.getContext()));
		}},

		{"__eq__", {Byte}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpEQ(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ne__", {Byte}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpNE(self, args[0]), Bool->getLLVMType(b.getContext()));
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

Type *types::ByteType::getLLVMType(LLVMContext& context) const
{
	return IntegerType::getInt8Ty(context);
}

size_t types::IntType::size(Module *module) const
{
	return sizeof(seq_int_t);
}

size_t types::FloatType::size(Module *module) const
{
	return sizeof(double);
}

size_t types::BoolType::size(Module *module) const
{
	return sizeof(bool);
}

size_t types::ByteType::size(Module *module) const
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
