#include "seq/seq.h"

using namespace seq;
using namespace llvm;

Print::Print(Expr *expr) :
    Stmt("print"), expr(expr)
{
}

void Print::resolveTypes()
{
	expr->resolveTypes();
}

void Print::codegen0(BasicBlock*& block)
{
	Value *val = expr->codegen(getBase(), block);
	expr->getType()->callMagic("__print__", {}, val, {}, block, getTryCatch());
}

Print *Print::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (Print *)ref->getClone(this);

	auto *x = new Print(expr->clone(ref));
	ref->addClone(this, x);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

ExprStmt::ExprStmt(Expr *expr) :
    Stmt("expr"), expr(expr)
{
}

void ExprStmt::resolveTypes()
{
	expr->resolveTypes();
}

void ExprStmt::codegen0(BasicBlock*& block)
{
	expr->codegen(getBase(), block);
}

ExprStmt *ExprStmt::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (ExprStmt *)ref->getClone(this);

	auto *x = new ExprStmt(expr->clone(ref));
	ref->addClone(this, x);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

VarStmt::VarStmt(Expr *init) :
    Stmt("var"), init(init), var(new Var())
{
}

Var *VarStmt::getVar()
{
	return var;
}

void VarStmt::resolveTypes()
{
	init->resolveTypes();
	var->setType(init->getType());
}

void VarStmt::codegen0(BasicBlock*& block)
{
	Value *val = init->codegen(getBase(), block);
	var->store(getBase(), val, block);
}

VarStmt *VarStmt::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (VarStmt *)ref->getClone(this);

	auto *x = new VarStmt(init->clone(ref));
	ref->addClone(this, x);
	x->var = var->clone(ref);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

FuncStmt::FuncStmt(Func *func) :
    Stmt("func"), func(func)
{
}

void FuncStmt::resolveTypes()
{
	func->resolveTypes();
}

void FuncStmt::codegen0(BasicBlock*& block)
{
}

FuncStmt *FuncStmt::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (FuncStmt *)ref->getClone(this);

	auto *x = new FuncStmt(nullptr);
	ref->addClone(this, x);
	x->func = func->clone(ref);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

Assign::Assign(Var *var, Expr *value) :
    Stmt("(=)"), var(var), value(value)
{
}

void Assign::resolveTypes()
{
	value->resolveTypes();
}

void Assign::codegen0(BasicBlock*& block)
{
	value->ensure(var->getType());
	Value *val = value->codegen(getBase(), block);
	var->store(getBase(), val, block);
}

Assign *Assign::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (Assign *)ref->getClone(this);

	auto *x = new Assign(var->clone(ref), value->clone(ref));
	ref->addClone(this, x);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

AssignIndex::AssignIndex(Expr *array, Expr *idx, Expr *value) :
    Stmt("([]=)"), array(array), idx(idx), value(value)
{
}

void AssignIndex::resolveTypes()
{
	array->resolveTypes();
	idx->resolveTypes();
	value->resolveTypes();
}

void AssignIndex::codegen0(BasicBlock*& block)
{
	Value *val = value->codegen(getBase(), block);
	Value *arr = array->codegen(getBase(), block);
	Value *idx = this->idx->codegen(getBase(), block);
	array->getType()->callMagic("__setitem__",
	                            {this->idx->getType(), value->getType()},
	                            arr,
	                            {idx, val},
	                            block,
	                            getTryCatch());
}

AssignIndex *AssignIndex::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (AssignIndex *)ref->getClone(this);

	auto *x = new AssignIndex(array->clone(ref), idx->clone(ref), value->clone(ref));
	ref->addClone(this, x);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

DelIndex::DelIndex(Expr *array, Expr *idx) :
    Stmt("del []"), array(array), idx(idx)
{
}

void DelIndex::resolveTypes()
{
	array->resolveTypes();
	idx->resolveTypes();
}

void DelIndex::codegen0(BasicBlock*& block)
{
	Value *arr = array->codegen(getBase(), block);
	Value *idx = this->idx->codegen(getBase(), block);
	array->getType()->callMagic("__delitem__",
	                            {this->idx->getType()},
	                            arr,
	                            {idx},
	                            block,
	                            getTryCatch());
}

