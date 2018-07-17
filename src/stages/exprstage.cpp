#include "seq/seq.h"
#include "seq/exprstage.h"

using namespace seq;
using namespace llvm;

ExprStage::ExprStage(Expr *expr) :
    Stage("expr", types::AnyType::get(), types::VoidType::get()), expr(expr)
{
}

void ExprStage::validate()
{
	out = expr->getType();
	Stage::validate();
}

void ExprStage::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->getAfter();
	Value *val = expr->codegen(getBase(), block);
	result = getOutType()->storeInAlloca(getBase(), val, block);
	codegenNext(module);
	prev->setAfter(getAfter());
}

ExprStage& ExprStage::make(Expr *expr)
{
	return *new ExprStage(expr);
}

ExprStage *ExprStage::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (ExprStage *)ref->getClone(this);

	ExprStage& x = ExprStage::make(expr->clone(ref));
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}

CellStage::CellStage(Cell *cell) :
    Stage("cell", types::AnyType::get(), types::VoidType::get()), cell(cell)
{
}

void CellStage::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->getAfter();
	cell->codegen(block);
	codegenNext(module);
	prev->setAfter(getAfter());
}

CellStage& CellStage::make(Cell *cell)
{
	return *new CellStage(cell);
}

CellStage *CellStage::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (CellStage *)ref->getClone(this);

	CellStage& x = CellStage::make(cell->clone(ref));
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}

AssignStage::AssignStage(Cell *cell, Expr *value) :
    Stage("(=)", types::AnyType::get(), types::VoidType::get()), cell(cell), value(value)
{
}

void AssignStage::codegen(Module *module)
{
	value->ensure(cell->getType());

	ensurePrev();
	validate();

	block = prev->getAfter();
	Value *val = value->codegen(getBase(), block);
	cell->store(val, block);
	codegenNext(module);
	prev->setAfter(getAfter());
}

AssignStage& AssignStage::make(Cell *cell, Expr *value)
{
	return *new AssignStage(cell, value);
}

AssignStage *AssignStage::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (AssignStage *)ref->getClone(this);

	AssignStage& x = AssignStage::make(cell->clone(ref), value->clone(ref));
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}

AssignIndexStage::AssignIndexStage(Expr *array, Expr *idx, Expr *value) :
    Stage("([]=)", types::AnyType::get(), types::VoidType::get()),
    array(array), idx(idx), value(value)
{
}

void AssignIndexStage::codegen(Module *module)
{
	this->idx->ensure(types::IntType::get());

	if (!array->getType()->isGeneric(types::ArrayType::get()))
		throw exc::SeqException("can only assign indices of array type");

	auto *arrType = dynamic_cast<types::ArrayType *>(array->getType());
	assert(arrType != nullptr);
	value->ensure(arrType->indexType());

	ensurePrev();
	validate();

	block = prev->getAfter();
	Value *val = value->codegen(getBase(), block);
	Value *arr = array->codegen(getBase(), block);
	Value *idx = this->idx->codegen(getBase(), block);
	array->getType()->indexStore(getBase(), arr, idx, val, block);

	codegenNext(module);
	prev->setAfter(getAfter());
}

AssignIndexStage& AssignIndexStage::make(Expr *array, Expr *idx, Expr *value)
{
	return *new AssignIndexStage(array, idx, value);
}

AssignIndexStage *AssignIndexStage::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (AssignIndexStage *)ref->getClone(this);

	AssignIndexStage& x = AssignIndexStage::make(array->clone(ref), idx->clone(ref), value->clone(ref));
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}

AssignMemberStage::AssignMemberStage(Expr *expr, std::string memb, Expr *value) :
    Stage("(.=)", types::AnyType::get(), types::VoidType::get()),
    expr(expr), memb(std::move(memb)), value(value)
{
}

AssignMemberStage::AssignMemberStage(Expr *expr, seq_int_t idx, Expr *value) :
    AssignMemberStage(expr, std::to_string(idx), value)
{
}

