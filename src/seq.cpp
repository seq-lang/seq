#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include "seq/common.h"
#include "seq/util.h"
#include "seq/exc.h"
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

static types::Type *argsType()
{
	return types::ArrayType::get(types::StrType::get());
}

SeqModule::SeqModule() :
    BaseFunc(), scope(new Block()), argsVar(argsType()), initFunc(nullptr)
{
}

Block *SeqModule::getBlock()
{
	return scope;
}

Var *SeqModule::getArgVar()
{
	return &argsVar;
}

void SeqModule::codegen(Module *module)
{
	if (func)
		return;

	LLVMContext& context = module->getContext();
	this->module = module;

	types::Type *argsType = nullptr;
	Value *args = nullptr;
	argsType = types::ArrayType::get(types::StrType::get());

	func = cast<Function>(
	         module->getOrInsertFunction(
	           "main",
	           Type::getVoidTy(context),
	           argsType->getLLVMType(context)));

	auto argiter = func->arg_begin();
	args = argiter;

	/* preamble */
	preambleBlock = BasicBlock::Create(context, "preamble", func);
	IRBuilder<> builder(preambleBlock);

	initFunc = cast<Function>(module->getOrInsertFunction("seqinit", Type::getVoidTy(context)));
	initFunc->setCallingConv(CallingConv::C);
	builder.CreateCall(initFunc);

	assert(argsType != nullptr);
	argsVar.store(this, args, preambleBlock);

	BasicBlock *entry = BasicBlock::Create(context, "entry", func);
	BasicBlock *block = entry;

	scope->codegen(block);

	builder.SetInsertPoint(block);
	builder.CreateRetVoid();

	builder.SetInsertPoint(preambleBlock);
	builder.CreateBr(entry);
}

void SeqModule::codegenReturn(Value *val, types::Type *type, BasicBlock*& block)
{
	throw exc::SeqException("cannot return from SeqModule");
}

void SeqModule::codegenYield(Value *val, types::Type *type, BasicBlock*& block)
{
	throw exc::SeqException("cannot yield from SeqModule");
}

void SeqModule::execute(const std::vector<std::string>& args, bool debug)
{
	LLVMContext& context = getLLVMContext();
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();

	std::unique_ptr<Module> owner(new Module("seq", context));
	Module *module = owner.get();
	module->setTargetTriple(EngineBuilder().selectTarget()->getTargetTriple().str());
	module->setDataLayout(EngineBuilder().selectTarget()->createDataLayout());

	codegen(module);

	if (verifyModule(*module, &errs())) {
		if (debug)
			errs() << *module;
		assert(0);
	}

	std::unique_ptr<legacy::PassManager> pm(new legacy::PassManager());
	std::unique_ptr<legacy::FunctionPassManager> fpm(new legacy::FunctionPassManager(module));

	unsigned optLevel = 3;
	unsigned sizeLevel = 0;
	PassManagerBuilder builder;

	if (!debug) {
		builder.OptLevel = optLevel;
		builder.SizeLevel = sizeLevel;
		builder.Inliner = createFunctionInliningPass(optLevel, sizeLevel, false);
		builder.DisableUnitAtATime = false;
		builder.DisableUnrollLoops = false;
		builder.LoopVectorize = true;
		builder.SLPVectorize = true;
	}

	addCoroutinePassesToExtensionPoints(builder);
	builder.populateModulePassManager(*pm);
	builder.populateFunctionPassManager(*fpm);

	fpm->doInitialization();
	for (Function &f : *module)
		fpm->run(f);
	fpm->doFinalization();

	pm->run(*module);

	if (verifyModule(*module, &errs())) {
		if (debug)
			errs() << *module;
		assert(0);
	}

	if (debug)
		errs() << *module;

	EngineBuilder EB(std::move(owner));
	EB.setMCJITMemoryManager(make_unique<SectionMemoryManager>());
	EB.setUseOrcMCJITReplacement(true);
	ExecutionEngine *eng = EB.create();

	assert(initFunc);
	eng->addGlobalMapping(initFunc, (void *)util::seqinit);

	auto op = (SeqMain)eng->getPointerToFunction(func);
	auto numArgs = (seq_int_t)args.size();
	arr_t<str_t> argsArr = {numArgs, new str_t[numArgs]};

	for (seq_int_t i = 0; i < numArgs; i++)
		argsArr.arr[i] = {(seq_int_t)args[i].size(), (char *)args[i].data()};

	op(argsArr);
}

llvm::LLVMContext& seq::getLLVMContext()
{
	static LLVMContext context;
	return context;
}
