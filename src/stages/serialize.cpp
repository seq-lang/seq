#include "serialize.h"

using namespace seq;
using namespace llvm;

Serialize::Serialize(std::string filename) :
    Stage("ser", types::AnyType::get(), types::VoidType::get()), filename(filename)
{
	name += "('" + filename + "')";
}

void Serialize::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->block;
	prev->getOutType()->callSerialize(prev->outs, block, filename);
	codegenNext(module);
	prev->setAfter(getAfter());
}

void Serialize::finalize(ExecutionEngine *eng)
{
	assert(prev);
	prev->getOutType()->finalizeSerialize(eng);
}

Serialize& Serialize::make(std::string filename)
{
	return *new Serialize(std::move(filename));
}

Deserialize::Deserialize(types::Type *type, std::string filename) :
    Stage("deser", types::AnyType::get(), type), type(type), filename(filename)
{
	name += "('" + filename + "')";

	if (type->getKey() == SeqData::NONE)
		throw exc::SeqException("cannot deserialize type '" + type->getName() + "'");
}

void Deserialize::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->block;
	type->callDeserialize(outs, block, filename);
	codegenNext(module);
	prev->setAfter(getAfter());
}

void Deserialize::finalize(ExecutionEngine *eng)
{
	assert(prev);
	type->finalizeDeserialize(eng);
}

Deserialize& Deserialize::make(types::Type *type, std::string filename)
{
	return *new Deserialize(type, std::move(filename));
}