void AssignMemberStage::codegen(Module *module)
{
	value->ensure(expr->getType()->membType(memb));

	ensurePrev();
	validate();

	block = prev->getAfter();
	Value *x = expr->codegen(getBase(), block);
	Value *v = value->codegen(getBase(), block);
	expr->getType()->setMemb(x, memb, v, block);
	prev->setAfter(getAfter());
}

AssignMemberStage& AssignMemberStage::make(Expr *expr, std::string memb, Expr *value)
{
	return *new AssignMemberStage(expr, std::move(memb), value);
}

AssignMemberStage& AssignMemberStage::make(Expr *expr, seq_int_t idx, Expr *value)
{
	return *new AssignMemberStage(expr, idx, value);
}

AssignMemberStage *AssignMemberStage::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (AssignMemberStage *)ref->getClone(this);

	AssignMemberStage& x = AssignMemberStage::make(expr->clone(ref), memb, value->clone(ref));
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}

If::If() :
    Stage("if", types::AnyType::get(), types::VoidType::get()), conds(), branches(), elseAdded(false)
{
}

void If::codegen(Module *module)
{
	if (conds.empty())
		throw exc::SeqException("no conditions added to if-stage");

	assert(conds.size() == branches.size());

	for (auto *cond : conds)
		cond->ensure(types::BoolType::get());

	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	block = prev->getAfter();
	Function *func = block->getParent();
	IRBuilder<> builder(block);

	std::vector<BranchInst *> binsts;

	for (unsigned i = 0; i < conds.size(); i++) {
		Value *cond = conds[i]->codegen(getBase(), block);
		BaseStage *branch = branches[i];
		branch->setInOut(types::VoidType::get(), prev->getOutType());
		branch->result = prev->result;

		builder.SetInsertPoint(block);  // recall: expr codegen can change the block
		cond = builder.CreateTrunc(cond, IntegerType::getInt1Ty(context));

		BasicBlock *b1 = BasicBlock::Create(context, "", func);
		BranchInst *binst1 = builder.CreateCondBr(cond, b1, b1);  // we set false-branch below

		branch->block = b1;
		branch->codegen(module);
		block = getAfter();
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

	codegenNext(module);
	prev->setAfter(after);
}

If& If::make()
{
	return *new If();
}

BaseStage& If::addCond(Expr *cond)
{
	if (elseAdded)
		throw exc::SeqException("cannot add else-if branch to if-stage after else branch");

	BaseStage& branch = BaseStage::make(types::AnyType::get(), types::VoidType::get(), false);
	branch.setBase(getBase());
	branch.setPrev(this);
	conds.push_back(cond);
	branches.push_back(&branch);
	return branch;
}

BaseStage& If::addElse()
{
	if (elseAdded)
		throw exc::SeqException("cannot add second else branch to if-stage");

	BaseStage& branch = addCond(new BoolExpr(true));
	elseAdded = true;
	return branch;
}

If *If::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (If *)ref->getClone(this);

	If& x = If::make();
	ref->addClone(this, &x);

	std::vector<Expr *> condsCloned;
	std::vector<BaseStage *> branchesCloned;

	for (auto *cond : conds)
		condsCloned.push_back(cond->clone(ref));

	for (auto *branch : branches)
		branchesCloned.push_back(branch->clone(ref));

	x.conds = condsCloned;
	x.branches = branchesCloned;
	x.elseAdded = elseAdded;

	Stage::setCloneBase(&x, ref);
	return &x;
}

Match::Match() :
    Stage("match", types::AnyType::get(), types::VoidType::get()),
    value(nullptr), patterns(), branches()
{
}

