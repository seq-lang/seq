#include "seq/serialize.h"

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
	prev->getOutType()->callSerialize(getBase(), prev->outs, block, filename);
	codegenNext(module);
	prev->setAfter(getAfter());
}

void Serialize::finalize(Module *module, ExecutionEngine *eng)
{
	assert(prev);
	prev->getOutType()->finalizeSerialize(module, eng);
	Stage::finalize(module, eng);
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
	type->callDeserialize(getBase(), outs, block, filename);
	codegenNext(module);
	prev->setAfter(getAfter());
}

void Deserialize::finalize(Module *module, ExecutionEngine *eng)
{
	assert(prev);
	type->finalizeDeserialize(module, eng);
	Stage::finalize(module, eng);
}

Deserialize& Deserialize::make(types::Type *type, std::string filename)
{
	return *new Deserialize(type, std::move(filename));
}
