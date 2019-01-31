#include <iostream>
#include <string>
#include <map>
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::NumberType::NumberType() : Type("num", BaseType::get(), true)
{
}

types::IntType::IntType() : Type("int", NumberType::get(), false, true)
{
}

types::IntNType::IntNType(unsigned len, bool sign) :
    Type(std::string(sign ? "i" : "u") + std::to_string(len), NumberType::get(), false, true),
    len(len), sign(sign)
{
	if (len == 0 || len > MAX_LEN)
		throw exc::SeqException("integer bit width must be between 1 and " + std::to_string(MAX_LEN));
}

types::FloatType::FloatType() : Type("float", NumberType::get(), false, true)
{
}

types::BoolType::BoolType() : Type("bool", NumberType::get(), false, true)
{
}

types::ByteType::ByteType() : Type("byte", NumberType::get(), false, true)
{
}

Value *types::IntType::defaultValue(BasicBlock *block)
{
	return ConstantInt::get(getLLVMType(block->getContext()), 0);
}

Value *types::IntNType::defaultValue(BasicBlock *block)
{
	return ConstantInt::get(getLLVMType(block->getContext()), 0, sign);
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
		{"__init__", {}, Int, SEQ_MAGIC(self, args, b) {
			return Int->defaultValue(b.GetInsertBlock());
		}},

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

		{"__str__", {}, Str, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			Module *module = b.GetInsertBlock()->getModule();
			auto *strFunc = cast<Function>(
			                  module->getOrInsertFunction(
			                    "seq_str_int",
			                    Str->getLLVMType(context),
			                    getLLVMType(context)));
			strFunc->setDoesNotThrow();
			return b.CreateCall(strFunc, self);
		}},

		{"__copy__", {}, Int, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		{"__hash__", {}, Int, SEQ_MAGIC(self, args, b) {
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

		{"__truediv__", {Int}, Float, SEQ_MAGIC(self, args, b) {
			self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateFDiv(self, args[0]);
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
			Value *v = b.CreateFDiv(self, args[0]);
			Function *floor = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(),
			                                    Intrinsic::floor,
			                                    {Float->getLLVMType(b.getContext())});
			return b.CreateCall(floor, v);
		}},

		{"__truediv__", {Float}, Float, SEQ_MAGIC(self, args, b) {
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

	for (unsigned i = 1; i <= IntNType::MAX_LEN; i++) {
		vtable.magic.push_back(
			{"__init__", {IntNType::get(i, true)}, Int, SEQ_MAGIC(self, args, b) {
				return b.CreateSExtOrTrunc(args[0], Int->getLLVMType(b.getContext()));
			}}
		);

		vtable.magic.push_back(
			{"__init__", {IntNType::get(i, false)}, Int, SEQ_MAGIC(self, args, b) {
				return b.CreateZExtOrTrunc(args[0], Int->getLLVMType(b.getContext()));
			}}
		);
	}
}

void types::IntNType::initOps()
{
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__init__", {}, this, SEQ_MAGIC_CAPT(self, args, b) {
			return defaultValue(b.GetInsertBlock());
		}},

		{"__init__", {this}, this, SEQ_MAGIC(self, args, b) {
			return args[0];
		}},

		{"__init__", {Int}, this, SEQ_MAGIC_CAPT(self, args, b) {
			return sign ? b.CreateSExtOrTrunc(args[0], getLLVMType(b.getContext())) :
			              b.CreateZExtOrTrunc(args[0], getLLVMType(b.getContext()));
		}},

		{"__copy__", {}, this, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		{"__hash__", {}, Int, SEQ_MAGIC_CAPT(self, args, b) {
			return sign ? b.CreateSExtOrTrunc(self, seqIntLLVM(b.getContext())) :
			              b.CreateZExtOrTrunc(self, seqIntLLVM(b.getContext()));
		}},

		// int unary
		{"__bool__", {}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *zero = defaultValue(b.GetInsertBlock());
			return b.CreateZExt(b.CreateICmpNE(self, zero), Bool->getLLVMType(b.getContext()));
		}},

		{"__pos__", {}, this, SEQ_MAGIC(self, args, b) {
			return self;
		}},

		{"__neg__", {}, this, SEQ_MAGIC(self, args, b) {
			return b.CreateNeg(self);
		}},

		{"__invert__", {}, this, SEQ_MAGIC(self, args, b) {
			return b.CreateNot(self);
		}},

		// int,int binary
		{"__add__", {this}, this, SEQ_MAGIC(self, args, b) {
			return b.CreateAdd(self, args[0]);
		}},

		{"__sub__", {this}, this, SEQ_MAGIC(self, args, b) {
			return b.CreateSub(self, args[0]);
		}},

		{"__mul__", {this}, this, SEQ_MAGIC(self, args, b) {
			return b.CreateMul(self, args[0]);
		}},

		{"__div__", {this}, this, SEQ_MAGIC_CAPT(self, args, b) {
			return sign ? b.CreateSDiv(self, args[0]) : b.CreateUDiv(self, args[0]);
		}},

		{"__truediv__", {this}, Float, SEQ_MAGIC_CAPT(self, args, b) {
			self = sign ? b.CreateSIToFP(self, Float->getLLVMType(b.getContext())) :
			              b.CreateUIToFP(self, Float->getLLVMType(b.getContext()));
			args[0] = sign ? b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext())) :
			                 b.CreateUIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateFDiv(self, args[0]);
		}},

		{"__mod__", {this}, this, SEQ_MAGIC_CAPT(self, args, b) {
			return sign ? b.CreateSRem(self, args[0]) : b.CreateURem(self, args[0]);
		}},

		{"__lshift__", {this}, this, SEQ_MAGIC(self, args, b) {
			return b.CreateShl(self, args[0]);
		}},

		{"__rshift__", {this}, this, SEQ_MAGIC(self, args, b) {
			return b.CreateAShr(self, args[0]);
		}},

		{"__eq__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpEQ(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__ne__", {this}, Bool, SEQ_MAGIC(self, args, b) {
			return b.CreateZExt(b.CreateICmpNE(self, args[0]), Bool->getLLVMType(b.getContext()));
		}},

		{"__lt__", {this}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *cmp = sign ? b.CreateICmpSLT(self, args[0]) : b.CreateICmpULT(self, args[0]);
			return b.CreateZExt(cmp, Bool->getLLVMType(b.getContext()));
		}},

		{"__gt__", {this}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *cmp = sign ? b.CreateICmpSGT(self, args[0]) : b.CreateICmpUGT(self, args[0]);
			return b.CreateZExt(cmp, Bool->getLLVMType(b.getContext()));
		}},

		{"__le__", {this}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *cmp = sign ? b.CreateICmpSLE(self, args[0]) : b.CreateICmpULE(self, args[0]);
			return b.CreateZExt(cmp, Bool->getLLVMType(b.getContext()));
		}},

		{"__ge__", {this}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			Value *cmp = sign ? b.CreateICmpSGE(self, args[0]) : b.CreateICmpUGE(self, args[0]);
			return b.CreateZExt(cmp, Bool->getLLVMType(b.getContext()));
		}},

		{"__and__", {this}, this, SEQ_MAGIC(self, args, b) {
			return b.CreateAnd(self, args[0]);
		}},

		{"__or__", {this}, this, SEQ_MAGIC(self, args, b) {
			return b.CreateOr(self, args[0]);
		}},

		{"__xor__", {this}, this, SEQ_MAGIC(self, args, b) {
			return b.CreateXor(self, args[0]);
		}},
	};

	if (!sign && len % 2 == 0) {
		vtable.magic.push_back(
			{"__init__", {KMer::get(len/2)}, this, SEQ_MAGIC_CAPT(self, args, b) {
				return b.CreateBitCast(args[0], getLLVMType(b.getContext()));
			}}
		);
	}

	addMethod("len", new BaseFuncLite({}, types::IntType::get(), [this](Module *module) {
		const std::string name = "seq." + getName() + ".len";
		Function *func = module->getFunction(name);

		if (!func) {
			LLVMContext& context = module->getContext();
			func = cast<Function>(module->getOrInsertFunction(name, seqIntLLVM(context)));
			func->setDoesNotThrow();
			func->setLinkage(GlobalValue::PrivateLinkage);
			AttributeList v;
			v.addAttribute(context, 0, Attribute::AlwaysInline);
			func->setAttributes(v);
			BasicBlock *block = BasicBlock::Create(context, "entry", func);
			IRBuilder<> builder(block);
			builder.CreateRet(ConstantInt::get(seqIntLLVM(context), this->len));
		}

		return func;
	}), true);

	if (!sign && len % 2 == 0) {
		types::KMer *kType = types::KMer::get(len/2);
		addMethod("as_kmer", new BaseFuncLite({this}, kType, [this, kType](Module *module) {
			const std::string name = "seq." + getName() + ".as_kmer";
			Function *func = module->getFunction(name);

			if (!func) {
				LLVMContext &context = module->getContext();
				func = cast<Function>(module->getOrInsertFunction(name,
				                                                  kType->getLLVMType(context),
				                                                  getLLVMType(context)));
				func->setDoesNotThrow();
				func->setLinkage(GlobalValue::PrivateLinkage);
				AttributeList v;
				v.addAttribute(context, 0, Attribute::AlwaysInline);
				func->setAttributes(v);
				Value *arg = func->arg_begin();
				BasicBlock *block = BasicBlock::Create(context, "entry", func);
				IRBuilder<> builder(block);
				builder.CreateRet(builder.CreateBitCast(arg, kType->getLLVMType(context)));
			}

			return func;
		}), true);
	}
}