DelIndex *DelIndex::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (DelIndex *)ref->getClone(this);

	auto *x = new DelIndex(array->clone(ref), idx->clone(ref));
	ref->addClone(this, x);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

AssignMember::AssignMember(Expr *expr, std::string memb, Expr *value) :
    Stmt("(.=)"), expr(expr), memb(std::move(memb)), value(value)
{
}

AssignMember::AssignMember(Expr *expr, seq_int_t idx, Expr *value) :
    AssignMember(expr, std::to_string(idx), value)
{
}

void AssignMember::resolveTypes()
{
	expr->resolveTypes();
	value->resolveTypes();
}

void AssignMember::codegen0(BasicBlock*& block)
{
	value->ensure(expr->getType()->membType(memb));
	Value *x = expr->codegen(getBase(), block);
	Value *v = value->codegen(getBase(), block);
	expr->getType()->setMemb(x, memb, v, block);
}

AssignMember *AssignMember::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (AssignMember *)ref->getClone(this);

	auto *x = new AssignMember(expr->clone(ref), memb, value->clone(ref));
	ref->addClone(this, x);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

If::If() :
    Stmt("if"), conds(), branches(), elseAdded(false)
{
}

void If::resolveTypes()
{
	for (auto *cond : conds)
		cond->resolveTypes();

	for (auto *branch : branches)
		branch->resolveTypes();
}

void If::codegen0(BasicBlock*& block)
{
	assert(!conds.empty() && conds.size() == branches.size());

	LLVMContext& context = block->getContext();
	Function *func = block->getParent();
	IRBuilder<> builder(block);
	std::vector<BranchInst *> binsts;
	TryCatch *tc = getTryCatch();

	for (unsigned i = 0; i < conds.size(); i++) {
		Value *cond = conds[i]->codegen(getBase(), block);
		cond = conds[i]->getType()->boolValue(cond, block, tc);
		Block *branch = branches[i];

		builder.SetInsertPoint(block);  // recall: expr codegen can change the block
		cond = builder.CreateTrunc(cond, IntegerType::getInt1Ty(context));

		BasicBlock *b1 = BasicBlock::Create(context, "", func);
		BranchInst *binst1 = builder.CreateCondBr(cond, b1, b1);  // we set false-branch below

		block = b1;
		branch->codegen(block);
		builder.SetInsertPoint(block);
		BranchInst *binst2 = builder.CreateBr(b1);  // we reset this below
		binsts.push_back(binst2);

		BasicBlock *b2 = BasicBlock::Create(context, "", func);
		binst1->setSuccessor(1, b2);
		block = b2;
	}

	BasicBlock *after = BasicBlock::Create(context, "", func);
	builder.SetInsertPoint(block);
	builder.CreateBr(after);

	for (auto *binst : binsts)
		binst->setSuccessor(0, after);

	block = after;
}

Block *If::addCond(Expr *cond)
{
	assert(!elseAdded);
	auto *branch = new Block(this);
	conds.push_back(cond);
	branches.push_back(branch);
	return branch;
}

Block *If::addElse()
{
	assert(!elseAdded);
	Block *branch = addCond(new BoolExpr(true));
	elseAdded = true;
	return branch;
}

Block *If::getBlock(unsigned int idx)
{
	assert(idx < branches.size());
	return branches[idx];
}

If *If::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (If *)ref->getClone(this);

	auto *x = new If();
	ref->addClone(this, x);

	std::vector<Expr *> condsCloned;
	std::vector<Block *> branchesCloned;

	for (auto *cond : conds)
		condsCloned.push_back(cond->clone(ref));

	for (auto *branch : branches)
		branchesCloned.push_back(branch->clone(ref));

	x->conds = condsCloned;
	x->branches = branchesCloned;
	x->elseAdded = elseAdded;

	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

TryCatch::TryCatch() :
    Stmt("try"), scope(new Block(this)), catchTypes(), catchBlocks(),
    catchVars(), finally(new Block(this)), exceptionBlock(nullptr)
{
}

Block *TryCatch::getBlock()
{
	return scope;
}

