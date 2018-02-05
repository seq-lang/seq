#include <string>
#include <vector>
#include "../seq.h"
#include "../exc.h"
#include "stage.h"

using namespace seq;
using namespace llvm;

Stage::Stage(std::string name, types::Type in, types::Type out) :
    linked(false), in(in), out(out), prev(nullptr),
    next(nullptr), name(std::move(name)), block(nullptr),
    outs(new std::map<SeqData, llvm::Value *>)
{
}

Stage::Stage(std::string name) :
    Stage::Stage(std::move(name), types::Void(), types::Void())
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

Stage *Stage::getNext() const
{
	return next;
}

void Stage::setBase(Seq *base)
{
	this->base = base;
	Stage *next;
	if ((next = getNext()))
	{
		next->setBase(base);
	}
}

Seq *Stage::getBase() const
{
	return base;
}

types::Type Stage::getInType() const
{
	return in;
}

types::Type Stage::getOutType() const
{
	return out;
}

Pipeline& Stage::asPipeline()
{
	return *new Pipeline(this, this);
}

void Stage::validate()
{
	if (prev && typeid(prev->out) != typeid(in))
		throw exc::ValidationException::ValidationException(*this);
}

void Stage::codegen(Module *module, LLVMContext& context)
{
	throw exc::StageException("cannot codegen abstract stage", *this);
}

void Stage::finalize(ExecutionEngine *eng)
{
	if (next)
		next->finalize(eng);
}
