#include <string>
#include <vector>
#include "func.h"
#include "exc.h"
#include "filter.h"

using namespace seq;
using namespace llvm;

Filter::Filter(Func& func) :
    Stage("filter", types::VoidType::get(), types::VoidType::get()), func(func)
{
	if (!func.getOutType()->isChildOf(types::BoolType::get()))
		throw exc::SeqException("filter function must return boolean");
}

void Filter::validate()
{
	if (prev)
		in = out = prev->getOutType();

	Stage::validate();
}

void Filter::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	block = prev->block;
	outs->insert(prev->outs->begin(), prev->outs->end());

	IRBuilder<> builder(block);

	ValMap result = std::make_shared<std::map<SeqData, Value *>>(*new std::map<SeqData, Value *>());
	func.codegenCall(getBase(), outs, result, block);
	Value *pred = builder.CreateLoad(getSafe(result, SeqData::BOOL));

	BasicBlock *body = BasicBlock::Create(context, "body", block->getParent());
	block = body;

	codegenNext(module);

	BasicBlock *exit = BasicBlock::Create(context, "exit", body->getParent());
	builder.CreateCondBr(pred, body, exit);
	builder.SetInsertPoint(getAfter());
	builder.CreateBr(exit);
	prev->setAfter(exit);
}

void Filter::finalize(ExecutionEngine *eng)
{
	func.finalize(eng);
	Stage::finalize(eng);
}

Filter& Filter::make(Func& func)
{
	return *new Filter(func);
}
