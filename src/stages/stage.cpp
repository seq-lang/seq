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

Nop::Nop() : Stage("nop", types::AnyType::get(), types::VoidType::get())
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

	block = prev->getAfter();
	outs->insert(prev->outs->begin(), prev->outs->end());
	codegenNext(module);
	prev->setAfter(getAfter());
}

Nop& Nop::make()
{
	return *new Nop();
}
