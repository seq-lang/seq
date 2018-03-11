#include <string>
#include <vector>
#include "seq.h"
#include "pipeline.h"
#include "exc.h"

using namespace seq;
using namespace llvm;

Stage::Stage(std::string name, types::Type *in, types::Type *out) :
    in(in), out(out), prev(nullptr), nexts(), name(std::move(name)),
    block(nullptr), after(nullptr), outs(new std::map<SeqData, Value *>)
{
}

Stage::Stage(std::string name) :
    Stage::Stage(std::move(name), types::Void::get(), types::Void::get())
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

Seq *Stage::getBase() const
{
	return base;
}

void Stage::setBase(Seq *base)
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

void Stage::addNext(Stage *next)
{
	nexts.push_back(next);
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
	if (prev && !prev->getOutType()->isChildOf(in))
		throw exc::ValidationException(*this);
}

void Stage::ensurePrev()
{
	if (!prev || !prev->block)
		throw exc::StageException("previous stage not compiled", *this);
}

void Stage::codegen(Module *module, LLVMContext& context)
{
	throw exc::StageException("cannot codegen abstract stage", *this);
}

void Stage::codegenNext(Module *module, LLVMContext& context)
{
	for (auto& next : nexts) {
		next->codegen(module, context);
	}
}

void Stage::finalize(ExecutionEngine *eng)
{
	for (auto& next : nexts) {
		next->finalize(eng);
	}
}

Pipeline Stage::operator|(Pipeline to)
{
	to.getHead()->setBase(getBase());
	addNext(to.getHead());
	to.getHead()->setPrev(this);
	return {this, to.getTail()};
}

Stage::operator Pipeline()
{
	return {this, this};
}

std::ostream& operator<<(std::ostream& os, Stage& stage)
{
	return os << stage.getName();
}