Var *TryCatch::getVar(unsigned idx)
{
	assert(idx < catchVars.size());
	return catchVars[idx];
}

Block *TryCatch::addCatch(types::Type *type)
{
	if (!type) {
		// make sure we have at most one catch-all:
		for (auto *catchType : catchTypes)
			assert(catchType);
	}

	auto *block = new Block(this);
	catchTypes.push_back(type);
	catchBlocks.push_back(block);
	catchVars.push_back(type ? new Var(type) : nullptr);
	return block;
}

Block *TryCatch::getFinally()
{
	return finally;
}

BasicBlock *TryCatch::getExceptionBlock()
{
	assert(exceptionBlock);
	return exceptionBlock;
}

void TryCatch::resolveTypes()
{
	scope->resolveTypes();
	for (auto *block : catchBlocks)
		block->resolveTypes();
	finally->resolveTypes();
}

void TryCatch::codegen0(BasicBlock*& block)
{
	assert(catchTypes.size() == catchBlocks.size());

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();
	Function *func = block->getParent();
	BaseFunc *base = getBase();
	BasicBlock *preambleBlock = base->getPreamble();

	// entry block:
	BasicBlock *entryBlock = BasicBlock::Create(context, "entry", func);

	// unwind block for invoke
	exceptionBlock = BasicBlock::Create(context, "exception", func);

	// block which routes exception to correct catch handler block
	BasicBlock *exceptionRouteBlock = BasicBlock::Create(context, "exceptionRoute", func);

	// foreign exception handler
	BasicBlock *externalExceptionBlock = BasicBlock::Create(context, "externalException", func);

	// block which calls _Unwind_Resume
	BasicBlock *unwindResumeBlock = BasicBlock::Create(context, "unwindResume", func);

	// clean up block which delete exception if needed
	BasicBlock *endBlock = llvm::BasicBlock::Create(context, "end", func);

	StructType *padType = StructType::get(IntegerType::getInt8PtrTy(context),
	                                      IntegerType::getInt32Ty(context));
	StructType *typeInfoType = StructType::get(IntegerType::getInt32Ty(context));
	StructType *unwindType = StructType::get(IntegerType::getInt64Ty(context));  // header only
	StructType *excType = StructType::get(typeInfoType, IntegerType::getInt8PtrTy(context));

	// exception storage/state:
	ConstantInt *excStateNotThrown = ConstantInt::get(Type::getInt8Ty(context), 0);
	ConstantInt *excStateThrown    = ConstantInt::get(Type::getInt8Ty(context), 1);
	ConstantInt *excStateCaught    = ConstantInt::get(Type::getInt8Ty(context), 2);
	Value *excFlag    = makeAlloca(excStateNotThrown, preambleBlock);
	Value *excStore   = makeAlloca(ConstantPointerNull::get(IntegerType::getInt8PtrTy(context)), preambleBlock);
	Value *catchStore = makeAlloca(ConstantAggregateZero::get(padType), preambleBlock);

	// finally:
	BasicBlock *finallyBlock = BasicBlock::Create(context, "finally", func);
	finally->codegen(finallyBlock);
	IRBuilder<> builder(finallyBlock);
	SwitchInst *theSwitch = builder.CreateSwitch(builder.CreateLoad(excFlag), endBlock, 2);
	theSwitch->addCase(excStateCaught, endBlock);
	theSwitch->addCase(excStateThrown, unwindResumeBlock);

	std::vector<BasicBlock *> catches;
	for (unsigned i = 0; i < catchTypes.size(); i++) {
		BasicBlock *catchBlock = BasicBlock::Create(context, "catch" + std::to_string(i+1), func);
		catches.push_back(catchBlock);
	}

	// codegen try:
	scope->codegen(entryBlock);

	// make sure we always get to finally block:
	builder.SetInsertPoint(entryBlock);
	builder.CreateBr(finallyBlock);

	// rethrow if uncaught:
	builder.SetInsertPoint(unwindResumeBlock);
	builder.CreateResume(builder.CreateLoad(catchStore));

	// exception handling:
	builder.SetInsertPoint(exceptionBlock);
	LandingPadInst *caughtResult = builder.CreateLandingPad(padType, (unsigned)catchTypes.size());
	caughtResult->setCleanup(true);
	std::vector<Value *> typeIndices;

	for (auto *catchType : catchTypes) {
		if (catchType && !catchType->asRef())
			throw exc::SeqException("cannot catch non-reference type '" + catchType->getName() + "'");

		const std::string typeVarName = catchType ? ("seq.typeidx." + catchType->getName()) : "";
		Value *tidx = catchType ? (Value *)module->getGlobalVariable(typeVarName) :
		                          ConstantPointerNull::get(PointerType::get(typeInfoType, 0));
		int idx = catchType ? catchType->getID() : 0;
		if (!tidx)
			tidx = new GlobalVariable(*module,
			                          typeInfoType,
			                          true,
			                          GlobalValue::PrivateLinkage,
			                          ConstantStruct::get(typeInfoType,
			                                              ConstantInt::get(IntegerType::getInt32Ty(context),
			                                                               (uint64_t)idx,
			                                                               true)),
			                          typeVarName);
		typeIndices.push_back(tidx);
		caughtResult->addClause(ConstantInt::get(seqIntLLVM(context), (uint64_t)catchType->getID()));
	}

	Value *unwindException = builder.CreateExtractValue(caughtResult, 0);
	Value *retTypeInfoIndex = builder.CreateExtractValue(caughtResult, 1);

	builder.CreateStore(caughtResult, catchStore);
	builder.CreateStore(unwindException, excStore);
	builder.CreateStore(excStateThrown, excFlag);

	Value *unwindExceptionClass =
	  builder.CreateLoad(builder.CreateStructGEP(
	    unwindType,
	    builder.CreatePointerCast(unwindException, unwindType->getPointerTo()),
	    0));

	// check for foreign exceptions:
	builder.CreateCondBr(builder.CreateICmpEQ(unwindExceptionClass, ConstantInt::get(IntegerType::getInt64Ty(context), seq_exc_class())),
	                     exceptionRouteBlock,
	                     externalExceptionBlock);

	// external exception:
	{
		// TODO: just failing now, but can actually handle this
		BoolExpr b(false);
		Assert fail(&b);
		fail.codegen(externalExceptionBlock);
	}
	builder.SetInsertPoint(externalExceptionBlock);
	builder.CreateBr(finallyBlock);

	// reroute Seq exceptions:
	builder.SetInsertPoint(exceptionRouteBlock);
	Value *excVal = builder.CreatePointerCast(
	                  builder.CreateConstGEP1_64(unwindException, (uint64_t)seq_exc_offset()),
	                  excType->getPointerTo());
	//Value *typeInfoThrown = builder.CreateStructGEP(excType, excVal, 0);
	Value *objPtr = builder.CreateStructGEP(excType, excVal, 1);
	//Value *typeInfoThrownType = builder.CreateStructGEP(builder.getInt8PtrTy(), typeInfoThrown, 0);

	SwitchInst *switchToCatchBlock = builder.CreateSwitch(retTypeInfoIndex,
	                                                      finallyBlock,
	                                                      (unsigned)catches.size());
	for (unsigned i = 0; i < catches.size(); i++) {
		BasicBlock *catchBlock = catches[i];
		switchToCatchBlock->addCase(
		  ConstantInt::get(IntegerType::getInt32Ty(context), i + 1),
		  catchBlock);

		builder.SetInsertPoint(catchBlock);
		Var *var = catchVars[i];

		if (var) {
			Value *obj = builder.CreateBitCast(objPtr, catchTypes[i]->getLLVMType(context));
			var->store(base, obj, catchBlock);
		}

		catchBlocks[i]->codegen(catchBlock);
		builder.SetInsertPoint(catchBlock);
		builder.CreateStore(excStateCaught, excFlag);
		builder.CreateBr(finallyBlock);
	}

	// link in our new blocks, and update the caller's block:
	builder.SetInsertPoint(block);
	builder.CreateBr(entryBlock);
	block = endBlock;
}

