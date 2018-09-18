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
	types::Type *type = expr->getType();
	Value *val = expr->codegen(getBase(), block);
	type->print(getBase(), val, block);
}

Print *Print::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (Print *)ref->getClone(this);

	auto *x = new Print(expr->clone(ref));
	ref->addClone(this, x);
	Stmt::setCloneBase(x, ref);
	return x;
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
	return x;
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
	return x;
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
	return x;
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
	return x;
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
	this->idx->ensure(types::Int);

	if (!array->getType()->isGeneric(types::Array))
		throw exc::SeqException("can only assign indices of array type");

	auto *arrType = dynamic_cast<types::ArrayType *>(array->getType());
	assert(arrType != nullptr);
	value->ensure(arrType->indexType());

	Value *val = value->codegen(getBase(), block);
	Value *arr = array->codegen(getBase(), block);
	Value *idx = this->idx->codegen(getBase(), block);
	array->getType()->indexStore(getBase(), arr, idx, val, block);
}

AssignIndex *AssignIndex::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (AssignIndex *)ref->getClone(this);

	auto *x = new AssignIndex(array->clone(ref), idx->clone(ref), value->clone(ref));
	ref->addClone(this, x);
	Stmt::setCloneBase(x, ref);
	return x;
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
	return x;
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

	for (auto *cond : conds)
		cond->ensure(types::BoolType::get());

	LLVMContext& context = block->getContext();
	Function *func = block->getParent();
	IRBuilder<> builder(block);

	std::vector<BranchInst *> binsts;

	for (unsigned i = 0; i < conds.size(); i++) {
		Value *cond = conds[i]->codegen(getBase(), block);
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
	return x;
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
	return x;
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
	cond->ensure(types::BoolType::get());
	LLVMContext& context = block->getContext();

	BasicBlock *entry = block;
	Function *func = entry->getParent();

	IRBuilder<> builder(entry);

	BasicBlock *loop0 = BasicBlock::Create(context, "while", func);
	BasicBlock *loop = loop0;
	builder.CreateBr(loop);

	Value *cond = this->cond->codegen(getBase(), loop);  // recall: this can change `loop`
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
	return x;
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

	if (!gen->getType()->isGeneric(types::Gen))
		throw exc::SeqException("cannot iterate over non-generator");

	var->setType(gen->getType()->getBaseType(0));
	scope->resolveTypes();
}

void For::codegen0(BasicBlock*& block)
{
	auto *type = dynamic_cast<types::GenType *>(gen->getType());
	assert(type);

	LLVMContext& context = block->getContext();

	BasicBlock *entry = block;
	Function *func = entry->getParent();

	Value *gen = this->gen->codegen(getBase(), entry);
	IRBuilder<> builder(entry);

	BasicBlock *loopCont = BasicBlock::Create(context, "for_cont", func);
	BasicBlock *loop = BasicBlock::Create(context, "for", func);
	builder.CreateBr(loop);

	builder.SetInsertPoint(loopCont);
	builder.CreateBr(loop);

	builder.SetInsertPoint(loop);
	type->resume(gen, loop);
	Value *cond = type->done(gen, loop);
	BasicBlock *body = BasicBlock::Create(context, "body", func);
	BranchInst *branch = builder.CreateCondBr(cond, body, body);  // we set true-branch below

	block = body;
	if (!type->getBaseType(0)->is(types::Void)) {
		Value *val = type->promise(gen, block);
		var->store(getBase(), val, block);
	}

	scope->codegen(block);

	builder.SetInsertPoint(block);
	builder.CreateBr(loop);

	BasicBlock *cleanup = BasicBlock::Create(context, "cleanup", func);
	branch->setSuccessor(0, cleanup);
	type->destroy(gen, cleanup);

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
	return x;
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
	return x;
}

Yield::Yield(Expr *expr) :
    Stmt("Yield"), expr(expr)
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
	return x;
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
	return x;
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
	return x;
}
