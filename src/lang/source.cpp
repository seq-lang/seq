#include "seq/seq.h"

using namespace seq;
using namespace llvm;

static Function *seqSourceInitFunc(Module *module)
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

static Function *seqSourceReadFunc(Module *module)
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

static Function *seqSourceGetFunc(Module *module)
{
	LLVMContext& context = module->getContext();
	auto *f = cast<Function>(
	            module->getOrInsertFunction(
	              "seq_source_get",
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
	              "seq_source_get_single",
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
	              "seq_source_dealloc",
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
	assert(!sources.empty());

	if (isSingle())
		return types::SeqType::get();

	return types::ArrayType::get(types::SeqType::get());
}

Source::Source(std::vector<Expr *> sources) :
    Stmt("source"), sources(std::move(sources)), scope(new Block(this)), var(new Var(determineOutType()))
{
}

Block *Source::getBlock()
{
	return scope;
}

Var *Source::getVar()
{
	return var;
}

void Source::resolveTypes()
{
	for (auto *source : sources)
		source->resolveTypes();

	scope->resolveTypes();
}

void Source::codegen0(BasicBlock*& block)
{
	Module *module = block->getModule();
	Function *initFunc = seqSourceInitFunc(module);
	Function *readFunc = seqSourceReadFunc(module);
	Function *getFunc = seqSourceGetFunc(module);
	Function *getSingleFunc = seqSourceGetSingleFunc(module);
	Function *deallocFunc = seqSourceDeallocFunc(module);

	LLVMContext& context = module->getContext();
	BasicBlock *entry = block;
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
	var->store(getBase(), val, body);

	block = body;
	scope->codegen(block);

	builder.SetInsertPoint(block);
	builder.CreateBr(loop);

	control->addIncoming(zeroLLVM(context), repeat);
	control->addIncoming(next, block);

	BasicBlock *exitLoop = BasicBlock::Create(context, "exit_loop", func);
	BasicBlock *exitRepeat = BasicBlock::Create(context, "exit_repeat", func);

	branch->setSuccessor(1, exitLoop);
	builder.SetInsertPoint(exitLoop);
	Value *done = builder.CreateICmpEQ(limit, zeroLLVM(context));
	builder.CreateCondBr(done, exitRepeat, repeat);

	builder.SetInsertPoint(exitRepeat);
	builder.CreateCall(deallocFunc, {state});

	block = exitRepeat;
}

Source *Source::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (Source *)ref->getClone(this);

	std::vector<Expr *> sourcesCloned;
	for (auto *source : sources)
		sourcesCloned.push_back(source->clone(ref));

	auto *x = new Source(sourcesCloned);
	ref->addClone(this, x);
	delete x->scope;
	delete x->var;
	x->scope = scope->clone(ref);
	x->var = var->clone(ref);
	Stmt::setCloneBase(x, ref);
	return x;
}