TryCatch *TryCatch::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (TryCatch *)ref->getClone(this);

	auto *x = new TryCatch();
	ref->addClone(this, x);
	x->scope = scope->clone(ref);

	std::vector<types::Type *> catchTypesCloned;
	std::vector<Block *> catchBlocksCloned;
	std::vector<Var *> catchVarsCloned;

	for (auto *type : catchTypes)
		catchTypesCloned.push_back(type->clone(ref));

	for (auto *block : catchBlocks)
		catchBlocksCloned.push_back(block->clone(ref));

	for (auto *var : catchVars)
		catchVarsCloned.push_back(var->clone(ref));

	x->catchTypes = catchTypesCloned;
	x->catchBlocks = catchBlocksCloned;
	x->catchVars = catchVarsCloned;
	x->finally = finally->clone(ref);

	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

Throw::Throw(Expr *expr) :
    Stmt("throw"), expr(expr)
{
}

void Throw::resolveTypes()
{
	expr->resolveTypes();
}

void Throw::codegen0(BasicBlock*& block)
{
	types::Type *type = expr->getType();
	if (!type->asRef())
		throw exc::SeqException("cannot throw non-reference type '" + type->getName() + "'");

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();
	Function *excAllocFunc = makeExcAllocFunc(module);
	Function *throwFunc = makeThrowFunc(module);

	Value *obj = expr->codegen(getBase(), block);
	IRBuilder<> builder(block);
	Value *exc = builder.CreateCall(excAllocFunc,
	                                {ConstantInt::get(IntegerType::getInt32Ty(context), (uint64_t)type->getID(), true),
	                                 obj});
	builder.CreateCall(throwFunc, exc);
}

