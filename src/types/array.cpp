#include "seq/seq.h"
#include "seq/any.h"
#include "seq/base.h"
#include "seq/exc.h"
#include "seq/array.h"

using namespace seq;
using namespace llvm;

SEQ_FUNC void *copyArray(void *arr, seq_int_t len, seq_int_t elem_size)
{
	const size_t size = (size_t)len * elem_size;
	auto *arr2 = std::malloc(size);
	std::memcpy(arr2, arr, size);
	return arr2;
}

types::ArrayType::ArrayType(Type *baseType) :
    Type(baseType->getName() + "Array", BaseType::get(), SeqData::ARRAY), baseType(baseType)
{
	vtable.copy = (void *)copyArray;
}

Function *types::ArrayType::makeFuncOf(Module *module, Type *outType)
{
	static int idx = 1;
	LLVMContext& context = module->getContext();

	return cast<Function>(
	         module->getOrInsertFunction(
	           getName() + "Func" + std::to_string(idx++),
	           outType->getLLVMType(context),
	           PointerType::get(getBaseType()->getLLVMType(context), 0),
	           seqIntLLVM(context)));
}

void types::ArrayType::setFuncArgs(Function *func,
                                   ValMap outs,
                                   BasicBlock *block)
{
	auto args = func->arg_begin();
	Value *ptr = args++;
	Value *len = args;
	Value *ptrVar = makeAlloca(ptr, block);
	Value *lenVar = makeAlloca(len, block);
	outs->insert({SeqData::ARRAY, ptrVar});
	outs->insert({SeqData::LEN, lenVar});
}

Value *types::ArrayType::callFuncOf(Function *func,
                                    ValMap outs,
                                    BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *ptr = builder.CreateLoad(getSafe(outs, SeqData::ARRAY));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));
	std::vector<Value *> args = {ptr, len};
	return builder.CreateCall(func, args);
}

Value *types::ArrayType::pack(BaseFunc *base,
                              ValMap outs,
                              BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);

	Value *ptr = builder.CreateLoad(getSafe(outs, SeqData::ARRAY));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));

	Value *packed = builder.CreateInsertValue(UndefValue::get(getLLVMType(context)), len, {0});
	return builder.CreateInsertValue(packed, ptr, {1});
}

void types::ArrayType::unpack(BaseFunc *base,
                              Value *value,
                              ValMap outs,
                              BasicBlock *block)
{
	LLVMContext& context = base->getContext();
	BasicBlock *preambleBlock = base->getPreamble();
	IRBuilder<> builder(block);

	Value *ptr = builder.CreateExtractValue(value, {1});
	Value *len = builder.CreateExtractValue(value, {0});

	Value *ptrVar = makeAlloca(PointerType::get(getBaseType()->getLLVMType(context), 0), preambleBlock);
	Value *lenVar = makeAlloca(zeroLLVM(context), preambleBlock);

	builder.CreateStore(ptr, ptrVar);
	builder.CreateStore(len, lenVar);

	outs->insert({SeqData::ARRAY, ptrVar});
	outs->insert({SeqData::LEN, lenVar});
}

void types::ArrayType::callCopy(BaseFunc *base,
                                ValMap ins,
                                ValMap outs,
                                BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	BasicBlock *preambleBlock = base->getPreamble();

	Function *copyFunc = cast<Function>(
	                       block->getModule()->getOrInsertFunction(
	                         copyFuncName(),
	                         IntegerType::getInt8PtrTy(context),
	                         IntegerType::getInt8PtrTy(context),
	                         seqIntLLVM(context),
	                         seqIntLLVM(context)));

	copyFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);
	Value *ptr = builder.CreateLoad(getSafe(ins, SeqData::ARRAY));
	Value *len = builder.CreateLoad(getSafe(ins, SeqData::LEN));
	Value *elemSize = ConstantInt::get(seqIntLLVM(context), (uint64_t)getBaseType()->size(block->getModule()));
	std::vector<Value *> args = {ptr, len, elemSize};
	Value *copy = builder.CreateCall(copyFunc, args, "");

	Value *ptrVar = makeAlloca(PointerType::get(getBaseType()->getLLVMType(context), 0), preambleBlock);
	Value *lenVar = makeAlloca(zeroLLVM(context), preambleBlock);

	builder.CreateStore(copy, ptrVar);
	builder.CreateStore(len, lenVar);

	outs->insert({SeqData::ARRAY, ptrVar});
	outs->insert({SeqData::LEN, lenVar});
}

