#include <string>
#include <vector>
#include "seq.h"
#include "pipeline.h"
#include "exc.h"

using namespace seq;
using namespace llvm;

Stage::Stage(std::string name, types::Type *in, types::Type *out) :
    linked(false), in(in), out(out), prev(nullptr), nexts(),
    name(std::move(name)), block(nullptr), after(nullptr),
    outs(new std::map<SeqData, llvm::Value *>)
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
	if (this->prev != nullptr)
		throw exc::MultiLinkException(*this);

	this->prev = prev;
}

std::vector<Stage *>& Stage::getNext()
{
	return nexts;
}

void Stage::setBase(Seq *base)
{
	this->base = base;

	for (auto& next : nexts) {
		next->setBase(base);
	}
}

Seq *Stage::getBase() const
{
	return base;
}

types::Type *Stage::getInType() const
{
	return in;
}

types::Type *Stage::getOutType() const
{
	return out;
}

Pipeline& Stage::asPipeline()
{
	return *new Pipeline(this, this);
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

bool Stage::isLinked() const
{
	return linked;
}

void Stage::setLinked()
{
	linked = true;
}

void Stage::validate()
{
	if (prev && !prev->getOutType()->isChildOf(in))
		throw exc::ValidationException::ValidationException(*this);
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

Pipeline& Stage::operator|(Stage& to)
{
	return *new Pipeline(this, &to);
}

Pipeline& Stage::operator|(Pipeline& to)
{
	to.setHead(this);
	return to;
}

std::ostream& operator<<(std::ostream& os, Stage& stage)
{
	return os << stage.getName();
}
