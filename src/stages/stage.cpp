#include <string>
#include <vector>
#include "seq/seq.h"
#include "seq/pipeline.h"
#include "seq/exc.h"

using namespace seq;
using namespace llvm;

Stage::Stage(std::string name, types::Type *in, types::Type *out) :
    base(nullptr), added(false), in(in), out(out),
    prev(nullptr), nexts(), weakNexts(), name(std::move(name)),
    block(nullptr), after(nullptr), outs(new std::map<SeqData, Value *>)
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
		auto *base = dynamic_cast<InitStage *>(stage);

		if (base)
			return base->getInitBlock();

		stage = stage->getPrev();
	}

	throw exc::SeqException("no enclosing init block found");
}

static LoopStage *findEnclosingLoop(Stage *stage)
{
	while (stage) {
		auto *loop = dynamic_cast<LoopStage *>(stage);

		if (loop)
			return loop;

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

Pipeline Stage::operator|(Var& to)
{
	return (Pipeline)*this | to;
}

Pipeline Stage::operator&(PipelineList& to)
{
	return (Pipeline)*this & to;
}

Stage::operator Pipeline()
{
	return {this, this};
}

std::ostream& operator<<(std::ostream& os, Stage& stage)
{
	return os << stage.getName();
}

InitStage::InitStage(std::string name, types::Type *in, types::Type *out) :
    Stage(std::move(name), in, out), init(nullptr), start(nullptr)
{
}

InitStage::InitStage(std::string name) :
    InitStage(std::move(name), types::VoidType::get(), types::VoidType::get())
{
}

void InitStage::codegenInit(llvm::BasicBlock*& block)
{
	LLVMContext& context = getBase()->getContext();
	Function *func = getBase()->getFunc();

	assert(block);
	init = BasicBlock::Create(context, "init", func);

	IRBuilder<> builder(block);
	builder.CreateBr(init);

	start = block = BasicBlock::Create(context, "start", func);
}

void InitStage::finalizeInit()
{
	assert(start);
	IRBuilder<> builder(init);
	builder.CreateBr(start);
}

BasicBlock *InitStage::getInitBlock()
{
	if (!init)
		throw exc::SeqException("cannot get base stage init block before code generation");

	return init;
}

LoopStage::LoopStage(std::string name, types::Type *in, types::Type *out) :
    Stage(std::move(name), in, out), breaks(), continues()
{
}

LoopStage::LoopStage(std::string name) :
    LoopStage(std::move(name), types::VoidType::get(), types::VoidType::get())
{
}

void LoopStage::addBreak(BranchInst *inst)
{
	breaks.push_back(inst);
}

void LoopStage::addContinue(BranchInst *inst)
{
	continues.push_back(inst);
}

void LoopStage::setBreaks(BasicBlock *block)
{
	for (auto *inst : breaks)
		inst->setSuccessor(0, block);
}

void LoopStage::setContinues(BasicBlock *block)
{
	for (auto *inst : continues)
		inst->setSuccessor(0, block);
}

Nop::Nop() : InitStage("nop", types::AnyType::get(), types::VoidType::get())
{
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

	outs->insert(prev->outs->begin(), prev->outs->end());

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
