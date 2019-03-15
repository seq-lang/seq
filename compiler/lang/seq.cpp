#include <iostream>
#include <memory>
#include <system_error>
#include <cassert>
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

#if LLVM_VERSION_MAJOR >= 7
using namespace llvm::orc;
#include "llvm/CodeGen/CommandFlags.inc"
#else
#include "llvm/CodeGen/CommandFlags.def"
#endif

static LLVMContext context;

SeqModule::SeqModule() :
    BaseFunc(), scope(new Block()), argVar(new Var(types::ArrayType::get(types::Str))),
    initFunc(nullptr), strlenFunc(nullptr), flags(SEQ_FLAG_FASTIO)
{
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();

	module = new Module("seq", context);
	module->setTargetTriple(EngineBuilder().selectTarget()->getTargetTriple().str());
	module->setDataLayout(EngineBuilder().selectTarget()->createDataLayout());
	argVar->setGlobal();
}

Block *SeqModule::getBlock()
{
	return scope;
}

Var *SeqModule::getArgVar()
{
	return argVar;
}

void SeqModule::setFileName(std::string file)
{
	module->setSourceFileName(file);
}

void SeqModule::setFlags(int flags)
{
	this->flags = flags;
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

	func->setPersonalityFn(makePersonalityFunc(module));
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
	PHINode *control = builder.CreatePHI(LLVM_I32(), 2, "i");
	Value *next = builder.CreateAdd(control, ConstantInt::get(LLVM_I32(), 1), "next");
	Value *cond = builder.CreateICmpSLT(control, argc);

	BasicBlock *body = BasicBlock::Create(context, "body", func);
	BranchInst *branch = builder.CreateCondBr(cond, body, body);  // we set false-branch below

	builder.SetInsertPoint(body);
	Value *arg = builder.CreateLoad(builder.CreateGEP(argv, control));
	Value *argLen = builder.CreateZExtOrTrunc(builder.CreateCall(strlen, arg), seqIntLLVM(context));
	Value *str = types::Str->make(arg, argLen, body);
	Value *idx = builder.CreateZExt(control, types::Int->getLLVMType(context));
	arrType->callMagic("__setitem__", {types::Int, types::Str}, arr, {idx, str}, body, nullptr);
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
	func->setPersonalityFn(makePersonalityFunc(module));
	auto argiter = func->arg_begin();
	args = argiter;

	/* preamble */
	preambleBlock = BasicBlock::Create(context, "preamble", func);
	IRBuilder<> builder(preambleBlock);

	initFunc = cast<Function>(module->getOrInsertFunction("seq_init", Type::getVoidTy(context), seqIntLLVM(context)));
	initFunc->setCallingConv(CallingConv::C);
	builder.CreateCall(initFunc, ConstantInt::get(seqIntLLVM(context), (uint64_t)flags));

	strlenFunc = cast<Function>(module->getOrInsertFunction("strlen", seqIntLLVM(context), IntegerType::getInt8PtrTy(context)));
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

static void verifyModuleFailFast(Module& module)
{
	if (verifyModule(module, &errs())) {
		errs() << module;
		assert(0);
	}
}

void SeqModule::verify()
{
	verifyModuleFailFast(*module);
}

static TargetMachine *getTargetMachine(Triple triple,
                                       StringRef cpuStr,
                                       StringRef featuresStr,
                                       const TargetOptions& options)
{
	std::string err;
	const Target *target = TargetRegistry::lookupTarget(MArch, triple, err);

	if (!target)
		return nullptr;

	return target->createTargetMachine(triple.getTriple(), cpuStr,
	                                   featuresStr, options, getRelocModel(),
	                                   getCodeModel(), CodeGenOpt::Aggressive);
}

#if SEQ_HAS_TAPIR
#include "llvm/Transforms/Tapir/CilkABI.h"
#include "llvm/Transforms/Tapir/OpenMPABI.h"
#endif

static void optimizeModule(Module *module, bool debug)
{
	std::unique_ptr<legacy::PassManager> pm(new legacy::PassManager());
	std::unique_ptr<legacy::FunctionPassManager> fpm(new legacy::FunctionPassManager(module));

	Triple moduleTriple(module->getTargetTriple());
	std::string cpuStr, featuresStr;
	TargetMachine *machine = nullptr;
	const TargetOptions options = InitTargetOptionsFromCodeGenFlags();
	TargetLibraryInfoImpl tlii(moduleTriple);
	pm->add(new TargetLibraryInfoWrapperPass(tlii));

	if (moduleTriple.getArch()) {
		cpuStr = getCPUStr();
		featuresStr = getFeaturesStr();
		machine = getTargetMachine(moduleTriple, cpuStr, featuresStr, options);
	}

	std::unique_ptr<TargetMachine> tm(machine);
	setFunctionAttributes(cpuStr, featuresStr, *module);
	pm->add(createTargetTransformInfoWrapperPass(tm ? tm->getTargetIRAnalysis() : TargetIRAnalysis()));
	fpm->add(createTargetTransformInfoWrapperPass(tm ? tm->getTargetIRAnalysis() : TargetIRAnalysis()));

	if (tm) {
		auto& ltm = dynamic_cast<LLVMTargetMachine&>(*tm);
		Pass *tpc = ltm.createPassConfig(*pm);
		pm->add(tpc);
	}

	unsigned optLevel = 3;
	unsigned sizeLevel = 0;
	PassManagerBuilder builder;

#if SEQ_HAS_TAPIR
	static OpenMPABI abi;
	builder.tapirTarget = &abi;
#endif

	if (!debug) {
		builder.OptLevel = optLevel;
		builder.SizeLevel = sizeLevel;
		builder.Inliner = createFunctionInliningPass(optLevel, sizeLevel, false);
		builder.DisableUnitAtATime = false;
		builder.DisableUnrollLoops = false;
		builder.LoopVectorize = true;
		builder.SLPVectorize = true;
	}

	if (tm)
		tm->adjustPassManager(builder);

	addCoroutinePassesToExtensionPoints(builder);
	builder.populateModulePassManager(*pm);
	builder.populateFunctionPassManager(*fpm);

	fpm->doInitialization();
	for (Function& f : *module)
		fpm->run(f);
	fpm->doFinalization();
	pm->run(*module);
}

void SeqModule::optimize(bool debug)
{
	optimizeModule(module, debug);
}

void SeqModule::compile(const std::string& out, bool debug)
{
	codegen(module);
	verify();
	optimize(debug);
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


/*
 * JIT
 */
#if LLVM_VERSION_MAJOR >= 7
static std::unique_ptr<Module> optimizeModule(std::unique_ptr<Module> module, bool debug)
{
	optimizeModule(module.get(), debug);
	return module;
}

SeqJIT::SeqJIT() :
    target(EngineBuilder().selectTarget()), layout(target->createDataLayout()),
    objLayer(es, [this](VModuleKey K) {
        return RTDyldObjectLinkingLayer::Resources{ std::make_shared<SectionMemoryManager>(), resolvers[K]}; }),
    comLayer(objLayer, SimpleCompiler(*target)),
    optLayer(comLayer, [](std::unique_ptr<Module> M) {
        auto module = optimizeModule(std::move(M), true);
        verifyModuleFailFast(*module);
        return module;
    }),
    callbackManager(orc::createLocalCompileCallbackManager(target->getTargetTriple(), es, 0)),
    codLayer(es, optLayer,
        [&](orc::VModuleKey K) { return resolvers[K]; },
        [&](orc::VModuleKey K, std::shared_ptr<SymbolResolver> R) { resolvers[K] = std::move(R); },
        [](Function &F) { return std::set<Function *>({&F}); },
        *callbackManager,
        orc::createLocalIndirectStubsManagerBuilder(
        target->getTargetTriple())),
    globals(),
    inputNum(0)
{
	sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
}

void SeqJIT::init()
{
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
}

std::unique_ptr<Module> SeqJIT::makeModule()
{
	auto module = make_unique<Module>("seq." + std::to_string(inputNum), context);

	module->setTargetTriple(target->getTargetTriple().str());
	module->setDataLayout(target->createDataLayout());
	return module;
}

VModuleKey SeqJIT::addModule(std::unique_ptr<Module> M)
{
	// create a new VModuleKey:
	VModuleKey K = es.allocateVModule();

	// build a resolver and associate it with the new key:
	resolvers[K] = createLegacyLookupResolver(es,
	    [this](const std::string& name) -> JITSymbol {
	        if (auto sym = comLayer.findSymbol(name, false))
	            return sym;
	        else if (auto err = sym.takeError())
	            return std::move(err);
	        if (auto symAddr = RTDyldMemoryManager::getSymbolAddressInProcess(name))
	            return JITSymbol(symAddr, JITSymbolFlags::Exported);
	        return nullptr;
	    },
	    [](Error err) { cantFail(std::move(err), "lookupFlags failed"); });

	// add the module to the JIT with the new key:
	cantFail(codLayer.addModule(K, std::move(M)));
	return K;
}

JITSymbol SeqJIT::findSymbol(std::string name)
{
	std::string mangledName;
	raw_string_ostream mangledNameStream(mangledName);
	Mangler::getNameWithPrefix(mangledNameStream, name, layout);
	return codLayer.findSymbol(mangledNameStream.str(), false);
}

void SeqJIT::removeModule(VModuleKey key)
{
	cantFail(codLayer.removeModule(key));
}

Func SeqJIT::makeFunc()
{
	Func func;
	func.setName("seq.repl.input." + std::to_string(inputNum));
	func.setIns({});
	func.setOut(types::Void);
	return func;
}

void SeqJIT::exec(Func *func, std::unique_ptr<Module> module)
{
	func->resolveTypes();
	func->codegen(module.get());
	func->getFunc()->setLinkage(GlobalValue::ExternalLinkage);

	// expose globals to the new function:
	IRBuilder<> builder(context);
	builder.SetInsertPoint(&*(*func->getFunc()->getBasicBlockList().begin()).begin());
	for (auto *var : globals) {
		Value *ptr = var->getPtr(func);
		auto sym = findSymbol(var->getName());
		auto addr = (uint64_t)cantFail(sym.getAddress());
		Value *addrVal = ConstantInt::get(seqIntLLVM(context), addr);
		Value *ptrVal = builder.CreateIntToPtr(addrVal, var->getType()->getLLVMType(context)->getPointerTo());
		builder.CreateStore(ptrVal, ptr);
	}

	verifyModuleFailFast(*module);
	addModule(std::move(module));
	auto sym = findSymbol(func->genericName());
	void (*fn)() = (void(*)())cantFail(sym.getAddress());
	fn();
}

void SeqJIT::addFunc(Func *func)
{
	auto module = makeModule();
	func->setName("seq.repl.input." + std::to_string(inputNum));
	exec(func, std::move(module));
	++inputNum;
}

void SeqJIT::addExpr(Expr *expr, bool print)
{
	auto module = makeModule();
	Func func = makeFunc();
	if (print) {
		auto *p1 = new Print(expr);
		auto *p2 = new Print(new StrExpr("\n"));
		p1->setBase(&func);
		p2->setBase(&func);
		func.getBlock()->add(p1);
		func.getBlock()->add(p2);
	} else {
		auto *e = new ExprStmt(expr);
		e->setBase(&func);
		func.getBlock()->add(e);
	}

	exec(&func, std::move(module));
	++inputNum;
}

Var *SeqJIT::addVar(Expr *expr)
{
	auto module = makeModule();
	Func func = makeFunc();
	auto *v = new VarStmt(expr);
	Var *var = v->getVar();
	var->setGlobal();
	v->setBase(&func);
	func.getBlock()->add(v);

	exec(&func, std::move(module));
	var->setREPL();
	globals.push_back(var);
	++inputNum;
	return var;
}

void SeqJIT::delVar(Var *var)
{
	auto it = std::find(globals.begin(), globals.end(), var);
	if (it != globals.end())
		globals.erase(it);
}
#endif

void seq::compilationError(const std::string& msg, const std::string& file, int line, int col)
{
	std::cerr << "\033[1m"
	          << file.substr(file.rfind('/') + 1)
	          << ":" << line << ":" << col
	          << ": \033[1;31merror:\033[0m\033[1m " << msg << "\033[0m" << std::endl;
	exit(EXIT_FAILURE);
}