void types::FloatType::initOps()
{
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__init__", {}, Float, SEQ_MAGIC(self, args, b) {
			return Float->defaultValue(b.GetInsertBlock());
		}},

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

		{"__str__", {}, Str, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			Module *module = b.GetInsertBlock()->getModule();
			auto *strFunc = cast<Function>(
			                  module->getOrInsertFunction(
			                    "seq_str_float",
			                    Str->getLLVMType(context),
			                    getLLVMType(context)));
			strFunc->setDoesNotThrow();
			return b.CreateCall(strFunc, self);
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
			Value *v = b.CreateFDiv(self, args[0]);
			Function *floor = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(),
			                                    Intrinsic::floor,
			                                    {Float->getLLVMType(b.getContext())});
			return b.CreateCall(floor, v);
		}},

		{"__truediv__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			return b.CreateFDiv(self, args[0]);
		}},

		{"__mod__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			return b.CreateFRem(self, args[0]);
		}},

		{"__pow__", {Float}, Float, SEQ_MAGIC(self, args, b) {
			Function *pow = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(),
			                                          Intrinsic::pow,
			                                          {Float->getLLVMType(b.getContext())});
			return b.CreateCall(pow, {self, args[0]});
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
			Value *v = b.CreateFDiv(self, args[0]);
			Function *floor = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(),
			                                            Intrinsic::floor,
			                                            {Float->getLLVMType(b.getContext())});
			return b.CreateCall(floor, v);
		}},

		{"__truediv__", {Int}, Float, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateFDiv(self, args[0]);
		}},

		{"__mod__", {Int}, Float, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			return b.CreateFRem(self, args[0]);
		}},

		{"__pow__", {Int}, Float, SEQ_MAGIC(self, args, b) {
			args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
			Function *pow = Intrinsic::getDeclaration(b.GetInsertBlock()->getModule(),
			                                          Intrinsic::pow,
			                                          {Float->getLLVMType(b.getContext())});
			return b.CreateCall(pow, {self, args[0]});
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
		{"__init__", {}, Bool, SEQ_MAGIC(self, args, b) {
			return Bool->defaultValue(b.GetInsertBlock());
		}},

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

		{"__str__", {}, Str, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			Module *module = b.GetInsertBlock()->getModule();
			auto *strFunc = cast<Function>(
			                  module->getOrInsertFunction(
			                    "seq_str_bool",
			                    Str->getLLVMType(context),
			                    getLLVMType(context)));
			strFunc->setDoesNotThrow();
			return b.CreateCall(strFunc, self);
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
		{"__init__", {}, Byte, SEQ_MAGIC(self, args, b) {
			return Byte->defaultValue(b.GetInsertBlock());
		}},

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

		{"__str__", {}, Str, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			Module *module = b.GetInsertBlock()->getModule();
			auto *strFunc = cast<Function>(
			                  module->getOrInsertFunction(
			                    "seq_str_byte",
			                    Str->getLLVMType(context),
			                    getLLVMType(context)));
			strFunc->setDoesNotThrow();
			return b.CreateCall(strFunc, self);
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

Type *types::IntNType::getLLVMType(LLVMContext& context) const
{
	return IntegerType::getIntNTy(context, len);
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

size_t types::IntNType::size(Module *module) const
{
	return module->getDataLayout().getTypeAllocSize(getLLVMType(module->getContext()));
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

types::IntNType *types::IntNType::get(unsigned len, bool sign)
{
	static std::map<int, IntNType *> cache;

	int key = (sign ? 1 : -1) * (int)len;
	if (cache.find(key) != cache.end())
		return cache.find(key)->second;

	auto *intNType = new IntNType(len, sign);
	cache.insert({key, intNType});
	return intNType;
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

bool types::IntNType::is(types::Type *type) const
{
	auto *iN = dynamic_cast<types::IntNType *>(type);
	return iN && (len == iN->len && sign == iN->sign);
}