Throw *Throw::clone(Generic *ref)
{
	SEQ_RETURN_CLONE(new Throw(expr->clone(ref)));
}

Match::Match() :
    Stmt("match"), value(nullptr), patterns(), branches()
{
}

void Match::resolveTypes()
{
	assert(value);
	value->resolveTypes();

	for (auto *pattern : patterns)
		pattern->resolveTypes(value->getType());

	for (auto *branch : branches)
		branch->resolveTypes();
}

void Match::codegen0(BasicBlock*& block)
{
	assert(!patterns.empty() && patterns.size() == branches.size() && value);

	LLVMContext& context = block->getContext();
	Function *func = block->getParent();

	IRBuilder<> builder(block);
	types::Type *valType = value->getType();

	bool seenCatchAll = false;
	for (auto *pattern : patterns) {
		pattern->resolveTypes(valType);
		if (pattern->isCatchAll())
			seenCatchAll = true;
	}

	if (!seenCatchAll)
		throw exc::SeqException("match statement missing catch-all pattern");

	Value *val = value->codegen(getBase(), block);

	std::vector<BranchInst *> binsts;

	for (unsigned i = 0; i < patterns.size(); i++) {
		Value *cond = patterns[i]->codegen(getBase(), valType, val, block);
		Block *branch = branches[i];

		builder.SetInsertPoint(block);  // recall: expr codegen can change the block

		BasicBlock *b1 = BasicBlock::Create(context, "", func);
		BranchInst *binst1 = builder.CreateCondBr(cond, b1, b1);  // we set false-branch below

		block = b1;
		branch->codegen(block);
		builder.SetInsertPoint(block);
		BranchInst *binst2 = builder.CreateBr(b1);  // we reset this below
		binsts.push_back(binst2);

		BasicBlock *b2 = BasicBlock::Create(context, "", func);
		binst1->setSuccessor(1, b2);
		block = b2;
	}

	BasicBlock *after = BasicBlock::Create(context, "", func);
	builder.SetInsertPoint(block);
	builder.CreateBr(after);

	for (auto *binst : binsts)
		binst->setSuccessor(0, after);

	block = after;
}

void Match::setValue(Expr *value)
{
	assert(!this->value);
	this->value = value;
}

Block *Match::addCase(Pattern *pattern)
{
	assert(value);
	auto *branch = new Block(this);
	patterns.push_back(pattern);
	branches.push_back(branch);
	return branch;
}