void types::ArrayType::callSerialize(BaseFunc *base,
                                     ValMap outs,
                                     Value *fp,
                                     BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	const std::string name = "serialize" + getName();
	Function *serialize = module->getFunction(name);
	bool makeFunc = (serialize == nullptr);

	if (makeFunc) {
		serialize = cast<Function>(
		              module->getOrInsertFunction(
		                "serialize" + getName(),
		                llvm::Type::getVoidTy(context),
		                PointerType::get(getBaseType()->getLLVMType(context), 0),
		                seqIntLLVM(context),
		                IntegerType::getInt8PtrTy(context)));
	}

	IRBuilder<> builder(block);

	if (makeFunc) {
		auto args = serialize->arg_begin();
		Value *ptrArg = args++;
		Value *lenArg = args++;
		Value *fpArg = args;

		BasicBlock *entry = BasicBlock::Create(context, "entry", serialize);
		BasicBlock *loop = BasicBlock::Create(context, "loop", serialize);

		builder.SetInsertPoint(loop);
		PHINode *control = builder.CreatePHI(seqIntLLVM(context), 2, "i");
		Value *next = builder.CreateAdd(control, oneLLVM(context), "next");
		Value *cond = builder.CreateICmpSLT(control, lenArg);

		BasicBlock *body = BasicBlock::Create(context, "body", serialize);
		BranchInst *branch = builder.CreateCondBr(cond, body, body);  // we set false-branch below

		builder.SetInsertPoint(body);

		BaseFuncLite serializeBase(serialize);
		auto subOuts1 = makeValMap();
		Value *elem = builder.CreateLoad(builder.CreateGEP(ptrArg, control));
		getBaseType()->unpack(&serializeBase, elem, subOuts1, body);
		getBaseType()->callSerialize(&serializeBase, subOuts1, fpArg, body);

		builder.CreateBr(loop);

		control->addIncoming(zeroLLVM(context), entry);
		control->addIncoming(next, body);

		BasicBlock *exit = BasicBlock::Create(context, "exit", serialize);
		builder.SetInsertPoint(exit);
		builder.CreateRetVoid();
		branch->setSuccessor(1, exit);

		builder.SetInsertPoint(entry);
		auto subOuts2 = makeValMap();
		IntType::get()->unpack(&serializeBase, lenArg, subOuts2, entry);
		IntType::get()->callSerialize(&serializeBase, subOuts2, fpArg, entry);
		builder.CreateBr(loop);
	}

	builder.SetInsertPoint(block);
	Value *ptr = builder.CreateLoad(getSafe(outs, SeqData::ARRAY));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));
	builder.CreateCall(serialize, {ptr, len, fp});
}

void types::ArrayType::callDeserialize(BaseFunc *base,
                                       ValMap outs,
                                       Value *fp,
                                       BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();
	BasicBlock *preambleBlock = base->getPreamble();

	const std::string name = "deserialize" + getName();
	Function *deserialize = module->getFunction(name);
	bool makeFunc = (deserialize == nullptr);

	if (makeFunc) {
		deserialize = cast<Function>(
		              module->getOrInsertFunction(
		                "deserialize" + getName(),
		                llvm::Type::getVoidTy(context),
		                PointerType::get(getBaseType()->getLLVMType(context), 0),
		                seqIntLLVM(context),
		                IntegerType::getInt8PtrTy(context)));
	}

	Function *allocFunc = cast<Function>(
	                        module->getOrInsertFunction(
	                          getBaseType()->allocFuncName(),
	                          IntegerType::getInt8PtrTy(context),
	                          IntegerType::getIntNTy(context, sizeof(size_t)*8)));

	IRBuilder<> builder(block);

	if (makeFunc) {
		auto args = deserialize->arg_begin();
		Value *ptrArg = args++;
		Value *lenArg = args++;
		Value *fpArg = args;

		BasicBlock *entry = BasicBlock::Create(context, "entry", deserialize);
		BasicBlock *loop = BasicBlock::Create(context, "loop", deserialize);

		builder.SetInsertPoint(loop);
		PHINode *control = builder.CreatePHI(seqIntLLVM(context), 2, "i");
		Value *next = builder.CreateAdd(control, oneLLVM(context), "next");
		Value *cond = builder.CreateICmpSLT(control, lenArg);

		BasicBlock *body = BasicBlock::Create(context, "body", deserialize);
		BranchInst *branch = builder.CreateCondBr(cond, body, body);  // we set false-branch below

		builder.SetInsertPoint(body);

		BaseFuncLite serializeBase(deserialize);
		auto subOuts1 = makeValMap();
		Value *elemPtr = builder.CreateGEP(ptrArg, control);
		getBaseType()->callDeserialize(&serializeBase, subOuts1, fpArg, body);
		Value *elem = getBaseType()->pack(&serializeBase, subOuts1, body);
		builder.CreateStore(elem, elemPtr);

		builder.CreateBr(loop);

		control->addIncoming(zeroLLVM(context), entry);
		control->addIncoming(next, body);

		BasicBlock *exit = BasicBlock::Create(context, "exit", deserialize);
		builder.SetInsertPoint(exit);
		builder.CreateRetVoid();
		branch->setSuccessor(1, exit);

		builder.SetInsertPoint(entry);
		builder.CreateBr(loop);
	}

	builder.SetInsertPoint(block);
	auto subOuts2 = makeValMap();
	IntType::get()->callDeserialize(base, subOuts2, fp, block);
	Value *len = builder.CreateLoad(getSafe(subOuts2, SeqData::INT));
	Value *size = ConstantInt::get(seqIntLLVM(context), (uint64_t)getBaseType()->size(module));
	Value *bytes = builder.CreateMul(len, size);
	bytes = builder.CreateBitCast(bytes, IntegerType::getIntNTy(context, sizeof(size_t)*8));
	Value *ptr = builder.CreateCall(allocFunc, {bytes});
	builder.CreateCall(deserialize, {ptr, len, fp});

	Value *ptrVar = makeAlloca(
	                  ConstantPointerNull::get(
	                    PointerType::get(getBaseType()->getLLVMType(context), 0)), preambleBlock);
	Value *lenVar = makeAlloca(zeroLLVM(context), preambleBlock);

	builder.CreateStore(ptr, ptrVar);
	builder.CreateStore(len, lenVar);

	outs->insert({SeqData::ARRAY, ptrVar});
	outs->insert({SeqData::LEN, lenVar});
}

