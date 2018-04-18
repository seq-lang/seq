#include <vector>
#include "seq/var.h"
#include "seq/record.h"
#include "seq/makerec.h"

using namespace seq;
using namespace llvm;

MakeRec::MakeRec(seq::PipelineList &pl) :
    Stage("makerec", types::VoidType::get(), types::VoidType::get()),
    validated(false), pl(pl), proxy(types::VoidType::get(), types::VoidType::get())
{
}

void MakeRec::validate()
{
	if (validated)
		return;

	if (prev) {
		for (auto *n = pl.head; n; n = n->next) {
			if (!n->isVar) {
				n->p.getHead()->setPrev(&proxy);
				n->p.getHead()->setBase(getBase());
				n->p.setAdded();
			}
		}
	}

	in  = prev->getOutType();
	proxy.setInOut(prev->getOutType(), prev->getOutType());
	std::vector<types::Type *> outTypes;

	for (auto *n = pl.head; n; n = n->next) {
		if (!n->isVar)
			n->p.validate();

		types::Type *type = n->isVar ? n->v->getType(this) :
		                               n->p.getTail()->getOutType();

		if (type->is(types::VoidType::get()))
			throw exc::StageException("cannot output void in record expression", *this);

		outTypes.push_back(type);
	}

	out = types::RecordType::get(outTypes);
	Stage::validate();
	validated = true;
}

void MakeRec::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	block = proxy.block = prev->block;
	proxy.outs = prev->outs;
	IRBuilder<> builder(block);

	Value *rec = UndefValue::get(out->getLLVMType(context));
	unsigned idx = 0;

	for (auto *n = pl.head; n; n = n->next) {
		Value *val;

		if (n->isVar) {
			Var *var = n->v;
			val = var->getType(this)->pack(getBase(), var->outs(this), proxy.getAfter());
		} else {
			Pipeline pipeline = n->p;
			pipeline.getHead()->codegen(module);
			types::Type *outType = pipeline.getTail()->getOutType();
			val = outType->pack(getBase(), pipeline.getTail()->outs, proxy.getAfter());
			setAfter(pipeline.getHead()->getAfter());
		}

		proxy.block = proxy.getAfter();
		builder.SetInsertPoint(proxy.block);
		rec = builder.CreateInsertValue(rec, val, {idx++});
	}

	block = proxy.block;
	setAfter(proxy.getAfter());
	out->unpack(getBase(), rec, outs, getAfter());
	codegenNext(module);

	prev->setAfter(getAfter());
}

void MakeRec::finalize(Module *module, ExecutionEngine *eng)
{
	for (auto *n = pl.head; n; n = n->next) {
		if (!n->isVar)
			n->p.getHead()->finalize(module, eng);
	}

	Stage::finalize(module, eng);
}

MakeRec& MakeRec::make(PipelineList& pl)
{
	return *new MakeRec(pl);
}