Match *Match::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (Match *)ref->getClone(this);

	auto *x = new Match();
	ref->addClone(this, x);

	std::vector<Pattern *> patternsCloned;
	std::vector<Block *> branchesCloned;

	for (auto *pattern : patterns)
		patternsCloned.push_back(pattern->clone(ref));

	for (auto *branch : branches)
		branchesCloned.push_back(branch->clone(ref));

	if (value) x->value = value->clone(ref);
	x->patterns = patternsCloned;
	x->branches = branchesCloned;

	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

While::While(Expr *cond) :
    Stmt("while"), cond(cond), scope(new Block(this))
{
	loop = true;
}

Block *While::getBlock()
{
	return scope;
}

void While::resolveTypes()
{
	cond->resolveTypes();
	scope->resolveTypes();
}

void While::codegen0(BasicBlock*& block)
{
	LLVMContext& context = block->getContext();
	BasicBlock *entry = block;
	Function *func = entry->getParent();
	IRBuilder<> builder(entry);

	BasicBlock *loop0 = BasicBlock::Create(context, "while", func);
	BasicBlock *loop = loop0;
	builder.CreateBr(loop);

	Value *cond = this->cond->codegen(getBase(), loop);  // recall: this can change `loop`
	cond = this->cond->getType()->boolValue(cond, loop, getTryCatch());
	builder.SetInsertPoint(loop);
	cond = builder.CreateTrunc(cond, IntegerType::getInt1Ty(context));

	BasicBlock *body = BasicBlock::Create(context, "body", func);
	BranchInst *branch = builder.CreateCondBr(cond, body, body);  // we set false-branch below

	block = body;

	scope->codegen(block);

	builder.SetInsertPoint(block);
	builder.CreateBr(loop0);

	BasicBlock *exit = BasicBlock::Create(context, "exit", func);
	branch->setSuccessor(1, exit);
	block = exit;

	setBreaks(exit);
	setContinues(loop0);
}

While *While::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (While *)ref->getClone(this);

	auto *x = new While(cond->clone(ref));
	ref->addClone(this, x);
	delete x->scope;
	x->scope = scope->clone(ref);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

For::For(Expr *gen) :
    Stmt("for"), gen(gen), scope(new Block(this)), var(new Var())
{
	loop = true;
}

Block *For::getBlock()
{
	return scope;
}

Var *For::getVar()
{
	return var;
}

void For::resolveTypes()
{
	gen->resolveTypes();

	try {
		types::GenType *genType = gen->getType()->magicOut("__iter__", {})->asGen();

		if (!genType)
			throw exc::SeqException("__iter__ does not return a generator");

		var->setType(genType->getBaseType(0));
	} catch (exc::SeqException& e) {
		e.setSrcInfo(getSrcInfo());
		throw e;
	}

	scope->resolveTypes();
}

void For::codegen0(BasicBlock*& block)
{
	types::Type *type = gen->getType()->magicOut("__iter__", {});
	types::GenType *genType = type->asGen();

	if (!genType)
		throw exc::SeqException("cannot iterate over object of type '" + type->getName() + "'");

	LLVMContext& context = block->getContext();
	BasicBlock *entry = block;
	Function *func = entry->getParent();

	Value *gen = this->gen->codegen(getBase(), entry);
	gen = this->gen->getType()->callMagic("__iter__", {}, gen, {}, entry, getTryCatch());

	IRBuilder<> builder(entry);
	BasicBlock *loopCont = BasicBlock::Create(context, "for_cont", func);
	BasicBlock *loop = BasicBlock::Create(context, "for", func);
	builder.CreateBr(loop);

	builder.SetInsertPoint(loopCont);
	builder.CreateBr(loop);

	builder.SetInsertPoint(loop);
	genType->resume(gen, loop);
	Value *cond = genType->done(gen, loop);
	BasicBlock *body = BasicBlock::Create(context, "body", func);
	BranchInst *branch = builder.CreateCondBr(cond, body, body);  // we set true-branch below

	block = body;
	if (!genType->getBaseType(0)->is(types::Void)) {
		Value *val = genType->promise(gen, block);
		var->store(getBase(), val, block);
	}

	scope->codegen(block);

	builder.SetInsertPoint(block);
	builder.CreateBr(loop);

	BasicBlock *cleanup = BasicBlock::Create(context, "cleanup", func);
	branch->setSuccessor(0, cleanup);
	genType->destroy(gen, cleanup);

	builder.SetInsertPoint(cleanup);
	BasicBlock *exit = BasicBlock::Create(context, "exit", func);
	builder.CreateBr(exit);
	block = exit;

	setBreaks(exit);
	setContinues(loopCont);
}

