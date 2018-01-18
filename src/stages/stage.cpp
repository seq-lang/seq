#include <string>
#include <vector>
#include "../seq.h"
#include "../exc.h"
#include "stage.h"

using namespace seq;
using namespace llvm;

Stage::Stage(std::string name, types::Type in, types::Type out) :
    linked(false), in(in), out(out), prev(nullptr), next(nullptr),
    name(std::move(name)), block(nullptr), outs()
{
}

Stage::Stage(std::string name) :
    Stage::Stage(std::move(name), types::Void(), types::Void())
{
}

std::string& Stage::getName()
{
	return name;
}

Stage *Stage::getPrev()
{
	return prev;
}

Stage *Stage::getNext()
{
	return next;
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
