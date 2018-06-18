#include "seq/record.h"
#include "seq/getitem.h"

using namespace seq;
using namespace llvm;

GetItem::GetItem(seq_int_t idx) : Stage("get"), idx(idx)
{

}

void GetItem::validate()
{
	if (prev) {
		types::RecordType *recType = nullptr;

		if ((recType = dynamic_cast<types::RecordType *>(prev->getOutType()))) {
			in = recType;
			out = recType->getBaseType(idx);
		}
	}

	Stage::validate();
}

void GetItem::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();
	block = prev->getAfter();
	IRBuilder<> builder(block);
	Value *rec = builder.CreateLoad(prev->result);
	Value *idx = ConstantInt::get(seqIntLLVM(context), (uint64_t)this->idx);

	Value *val = getInType()->indexLoad(getBase(), rec, idx, block);
	result = getInType()->getBaseType(this->idx)->storeInAlloca(getBase(), val, block, true);

	codegenNext(module);
	prev->setAfter(getAfter());
}

GetItem& GetItem::make(seq_int_t idx)
{
	return *new GetItem(idx);
}

GetItem *GetItem::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (GetItem *)ref->getClone(this);

	GetItem& x = GetItem::make(idx);
	ref->addClone(this, &x);
	Stage::setCloneBase(&x, ref);
	return &x;
}
