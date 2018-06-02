#include <string>
#include <vector>
#include "seq/func.h"
#include "seq/varexpr.h"
#include "seq/exc.h"
#include "seq/filter.h"

using namespace seq;
using namespace llvm;

Filter::Filter(Expr *key) : Stage("filter"), key(key)
{
}

Filter::Filter(Func *key) : Filter(new FuncExpr(key))
{
}

void Filter::validate()
{
	if (prev)
		in = out = prev->getOutType();

	if (!key->getType()->getCallType(in)->isChildOf(types::BoolType::get()))
		throw exc::SeqException("filter key must return boolean");

	Stage::validate();
}

void Filter::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	block = prev->getAfter();
	result = prev->result;

	Value *f = key->codegen(getBase(), block);
	IRBuilder<> builder(block);
	Value *x = builder.CreateLoad(result);
	Value *pred = key->getType()->call(getBase(), f, x, block);

	BasicBlock *body = BasicBlock::Create(context, "body", block->getParent());
	block = body;

	codegenNext(module);

	BasicBlock *exit = BasicBlock::Create(context, "exit", body->getParent());
	builder.CreateCondBr(pred, body, exit);
	builder.SetInsertPoint(getAfter());
	builder.CreateBr(exit);
	prev->setAfter(exit);
}

Filter& Filter::make(Expr *key)
{
	return *new Filter(key);
}

Filter& Filter::make(Func& key)
{
	return *new Filter(&key);
}
