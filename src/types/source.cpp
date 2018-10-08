#include "seq/seq.h"

using namespace seq;
using namespace llvm;

static inline Function *seqSourceInitFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seq_source_init",
	              IntegerType::getInt8PtrTy(context),
	              PointerType::get(IntegerType::getInt8PtrTy(context), 0),
	              seqIntLLVM(context)));
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

types::SourceType::SourceType() : Type("Source", BaseType::get())
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

Value *types::SourceType::construct(BaseFunc *base,
                                    const std::vector<Value *>& args,
                                    BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();
	Function *initFunc = seqSourceInitFunc(module);
	BasicBlock *preambleBlock = base->getPreamble();
	Value *sources = makeAlloca(IntegerType::getInt8PtrTy(context), preambleBlock, args.size());
	IRBuilder<> builder(block);

	unsigned idx = 0;
	for (auto *str : args) {
		Value *idxVal = ConstantInt::get(seqIntLLVM(context), idx++);
		Value *slot = builder.CreateGEP(sources, idxVal);
		Value *strVal = types::Str->memb(str, "ptr", block);
		builder.CreateStore(strVal, slot);
	}

	Value *numSources = ConstantInt::get(seqIntLLVM(context), args.size());
	Value *source = builder.CreateCall(initFunc, {sources, numSources});
	return source;
}

bool types::SourceType::isAtomic() const
{
	return false;
}

types::Type *types::SourceType::getConstructType(const std::vector<types::Type *>& inTypes)
{
	if (inTypes.empty())
		throw exc::SeqException("Source constructor takes at least one argument");

	for (auto *type : inTypes) {
		if (!types::is(type, types::Str))
			throw exc::SeqException("Source constructor takes only string arguments");
	}

	return this;
}

Type *types::SourceType::getLLVMType(LLVMContext& context) const
{
	return IntegerType::getInt8PtrTy(context);
}

seq_int_t types::SourceType::size(Module *module) const
{
	return sizeof(void *);
}

types::SourceType *types::SourceType::get() noexcept
{
	return new SourceType();
}

/************************************************************************/

static inline Function *seqRawInitFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seq_raw_init",
	              IntegerType::getInt8PtrTy(context),
	              PointerType::get(IntegerType::getInt8PtrTy(context), 0)));
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

types::RawType::RawType() : Type("RawFile", BaseType::get())
{
	addMethod("read", new BaseFuncLite({this}, types::SeqType::get(), [](Module *module) {
		return seqRawReadFunc(module);
	}), false);

	addMethod("close", new BaseFuncLite({this}, types::VoidType::get(), [](Module *module) {
		return seqRawDeallocFunc(module);
	}), false);
}

Value *types::RawType::construct(BaseFunc *base,
                                 const std::vector<Value *>& args,
                                 BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();
	Function *initFunc = seqRawInitFunc(module);
	BasicBlock *preambleBlock = base->getPreamble();
	Value *sources = makeAlloca(IntegerType::getInt8PtrTy(context), preambleBlock, args.size());
	IRBuilder<> builder(block);

	unsigned idx = 0;
	for (auto *str : args) {
		Value *idxVal = ConstantInt::get(seqIntLLVM(context), idx++);
		Value *slot = builder.CreateGEP(sources, idxVal);
		Value *strVal = types::Str->memb(str, "ptr", block);
		builder.CreateStore(strVal, slot);
	}

	Value *source = builder.CreateCall(initFunc, {sources});
	return source;
}

bool types::RawType::isAtomic() const
{
	return false;
}

types::Type *types::RawType::getConstructType(const std::vector<types::Type *>& inTypes)
{
	if (inTypes.empty())
		throw exc::SeqException("RawFile constructor takes at least one argument");

	for (auto *type : inTypes) {
		if (!types::is(type, types::Str))
			throw exc::SeqException("RawFile constructor takes only string arguments");
	}

	return this;
}

Type *types::RawType::getLLVMType(LLVMContext& context) const
{
	return IntegerType::getInt8PtrTy(context);
}

seq_int_t types::RawType::size(Module *module) const
{
	return sizeof(void *);
}

types::RawType *types::RawType::get() noexcept
{
	return new RawType();
}
