#include <string>
#include <vector>
#include "seq/func.h"
#include "seq/exc.h"
#include "seq/filter.h"

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
	block = prev->getAfter();
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

void Filter::finalize(Module *module, ExecutionEngine *eng)
{
	func.finalize(module, eng);
	Stage::finalize(module, eng);
}

Filter& Filter::make(Func& func)
{
	return *new Filter(func);
}
