#include "seq/record.h"
#include "seq/func.h"
#include "seq/call.h"

using namespace seq;
using namespace llvm;

static types::Type *voidToAny(types::Type *type)
{
	if (type->is(types::VoidType::get()))
		return types::AnyType::get();
	return type;
}

Call::Call(Func& func) :
    Stage("call", voidToAny(func.getInType()), func.getOutType()), func(func)
{
}

void Call::codegen(Module *module)
{
	ensurePrev();
	validate();

	block = prev->getAfter();
	func.codegenCall(getBase(), prev->outs, outs, block);
	codegenNext(module);
	prev->setAfter(getAfter());
}

void Call::finalize(Module *module, ExecutionEngine *eng)
{
	func.finalize(module, eng);
	Stage::finalize(module, eng);
}

Call& Call::make(Func& func)
{
	return *new Call(func);
}

MultiCall::MultiCall(std::vector<Func *> funcs) :
    Stage("call", types::VoidType::get(), types::VoidType::get()), funcs(std::move(funcs))
{
	if (this->funcs.empty())
		throw exc::StageException("unexpected empty function vector", *this);

	std::vector<types::Type *> outTypes;
	in = this->funcs[0]->getInType();

	for (auto *func : this->funcs) {
		if (!func->getInType()->is(in))
			throw exc::StageException("inconsistent function input types", *this);

		if (func->getOutType()->is(types::VoidType::get()))
			throw exc::StageException("function cannot have void output type in multi-call", *this);

		outTypes.push_back(func->getOutType());
	}

	in = voidToAny(in);
	out = types::RecordType::get(outTypes);
}

void MultiCall::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();

	block = prev->getAfter();
	IRBuilder<> builder(block);

	Value *rec = UndefValue::get(out->getLLVMType(context));
	unsigned idx = 0;

	for (auto *func : funcs) {
		rec = builder.CreateInsertValue(rec, func->codegenCallRaw(getBase(), prev->outs, block), idx++);
	}

	out->unpack(getBase(), rec, outs, block);

	codegenNext(module);
	prev->setAfter(getAfter());
}

void MultiCall::finalize(Module *module, ExecutionEngine *eng)
{
	for (auto *func : funcs)
		func->finalize(module, eng);
	Stage::finalize(module, eng);
}

MultiCall& MultiCall::make(std::vector<Func *> funcs)
{
	return *new MultiCall(std::move(funcs));
}
