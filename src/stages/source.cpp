#include "seq/seq.h"
#include "seq/exc.h"
#include "seq/source.h"

using namespace seq;
using namespace llvm;

static inline void ioError(const std::string& msg)
{
	std::cerr << "IO error: " << msg << std::endl;
	abort();
}

struct IOState {
	io::DataBlock data;
	std::vector<std::ifstream *> ins;
	io::Format fmt;

	IOState(char **sources, const seq_int_t numSources) :
	    data(), ins()
	{
		if (numSources == 0)
			ioError("sequence source not specified");

		if (numSources > io::MAX_INPUTS)
			ioError("too many inputs (max: " + std::to_string(io::MAX_INPUTS) + ")");

		fmt = io::extractExt(sources[0]);

		for (seq_int_t i = 1; i < numSources; i++) {
			if (io::extractExt(sources[i]) != fmt)
				ioError("inconsistent input formats");
		}

		for (seq_int_t i = 0; i < numSources; i++) {
			ins.push_back(new std::ifstream(sources[i]));
			if (!ins.back()->good())
				ioError("could not open '" + std::string(sources[i]) + "' for reading");
		}
	}

	void close()
	{
		for (auto *in : ins) {
			in->close();
			delete in;
		}
	}
};

SEQ_FUNC void *seqSourceInit(char **sources, seq_int_t numSources)
{
	auto *state = (IOState *)seqAlloc(sizeof(IOState));
	new (state) IOState(sources, numSources);
	return state;
}

SEQ_FUNC seq_int_t seqSourceRead(void *state)
{
	auto *ioState = (IOState *)state;
	ioState->data.read(ioState->ins, ioState->fmt);
	return (seq_int_t)ioState->data.len;
}

SEQ_FUNC arr_t<seq_t> seqSourceGet(void *state, seq_int_t idx)
{
	auto *ioState = (IOState *)state;
	return ioState->data.block[idx].getSeqs(ioState->ins.size());
}

SEQ_FUNC seq_t seqSourceGetSingle(void *state, seq_int_t idx)
{
	auto *ioState = (IOState *)state;
	return ioState->data.block[idx].getSeq();
}

SEQ_FUNC void seqSourceDealloc(void *state)
{
	auto *ioState = (IOState *)state;
	ioState->close();
}