For *For::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (For *)ref->getClone(this);

	auto *x = new For(gen->clone(ref));
	ref->addClone(this, x);
	delete x->scope;
	x->scope = scope->clone(ref);
	x->var = var->clone(ref);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

Return::Return(Expr *expr) :
    Stmt("return"), expr(expr)
{
}

Expr *Return::getExpr()
{
	return expr;
}

void Return::resolveTypes()
{
	if (expr) expr->resolveTypes();
}

void Return::codegen0(BasicBlock*& block)
{
	types::Type *type = expr ? expr->getType() : types::Void;
	Value *val = expr ? expr->codegen(getBase(), block) : nullptr;
	auto *func = dynamic_cast<Func *>(getBase());
	assert(func);
	func->codegenReturn(val, type, block);
}

Return *Return::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (Return *)ref->getClone(this);

	auto *x = new Return(expr ? expr->clone(ref) : nullptr);
	ref->addClone(this, x);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

Yield::Yield(Expr *expr) :
    Stmt("yield"), expr(expr)
{
}

Expr *Yield::getExpr()
{
	return expr;
}

void Yield::resolveTypes()
{
	if (expr) expr->resolveTypes();
}

void Yield::codegen0(BasicBlock*& block)
{
	types::Type *type = expr ? expr->getType() : types::Void;
	Value *val = expr ? expr->codegen(getBase(), block) : nullptr;
	auto *func = dynamic_cast<Func *>(getBase());
	assert(func);
	func->codegenYield(val, type, block);
}

Yield *Yield::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (Yield *)ref->getClone(this);

	auto *x = new Yield(expr ? expr->clone(ref) : nullptr);
	ref->addClone(this, x);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

Break::Break() :
    Stmt("break")
{
}

void Break::codegen0(BasicBlock*& block)
{
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);
	BranchInst *inst = builder.CreateBr(block);  // destination will be fixed by `setBreaks`
	addBreakToEnclosingLoop(inst);
	block = BasicBlock::Create(context, "", block->getParent());
}

Break *Break::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (Break *)ref->getClone(this);

	auto *x = new Break();
	ref->addClone(this, x);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

Continue::Continue() :
    Stmt("continue")
{
}

void Continue::codegen0(BasicBlock*& block)
{
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);
	BranchInst *inst = builder.CreateBr(block);  // destination will be fixed by `setContinues`
	addContinueToEnclosingLoop(inst);
	block = BasicBlock::Create(context, "", block->getParent());
}

Continue *Continue::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (Continue *)ref->getClone(this);

	auto *x = new Continue();
	ref->addClone(this, x);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}

Assert::Assert(Expr *expr) :
    Stmt("assert"), expr(expr)
{
}

void Assert::resolveTypes()
{
	expr->resolveTypes();
}

void Assert::codegen0(BasicBlock*& block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	auto *func = cast<Function>(
	               module->getOrInsertFunction(
	                 "seq_assert",
	                 Type::getVoidTy(context),
	                 types::Bool->getLLVMType(context),
	                 types::Int->getLLVMType(context),
	                 types::Str->getLLVMType(context)));

	Value *check = expr->codegen(getBase(), block);
	check = expr->getType()->boolValue(check, block, getTryCatch());
	Value *file = StrExpr(getSrcInfo().file).codegen(getBase(), block);
	Value *line = IntExpr(getSrcInfo().line).codegen(getBase(), block);

	IRBuilder<> builder(block);
	builder.CreateCall(func, {check, line, file});
}

Assert *Assert::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (Assert *)ref->getClone(this);

	auto *x = new Assert(expr->clone(ref));
	ref->addClone(this, x);
	Stmt::setCloneBase(x, ref);
	SEQ_RETURN_CLONE(x);
}