void types::ArrayType::codegenLoad(BaseFunc *base,
                                   ValMap outs,
                                   BasicBlock *block,
                                   Value *ptr,
                                   Value *idx)
{
	LLVMContext& context = base->getContext();
	BasicBlock *preambleBlock = base->getPreamble();
	IRBuilder<> builder(block);

	Value *zero = ConstantInt::get(IntegerType::getInt32Ty(context), 0);
	Value *one  = ConstantInt::get(IntegerType::getInt32Ty(context), 1);

	Value *memPtr = builder.CreateGEP(ptr, {idx, one});
	Value *lenPtr = builder.CreateGEP(ptr, {idx, zero});

	Value *mem = builder.CreateLoad(memPtr);
	Value *len = builder.CreateLoad(lenPtr);

	Value *memVar = makeAlloca(PointerType::get(getBaseType()->getLLVMType(context), 0), preambleBlock);
	Value *lenVar = makeAlloca(zeroLLVM(context), preambleBlock);

	builder.CreateStore(mem, memVar);
	builder.CreateStore(len, lenVar);

	outs->insert({SeqData::ARRAY, memVar});
	outs->insert({SeqData::LEN, lenVar});
}

void types::ArrayType::codegenStore(BaseFunc *base,
                                    ValMap outs,
                                    BasicBlock *block,
                                    Value *ptr,
                                    Value *idx)
{
	LLVMContext& context = base->getContext();
	IRBuilder<> builder(block);

	Value *arr = builder.CreateLoad(getSafe(outs, SeqData::ARRAY));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));

	Value *zero = ConstantInt::get(IntegerType::getInt32Ty(context), 0);
	Value *one  = ConstantInt::get(IntegerType::getInt32Ty(context), 1);

	Value *arrPtr = builder.CreateGEP(ptr, {idx, one});
	Value *lenPtr = builder.CreateGEP(ptr, {idx, zero});

	builder.CreateStore(arr, arrPtr);
	builder.CreateStore(len, lenPtr);
}

void types::ArrayType::codegenIndexLoad(BaseFunc *base,
                                        ValMap outs,
                                        BasicBlock *block,
                                        Value *ptr,
                                        Value *idx)
{
	getBaseType()->codegenLoad(base, outs, block, ptr, idx);
}

void types::ArrayType::codegenIndexStore(BaseFunc *base,
                                         ValMap outs,
                                         BasicBlock *block,
                                         Value *ptr,
                                         Value *idx)
{
	getBaseType()->codegenStore(base, outs, block, ptr, idx);
}

bool types::ArrayType::isGeneric(Type *type) const
{
	return dynamic_cast<types::ArrayType *>(type) != nullptr;
}

types::Type *types::ArrayType::getBaseType() const
{
	return getBaseType(0);
}

types::Type *types::ArrayType::getBaseType(seq_int_t idx) const
{
	return baseType;
}

Type *types::ArrayType::getLLVMType(LLVMContext& context) const
{
	llvm::StructType *arrStruct = StructType::create(context, "arr_t");
	arrStruct->setBody({seqIntLLVM(context),
	                    PointerType::get(baseType->getLLVMType(context), 0)});
	return arrStruct;
}

seq_int_t types::ArrayType::size(Module *module) const
{
	std::unique_ptr<DataLayout> layout(new DataLayout(module));
	return layout->getTypeAllocSize(getLLVMType(module->getContext()));
}

types::ArrayType& types::ArrayType::of(Type& baseType) const
{
	return *ArrayType::get(&baseType);
}

types::ArrayType *types::ArrayType::get(Type *baseType)
{
	return new ArrayType(baseType);
}

types::ArrayType *types::ArrayType::get()
{
	return new ArrayType(types::BaseType::get());
}
