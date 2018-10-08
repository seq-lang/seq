#include <iostream>
#include <system_error>
#include <cassert>
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

SeqModule::SeqModule(std::string source) :
    BaseFunc(), scope(new Block()), argVar(new Var(types::ArrayType::get(types::Str))),
    initFunc(nullptr), strlenFunc(nullptr)
{
	static LLVMContext context;
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();

	module = new Module("seq", context);
	module->setSourceFileName(source);
	module->setTargetTriple(EngineBuilder().selectTarget()->getTargetTriple().str());
	module->setDataLayout(EngineBuilder().selectTarget()->createDataLayout());
}

Block *SeqModule::getBlock()
{
	return scope;
}

Var *SeqModule::getArgVar()
{
	return argVar;
}

void SeqModule::resolveTypes()
{
	scope->resolveTypes();
}

static Function *makeCanonicalMainFunc(Function *realMain, Function *strlen)
{
#define LLVM_I32() IntegerType::getInt32Ty(context)
	LLVMContext& context = realMain->getContext();
	Module *module = realMain->getParent();

	types::ArrayType *arrType = types::ArrayType::get(types::Str);

	auto *func = cast<Function>(
	               module->getOrInsertFunction(
	                 "main",
	                 LLVM_I32(),
	                 LLVM_I32(),
	                 PointerType::get(IntegerType::getInt8PtrTy(context), 0)));

	auto argiter = func->arg_begin();
	Value *argc = argiter++;
	Value *argv = argiter;
	argc->setName("argc");
	argv->setName("argv");

	BasicBlock *entry = BasicBlock::Create(context, "entry", func);
	BasicBlock *loop = BasicBlock::Create(context, "loop", func);

	IRBuilder<> builder(entry);
	Value *len = builder.CreateZExt(argc, seqIntLLVM(context));
	Value *ptr = types::Str->alloc(len, entry);
	Value *arr = arrType->make(ptr, len, entry);
	builder.CreateBr(loop);

	builder.SetInsertPoint(loop);
	PHINode *control = builder.CreatePHI(IntegerType::getInt32Ty(context), 2, "i");
	Value *next = builder.CreateAdd(control, ConstantInt::get(LLVM_I32(), 1), "next");
	Value *cond = builder.CreateICmpSLT(control, argc);

	BasicBlock *body = BasicBlock::Create(context, "body", func);
	BranchInst *branch = builder.CreateCondBr(cond, body, body);  // we set false-branch below

	builder.SetInsertPoint(body);
	Value *arg = builder.CreateLoad(builder.CreateGEP(argv, control));
	Value *argLen = builder.CreateZExtOrTrunc(builder.CreateCall(strlen, arg), seqIntLLVM(context));
	Value *str = types::Str->make(arg, argLen, body);
	arrType->indexStore(nullptr, arr, control, str, body);
	builder.CreateBr(loop);

	control->addIncoming(ConstantInt::get(LLVM_I32(), 0), entry);
	control->addIncoming(next, body);

	BasicBlock *exit = BasicBlock::Create(context, "exit", func);
	branch->setSuccessor(1, exit);

	builder.SetInsertPoint(exit);
	builder.CreateCall(realMain, {arr});
	builder.CreateRet(ConstantInt::get(LLVM_I32(), 0));

	return func;
#undef LLVM_I32
}

void SeqModule::codegen(Module *module)
{
	assert(module);

	if (func)
		return;

	resolveTypes();
	LLVMContext& context = module->getContext();
	this->module = module;

	types::Type *argsType = nullptr;
	Value *args = nullptr;
	argsType = types::ArrayType::get(types::Str);

	func = cast<Function>(
	         module->getOrInsertFunction(
	           "seq.main",
	           Type::getVoidTy(context),
	           argsType->getLLVMType(context)));

	func->setLinkage(GlobalValue::PrivateLinkage);
	auto argiter = func->arg_begin();
	args = argiter;

	/* preamble */
	preambleBlock = BasicBlock::Create(context, "preamble", func);
	IRBuilder<> builder(preambleBlock);

	initFunc = cast<Function>(module->getOrInsertFunction("seq_init", Type::getVoidTy(context)));
	initFunc->setCallingConv(CallingConv::C);
	builder.CreateCall(initFunc);

	strlenFunc = cast<Function>(module->getOrInsertFunction("strlen", Type::getIntNTy(context, 8*sizeof(size_t)), IntegerType::getInt8PtrTy(context)));
	strlenFunc->setCallingConv(CallingConv::C);

	assert(argsType != nullptr);
	argVar->store(this, args, preambleBlock);

	BasicBlock *entry = BasicBlock::Create(context, "entry", func);
	BasicBlock *block = entry;

	scope->codegen(block);

	builder.SetInsertPoint(block);
	builder.CreateRetVoid();

	builder.SetInsertPoint(preambleBlock);
	builder.CreateBr(entry);

	func = makeCanonicalMainFunc(func, strlenFunc);
}

void SeqModule::verify()
{
	if (verifyModule(*module, &errs())) {
		errs() << *module;
		assert(0);
	}
}

void SeqModule::optimize(bool debug)
{
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

	builder.MergeFunctions = true;
	addCoroutinePassesToExtensionPoints(builder);
	builder.populateModulePassManager(*pm);
	builder.populateFunctionPassManager(*fpm);

	fpm->doInitialization();
	for (Function& f : *module)
		fpm->run(f);
	fpm->doFinalization();
	pm->run(*module);
}

void SeqModule::compile(const std::string& out, bool debug)
{
	codegen(module);
	verify();
	optimize(debug);
	verify();

	if (debug)
		errs() << *module;

	std::error_code err;
	raw_fd_ostream stream(out, err, llvm::sys::fs::F_None);

#if LLVM_VERSION_MAJOR >= 7
	WriteBitcodeToFile(*module, stream);
#else
	WriteBitcodeToFile(module, stream);
#endif

	module = nullptr;

	if (err) {
		std::cerr << "error: " << err.message() << std::endl;
		exit(err.value());
	}
}

void SeqModule::execute(const std::vector<std::string>& args,
                        const std::vector<std::string>& libs,
                        bool debug)
{
	codegen(module);
	verify();
	optimize(debug);
	verify();

	if (debug)
		errs() << *module;

	std::unique_ptr<Module> owner(module);
	module = nullptr;
	EngineBuilder EB(std::move(owner));
	EB.setMCJITMemoryManager(make_unique<SectionMemoryManager>());
	EB.setUseOrcMCJITReplacement(true);
	ExecutionEngine *eng = EB.create();

	assert(initFunc);
	assert(strlenFunc);
	eng->addGlobalMapping(initFunc, (void *)seq_init);
	eng->addGlobalMapping(strlenFunc, (void *)strlen);

	std::string err;
	for (auto& lib : libs) {
		if (sys::DynamicLibrary::LoadLibraryPermanently(lib.c_str(), &err)) {
			std::cerr << "error: " << err << std::endl;
			exit(EXIT_FAILURE);
		}
	}

	eng->runFunctionAsMain(func, args, nullptr);
}
