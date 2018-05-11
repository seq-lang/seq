#include "seq/seq.h"
#include "seq/strexpr.h"

using namespace seq;
using namespace llvm;

StrExpr::StrExpr(std::string s) : Expr(types::StrType::get()), s(std::move(s))
{
}

Value *StrExpr::codegen(BaseFunc *base, BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();
	BasicBlock *preambleBlock = base->getPreamble();

	GlobalVariable *strVar = new GlobalVariable(*module,
	                                            llvm::ArrayType::get(IntegerType::getInt8Ty(context),
	                                                                 s.length() + 1),
	                                            true,
	                                            GlobalValue::PrivateLinkage,
	                                            ConstantDataArray::getString(context, s),
	                                            "str_literal");
	strVar->setAlignment(1);

	IRBuilder<> builder(preambleBlock);
	Value *strVal = builder.CreateGEP(strVar, zeroLLVM(context));
	Value *lenVal = ConstantInt::get(seqIntLLVM(context), s.length());

	Value *val = UndefValue::get(getType()->getLLVMType(context));
	val = builder.CreateInsertValue(val, lenVal, 0);
	val = builder.CreateInsertValue(val, strVal, 1);
	return val;
}
