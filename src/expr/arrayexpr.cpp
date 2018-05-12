#include "seq/seq.h"
#include "seq/arrayexpr.h"

using namespace seq;
using namespace llvm;

ArrayExpr::ArrayExpr(types::Type *type, Expr *count) :
    Expr(types::ArrayType::get(type)), count(count)
{
}

Value *ArrayExpr::codegen(BaseFunc *base, BasicBlock *block)
{
	auto *type = dynamic_cast<types::ArrayType *>(getType());
	assert(type != nullptr);
	count->ensure(types::IntType::get());

	Module *module = block->getModule();
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);

	GlobalVariable *ptrVar = new GlobalVariable(*module,
	                                            PointerType::get(type->getBaseType()->getLLVMType(context), 0),
	                                            false,
	                                            GlobalValue::PrivateLinkage,
	                                            nullptr,
	                                            "mem");
	ptrVar->setInitializer(
	  ConstantPointerNull::get(PointerType::get(type->getBaseType()->getLLVMType(context), 0)));

	GlobalVariable *lenVar = new GlobalVariable(*module,
	                                            seqIntLLVM(context),
	                                            false,
	                                            GlobalValue::PrivateLinkage,
	                                            nullptr,
	                                            "len");
	lenVar->setInitializer(zeroLLVM(context));

	Value *len = count->codegen(base, block);
	Value *ptr = type->getBaseType()->codegenAlloc(base, len, block);
	builder.CreateStore(ptr, ptrVar);
	builder.CreateStore(len, lenVar);

	auto outs = makeValMap();
	outs->insert({SeqData::ARRAY, ptrVar});
	outs->insert({SeqData::LEN, lenVar});
	return type->pack(base, outs, block);
}
