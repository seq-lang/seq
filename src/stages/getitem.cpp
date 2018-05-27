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
	Value *recVal = builder.CreateLoad(getSafe(prev->outs, SeqData::RECORD));
	Value *idxVal = ConstantInt::get(seqIntLLVM(context), (uint64_t)idx);

	getInType()->codegenIndexLoad(getBase(),
	                              outs,
	                              block,
	                              recVal,
	                              idxVal);

	codegenNext(module);
	prev->setAfter(getAfter());
}

GetItem& GetItem::make(seq_int_t idx)
{
	return *new GetItem(idx);
}