static Function *seqSourceInitFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seqSourceInit",
	              IntegerType::getInt8PtrTy(context),
	              PointerType::get(IntegerType::getInt8PtrTy(context), 0),
	              seqIntLLVM(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

static Function *seqSourceReadFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seqSourceRead",
	              seqIntLLVM(context),
	              IntegerType::getInt8PtrTy(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

static Function *seqSourceGetFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seqSourceGet",
	              types::ArrayType::get(types::SeqType::get())->getLLVMType(context),
	              IntegerType::getInt8PtrTy(context),
	              seqIntLLVM(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

static Function *seqSourceGetSingleFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seqSourceGetSingle",
	              types::SeqType::get()->getLLVMType(context),
	              IntegerType::getInt8PtrTy(context),
	              seqIntLLVM(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

static Function *seqSourceDeallocFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seqSourceDealloc",
	              Type::getVoidTy(context),
	              IntegerType::getInt8PtrTy(context)));
	f->setCallingConv(CallingConv::C);
	return f;
}

bool Source::isSingle() const
{
	return sources.size() == 1;
}

types::Type *Source::determineOutType() const
{
	if (sources.empty())
		throw exc::SeqException("no sources given to source stage");

	if (isSingle())
		return types::SeqType::get();

	return types::ArrayType::get(types::SeqType::get());
}

Source::Source(std::vector<Expr *> sources) :
    Stage("source"), sources(std::move(sources))
{
}

void Source::validate()
{
	out = determineOutType();
}

void Source::codegen(Module *module)
{
	ensurePrev();
	validate();

	Function *initFunc = seqSourceInitFunc(module);
	Function *readFunc = seqSourceReadFunc(module);
	Function *getFunc = seqSourceGetFunc(module);
	Function *getSingleFunc = seqSourceGetSingleFunc(module);
	Function *deallocFunc = seqSourceDeallocFunc(module);

	LLVMContext& context = module->getContext();
	BasicBlock *entry = prev->getAfter();
	BasicBlock *preambleBlock = getBase()->getPreamble();
	Function *func = entry->getParent();

	Value *sourcesVar = makeAlloca(IntegerType::getInt8PtrTy(context), preambleBlock, sources.size());
	IRBuilder<> builder(entry);

	unsigned idx = 0;
	for (auto *expr : sources) {
		expr->ensure(types::StrType::get());
		Value *str = expr->codegen(getBase(), entry);
		Value *idxVal = ConstantInt::get(seqIntLLVM(context), idx++);
		Value *slot = builder.CreateGEP(sourcesVar, idxVal);
		Value *strVal = types::Str.memb(str, "ptr", entry);
		builder.CreateStore(strVal, slot);
	}

	Value *numSourcesVal = ConstantInt::get(seqIntLLVM(context), sources.size());
	Value *sourcesVarCasted = builder.CreateBitCast(sourcesVar, PointerType::get(IntegerType::getInt8PtrTy(context), 0));
	Value *state = builder.CreateCall(initFunc, {sourcesVarCasted, numSourcesVal});

	BasicBlock *repeat = BasicBlock::Create(context, "repeat", func);
	builder.CreateBr(repeat);
	builder.SetInsertPoint(repeat);

	Value *limit = builder.CreateCall(readFunc, {state});

	BasicBlock *loop = BasicBlock::Create(context, "loop", func);
	builder.CreateBr(loop);
	builder.SetInsertPoint(loop);

	PHINode *control = builder.CreatePHI(seqIntLLVM(context), 2, "i");
	Value *next = builder.CreateAdd(control, oneLLVM(context), "next");
	Value *cond = builder.CreateICmpSLT(control, limit);

	BasicBlock *body = BasicBlock::Create(context, "body", func);
	BranchInst *branch = builder.CreateCondBr(cond, body, body);  // we set false-branch below

	builder.SetInsertPoint(body);
	Value *val = builder.CreateCall(isSingle() ? getSingleFunc : getFunc, {state, control});
	result = getOutType()->storeInAlloca(getBase(), val, body, true);

	block = body;

	codegenNext(module);

	builder.SetInsertPoint(getAfter());
	builder.CreateBr(loop);

	control->addIncoming(zeroLLVM(context), repeat);
	control->addIncoming(next, getAfter());

	BasicBlock *exitLoop = BasicBlock::Create(context, "exit_loop", func);
	BasicBlock *exitRepeat = BasicBlock::Create(context, "exit_repeat", func);

	branch->setSuccessor(1, exitLoop);
	builder.SetInsertPoint(exitLoop);
	Value *done = builder.CreateICmpEQ(limit, zeroLLVM(context));
	builder.CreateCondBr(done, exitRepeat, repeat);

	builder.SetInsertPoint(exitRepeat);
	builder.CreateCall(deallocFunc, {state});

	prev->setAfter(exitRepeat);
}

void Source::finalize(Module *module, ExecutionEngine *eng)
{
	Function *initFunc = seqSourceInitFunc(module);
	Function *readFunc = seqSourceReadFunc(module);
	Function *getFunc = seqSourceGetFunc(module);
	Function *getSingleFunc = seqSourceGetSingleFunc(module);
	Function *deallocFunc = seqSourceDeallocFunc(module);

	eng->addGlobalMapping(initFunc, (void *)seqSourceInit);
	eng->addGlobalMapping(readFunc, (void *)seqSourceRead);
	eng->addGlobalMapping(getFunc, (void *)seqSourceGet);
	eng->addGlobalMapping(getSingleFunc, (void *)seqSourceGetSingle);
	eng->addGlobalMapping(deallocFunc, (void *)seqSourceDealloc);

	Stage::finalize(module, eng);
}

Source& Source::make(std::vector<Expr *> sources)
{
	return *new Source(std::move(sources));
}
