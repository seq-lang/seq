#include <vector>
#include "seq/var.h"
#include "seq/record.h"
#include "seq/makerec.h"

using namespace seq;
using namespace llvm;

MakeRec::MakeRec(PipelineList &pl) :
    Stage("makerec"), validated(false), pl(pl)
{
}

void MakeRec::validate()
{
	if (validated)
		return;

	if (prev) {
		for (auto *n = pl.head; n; n = n->next) {
			if (!n->isVar) {
				n->p.getHead()->setPrev(prev);
				n->p.getHead()->setBase(getBase());
				n->p.setAdded();
			}
		}
	}

	in  = prev->getOutType();
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
	block = prev->getAfter();
	IRBuilder<> builder(block);

	Value *rec = UndefValue::get(out->getLLVMType(context));
	unsigned idx = 0;

	for (auto *n = pl.head; n; n = n->next) {
		Value *val;

		if (n->isVar) {
			Var *var = n->v;
			val = var->getType(this)->pack(getBase(), var->outs(this), prev->getAfter());
		} else {
			Pipeline pipeline = n->p;
			pipeline.getHead()->codegen(module);
			types::Type *outType = pipeline.getTail()->getOutType();
			val = outType->pack(getBase(), pipeline.getTail()->outs, prev->getAfter());
			setAfter(pipeline.getHead()->getAfter());
		}

		block = prev->getAfter();
		builder.SetInsertPoint(block);
		rec = builder.CreateInsertValue(rec, val, idx++);
	}

	setAfter(prev->getAfter());
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
