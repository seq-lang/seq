#include "seq/seq.h"

using namespace seq;
using namespace llvm;

static inline Function *seqSourceNewFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seq_source_new",
	              IntegerType::getInt8PtrTy(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

static inline Function *seqSourceInitFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seq_source_init",
	              Type::getVoidTy(context),
	              IntegerType::getInt8PtrTy(context),
	              types::Str->getLLVMType(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

static inline Function *seqSourceReadFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seq_source_read",
	              seqIntLLVM(context),
	              IntegerType::getInt8PtrTy(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

static inline Function *seqSourceGetFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seq_source_get",
	              types::ArrayType::get(types::Seq)->getLLVMType(context),
	              IntegerType::getInt8PtrTy(context),
	              seqIntLLVM(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

static inline Function *seqSourceGetSingleFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seq_source_get_single",
	              types::Seq->getLLVMType(context),
	              IntegerType::getInt8PtrTy(context),
	              seqIntLLVM(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

static inline Function *seqSourceDeallocFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seq_source_dealloc",
	              Type::getVoidTy(context),
	              IntegerType::getInt8PtrTy(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

types::SourceType::SourceType() : Type("source", BaseType::get(), false, true)
{
	addMethod("read", new BaseFuncLite({this}, types::IntType::get(), [](Module *module) {
		return seqSourceReadFunc(module);
	}), false);

	addMethod("get", new BaseFuncLite({this, types::IntType::get()}, types::SeqType::get(), [](Module *module) {
		return seqSourceGetSingleFunc(module);
	}), false);

	addMethod("get_multi", new BaseFuncLite({this, types::IntType::get()}, types::ArrayType::get(types::SeqType::get()), [](Module *module) {
		return seqSourceGetFunc(module);
	}), false);

	addMethod("close", new BaseFuncLite({this}, types::VoidType::get(), [](Module *module) {
		return seqSourceDeallocFunc(module);
	}), false);
}

bool types::SourceType::isAtomic() const
{
	return false;
}

void types::SourceType::initOps()
{
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__new__", {}, this, SEQ_MAGIC(self, args, b) {
			Function *newFunc = seqSourceNewFunc(b.GetInsertBlock()->getModule());
			return b.CreateCall(newFunc);
		}},

		{"__init__", {Str}, this, SEQ_MAGIC(self, args, b) {
			Function *initFunc = seqSourceInitFunc(b.GetInsertBlock()->getModule());
			b.CreateCall(initFunc, {self, args[0]});
			return self;
		}},
	};
}

Type *types::SourceType::getLLVMType(LLVMContext& context) const
{
	return IntegerType::getInt8PtrTy(context);
}

size_t types::SourceType::size(Module *module) const
{
	return sizeof(void *);
}

types::SourceType *types::SourceType::get() noexcept
{
	return new SourceType();
}

/************************************************************************/

static inline Function *seqRawNewFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seq_raw_new",
	              IntegerType::getInt8PtrTy(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

static inline Function *seqRawInitFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seq_raw_init",
	              Type::getVoidTy(context),
	              IntegerType::getInt8PtrTy(context),
	              types::Str->getLLVMType(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

static inline Function *seqRawReadFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seq_raw_read",
	              types::Seq->getLLVMType(context),
	              IntegerType::getInt8PtrTy(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

static inline Function *seqRawDeallocFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seq_raw_dealloc",
	              Type::getVoidTy(context),
	              IntegerType::getInt8PtrTy(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

types::RawType::RawType() : Type("file", BaseType::get(), false, true)
{
	addMethod("read", new BaseFuncLite({this}, types::SeqType::get(), [](Module *module) {
		return seqRawReadFunc(module);
	}), false);

	addMethod("close", new BaseFuncLite({this}, types::VoidType::get(), [](Module *module) {
		return seqRawDeallocFunc(module);
	}), false);
}

bool types::RawType::isAtomic() const
{
	return false;
}

void types::RawType::initOps()
{
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__new__", {}, this, SEQ_MAGIC(self, args, b) {
			Function *newFunc = seqRawNewFunc(b.GetInsertBlock()->getModule());
			return b.CreateCall(newFunc);
		}},

		{"__init__", {Str}, this, SEQ_MAGIC(self, args, b) {
			Function *initFunc = seqRawInitFunc(b.GetInsertBlock()->getModule());
			b.CreateCall(initFunc, {self, args[0]});
			return self;
		}},
	};
}

Type *types::RawType::getLLVMType(LLVMContext& context) const
{
	return IntegerType::getInt8PtrTy(context);
}

size_t types::RawType::size(Module *module) const
{
	return sizeof(void *);
}

types::RawType *types::RawType::get() noexcept
{
	return new RawType();
}