void Match::codegen(Module *module)
{
	if (patterns.empty())
		throw exc::SeqException("no patterns added to match-stage");

	assert(patterns.size() == branches.size() && value);

	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	block = prev->getAfter();
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
		BaseStage *branch = branches[i];
		branch->setInOut(types::VoidType::get(), prev->getOutType());
		branch->result = prev->result;

		builder.SetInsertPoint(block);  // recall: expr codegen can change the block

		BasicBlock *b1 = BasicBlock::Create(context, "", func);
		BranchInst *binst1 = builder.CreateCondBr(cond, b1, b1);  // we set false-branch below

		branch->block = b1;
		branch->codegen(module);
		block = getAfter();
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

	codegenNext(module);
	prev->setAfter(after);
}

Match& Match::make()
{
	return *new Match();
}

void Match::setValue(Expr *value)
{
	if (this->value)
		throw exc::SeqException("cannot re-set match stage's value");

	this->value = value;
}

BaseStage& Match::addCase(Pattern *pattern)
{
	BaseStage& branch = BaseStage::make(types::AnyType::get(), types::VoidType::get(), false);
	branch.setBase(getBase());
	branch.setPrev(this);
	patterns.push_back(pattern);
	branches.push_back(&branch);
	return branch;
}

Match *Match::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Match *)ref->getClone(this);

	Match& x = Match::make();
	ref->addClone(this, &x);

	std::vector<Pattern *> patternsCloned;
	std::vector<BaseStage *> branchesCloned;

	for (auto *pattern : patterns)
		patternsCloned.push_back(pattern->clone(ref));

	for (auto *branch : branches)
		branchesCloned.push_back(branch->clone(ref));

	if (value) x.value = value->clone(ref);
	x.patterns = patternsCloned;
	x.branches = branchesCloned;

	Stage::setCloneBase(&x, ref);
	return &x;
}

While::While(Expr *cond) :
    Stage("while", types::AnyType::get(), types::VoidType::get()), cond(cond)
{
	loop = true;
}

void While::codegen(Module *module)
{
	cond->ensure(types::BoolType::get());

	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();

	BasicBlock *entry = prev->getAfter();
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

	codegenNext(module);

	builder.SetInsertPoint(getAfter());
	builder.CreateBr(loop0);

	BasicBlock *exit = BasicBlock::Create(context, "exit", func);
	branch->setSuccessor(1, exit);
	prev->setAfter(exit);

	setBreaks(exit);
	setContinues(loop0);
}

While& While::make(Expr *cond)
{
	return *new While(cond);
}

While *While::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (While *)ref->getClone(this);

	While& x = While::make(cond->clone(ref));
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}

Return::Return(Expr *expr) :
    Stage("return", types::AnyType::get(), types::VoidType::get()), expr(expr)
{
}

void Return::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->getAfter();
	getBase()->codegenReturn(expr, block);
	prev->setAfter(getAfter());
}

Return& Return::make(Expr *expr)
{
	return *new Return(expr);
}

Return *Return::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Return *)ref->getClone(this);

	Return& x = Return::make(expr ? expr->clone(ref) : nullptr);
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}

Break::Break() :
    Stage("break", types::AnyType::get(), types::VoidType::get())
{
}

void Break::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	block = prev->getAfter();
	IRBuilder<> builder(block);
	BranchInst *inst = builder.CreateBr(block);  // destination will be fixed by Stage::setBreaks
	addBreakToEnclosingLoop(inst);
	block = BasicBlock::Create(context, "", block->getParent());
	prev->setAfter(getAfter());
}

Break& Break::make()
{
	return *new Break();
}

Break *Break::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Break *)ref->getClone(this);

	Break& x = Break::make();
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}

Continue::Continue() :
    Stage("continue", types::AnyType::get(), types::VoidType::get())
{
}

void Continue::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	block = prev->getAfter();
	IRBuilder<> builder(block);
	BranchInst *inst = builder.CreateBr(block);  // destination will be fixed by Stage::setContinues
	addContinueToEnclosingLoop(inst);
	block = BasicBlock::Create(context, "", block->getParent());
	prev->setAfter(getAfter());
}

Continue& Continue::make()
{
	return *new Continue();
}

Continue *Continue::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Continue *)ref->getClone(this);

	Continue& x = Continue::make();
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}
