#include <string>
#include <vector>
#include "seq/seq.h"
#include "seq/pipeline.h"
#include "seq/exc.h"

using namespace seq;
using namespace llvm;

Stage::Stage(std::string name, types::Type *in, types::Type *out) :
    base(nullptr), added(false), breaks(), continues(), in(in), out(out),
    prev(nullptr), nexts(), weakNexts(), loop(false), init(false), name(std::move(name)),
    block(nullptr), after(nullptr), result(nullptr), initBlock(nullptr), startBlock(nullptr)
{
}

Stage::Stage(std::string name) :
    Stage::Stage(std::move(name), types::VoidType::get(), types::VoidType::get())
{
}

std::string Stage::getName() const
{
	return name;
}

Stage *Stage::getPrev() const
{
	return prev;
}

void Stage::setPrev(Stage *prev)
{
	if (this->prev)
		throw exc::MultiLinkException(*this);

	this->prev = prev;
}

std::vector<Stage *>& Stage::getNext()
{
	return nexts;
}

std::vector<Stage *>& Stage::getWeakNext()
{
	return weakNexts;
}

BaseFunc *Stage::getBase() const
{
	return base;
}

void Stage::setBase(BaseFunc *base)
{
	if (!base)
		return;

	this->base = base;

	for (auto& next : nexts) {
		next->setBase(base);
	}
}

types::Type *Stage::getInType() const
{
	return in;
}

types::Type *Stage::getOutType() const
{
	return out;
}

void Stage::setInOut(types::Type *in, types::Type *out)
{
	this->in = in;
	this->out = out;
}

void Stage::addNext(Stage *next)
{
	nexts.push_back(next);
}

void Stage::addWeakNext(Stage *next)
{
	weakNexts.push_back(next);
}

BasicBlock *Stage::getAfter() const
{
	return after ? after : block;
}

void Stage::setAfter(BasicBlock *block)
{
	after = block;
}

bool Stage::isAdded() const
{
	return added;
}

void Stage::setAdded()
{
	added = true;

	for (auto& next : nexts) {
		next->setAdded();
	}
}

BasicBlock *Stage::getEnclosingInitBlock()
{
	Stage *stage = this;
	while (stage) {
		if (stage->isInit())
			return stage->getInitBlock();
		stage = stage->getPrev();
	}

	throw exc::SeqException("no enclosing init block found");
}

static Stage *findEnclosingLoop(Stage *stage)
{
	while (stage) {
		if (stage->isLoop())
			return stage;
		stage = stage->getPrev();
	}

	throw exc::SeqException("break or continue outside of loop");
}

void Stage::addBreakToEnclosingLoop(BranchInst *inst)
{
	findEnclosingLoop(this)->addBreak(inst);
}

void Stage::addContinueToEnclosingLoop(BranchInst *inst)
{
	findEnclosingLoop(this)->addContinue(inst);
}

bool Stage::isLoop()
{
	return loop;
}

void Stage::ensureLoop()
{
	if (!isLoop())
		throw exc::SeqException("stage '" + getName() + "' is not a loop stage");
}

void Stage::addBreak(BranchInst *inst)
{
	ensureLoop();
	breaks.push_back(inst);
}

void Stage::addContinue(BranchInst *inst)
{
	ensureLoop();
	continues.push_back(inst);
}

void Stage::setBreaks(BasicBlock *block)
{
	ensureLoop();
	for (auto *inst : breaks)
		inst->setSuccessor(0, block);
}

void Stage::setContinues(BasicBlock *block)
{
	ensureLoop();
	for (auto *inst : continues)
		inst->setSuccessor(0, block);
}

bool Stage::isInit()
{
	return init;
}

void Stage::ensureInit()
{
	if (!isInit())
		throw exc::SeqException("stage '" + getName() + "' is not an initialization stage");
}

void Stage::codegenInit(llvm::BasicBlock*& block)
{
	LLVMContext& context = getBase()->getContext();
	Function *func = getBase()->getFunc();

	assert(block);
	initBlock = BasicBlock::Create(context, "init", func);

	IRBuilder<> builder(block);
	builder.CreateBr(initBlock);

	startBlock = block = BasicBlock::Create(context, "start", func);
}

void Stage::finalizeInit()
{
	assert(startBlock);
	IRBuilder<> builder(initBlock);
	builder.CreateBr(startBlock);
}

BasicBlock *Stage::getInitBlock()
{
	if (!initBlock)
		throw exc::SeqException("cannot get base stage init block before code generation");

	return initBlock;
}

void Stage::validate()
{
	if ((prev && !prev->getOutType()->isChildOf(in)) || !getBase())
		throw exc::ValidationException(*this);
}

void Stage::ensurePrev()
{
	if (!prev || !prev->block)
		throw exc::StageException("previous stage not compiled", *this);
}

void Stage::codegen(Module *module)
{
	throw exc::StageException("cannot codegen abstract stage", *this);
}

void Stage::codegenNext(Module *module)
{
	for (auto& next : nexts) {
		next->codegen(module);
	}
}

void Stage::finalize(Module *module, ExecutionEngine *eng)
{
	for (auto& next : nexts) {
		next->finalize(module, eng);
	}
}

Pipeline Stage::operator|(Pipeline to)
{
	return (Pipeline)*this | to;
}

Stage::operator Pipeline()
{
	return {this, this};
}

void Stage::setCloneBase(Stage *stage, types::RefType *ref)
{
	stage->base = base->clone(ref);
	if (isAdded()) stage->setAdded();
	if (prev) stage->prev = prev->clone(ref);

	std::vector<Stage *> nextsCloned;
	std::vector<Stage *> weakNextsCloned;

	for (auto *next : nexts)
		nextsCloned.push_back(next->clone(ref));

	for (auto *next : weakNexts)
		weakNextsCloned.push_back(next->clone(ref));

	stage->nexts = nextsCloned;
	stage->weakNexts = weakNextsCloned;
	stage->name = name;
}

Stage *Stage::clone(types::RefType *ref)
{
	throw exc::SeqException("cannot clone stage '" + getName() + "'");
}

std::ostream& operator<<(std::ostream& os, Stage& stage)
{
	return os << stage.getName();
}

Nop::Nop() : Stage("nop", types::AnyType::get(), types::VoidType::get())
{
	init = true;
}

void Nop::validate()
{
	if (prev)
		out = prev->getOutType();

	Stage::validate();
}

void Nop::codegen(Module *module)
{
	ensurePrev();
	validate();

	result = prev->result;

	block = prev->getAfter();
	codegenInit(block);
	codegenNext(module);
	prev->setAfter(getAfter());
	finalizeInit();
}

Nop& Nop::make()
{
	return *new Nop();
}

Nop *Nop::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Nop *)ref->getClone(this);

	Nop& x = Nop::make();
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}
