#include <string>
#include <vector>
#include "seq/seq.h"
#include "seq/exc.h"

using namespace seq;
using namespace llvm;

Block::Block(Stage *parent) :
    parent(parent), stmts()
{
}

void Block::add(Stage *stmt)
{
	stmts.push_back(stmt);
	stmt->setParent(this);
}

void Block::codegen(BasicBlock*& block)
{
	for (auto *stmt : stmts)
		stmt->codegen(block);
}

Block *Block::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Block *)ref->getClone(this);

	auto *x = new Block(parent ? parent->clone(ref) : nullptr);
	ref->addClone(this, x);

	std::vector<Stage *> stmtsCloned;

	for (auto *stmt : stmts)
		stmtsCloned.push_back(stmt->clone(ref));

	x->stmts = stmtsCloned;
	return x;
}

Stage::Stage(std::string name) :
    base(nullptr), breaks(), continues(),
    parent(nullptr), loop(false), name(std::move(name))
{
}

std::string Stage::getName() const
{
	return name;
}

Stage *Stage::getPrev() const
{
	if (!parent)
		return nullptr;

	return parent->parent;
}

void Stage::setParent(Block *parent)
{
	assert(!this->parent);
	this->parent = parent;
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

void Stage::codegen(BasicBlock*& block)
{
	throw exc::StageException("cannot codegen abstract stage", *this);
}

void Stage::setCloneBase(Stage *stage, types::RefType *ref)
{
	if (base) stage->base = base->clone(ref);
	if (parent) stage->parent = parent->clone(ref);
	stage->loop = loop;
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
