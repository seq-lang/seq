#include "base.h"
#include "exc.h"
#include "array.h"

using namespace seq;
using namespace llvm;

types::ArrayType::ArrayType(Type *base, seq_int_t count) :
    Type("Array", BaseType::get(), SeqData::ARRAY),
    base(base), count(count), mallocFunc(nullptr)
{
	if (count < 0)
		throw exc::SeqException("array dimension must be positive");
}

void types::ArrayType::callSerialize(std::shared_ptr<std::map<SeqData, Value *>> outs,
                                     BasicBlock *block,
                                     std::string file)
{
	base->callSerializeArray(outs, block, file);
}

void types::ArrayType::finalizeSerialize(ExecutionEngine *eng)
{
	base->finalizeSerializeArray(eng);
}

void types::ArrayType::callDeserialize(std::shared_ptr<std::map<SeqData, llvm::Value *>> outs,
                                       BasicBlock *block,
                                       std::string file)
{
	base->callDeserializeArray(outs, block, file);
}

void types::ArrayType::finalizeDeserialize(ExecutionEngine *eng)
{
	base->finalizeDeserializeArray(eng);
}

void types::ArrayType::finalizeAlloc(ExecutionEngine *eng)
{
	eng->addGlobalMapping(mallocFunc, (void *)std::malloc);
}

void types::ArrayType::callAlloc(std::shared_ptr<std::map<SeqData, Value *>> outs, BasicBlock *block)
{
	if (base->size() == 0)
		throw exc::SeqException("cannot create array of type '" + base->getName() + "'");

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	if (!mallocFunc) {
		mallocFunc = cast<Function>(
		               module->getOrInsertFunction(
		                 "malloc",
		                 IntegerType::getInt8PtrTy(context),
		                 IntegerType::getIntNTy(context, sizeof(size_t) * 8)));
	}

	IRBuilder<> builder(block);

	GlobalVariable *ptr = new GlobalVariable(*module,
	                                         IntegerType::getIntNPtrTy(context, (unsigned)base->size()*8),
	                                         false,
	                                         GlobalValue::PrivateLinkage,
	                                         nullptr,
	                                         "mem");

	ptr->setInitializer(
	  ConstantPointerNull::get(IntegerType::getIntNPtrTy(context, (unsigned)base->size())));

	std::vector<Value *> args = {
	  ConstantInt::get(IntegerType::getIntNTy(context, sizeof(size_t)*8), (unsigned)(count * base->size()))};
	Value *mem = builder.CreateCall(mallocFunc, args);
	mem = builder.CreatePointerCast(mem, IntegerType::getIntNPtrTy(context, (unsigned)base->size()*8));
	builder.CreateStore(mem, ptr);

	outs->insert({SeqData::ARRAY, ptr});
	outs->insert({SeqData::LEN, ConstantInt::get(seqIntLLVM(context), (uint64_t)count)});
}

Value *types::ArrayType::codegenLoad(BasicBlock *block,
                                     Value *ptr,
                                     Value *idx)
{
	return base->codegenLoad(block, ptr, idx);
}

void types::ArrayType::codegenStore(BasicBlock *block,
                                    Value *ptr,
                                    Value *idx,
                                    Value *val)
{
	base->codegenStore(block, ptr, idx, val);
}

Type *types::ArrayType::getLLVMType(LLVMContext& context)
{
	return llvm::ArrayType::get(base->getLLVMType(context), (uint64_t)count);
}

seq_int_t types::ArrayType::size() const
{
	return count * base->size();
}

types::Type *types::ArrayType::getBaseType() const
{
	return base;
}

types::ArrayType *types::ArrayType::of(Type& base) const
{
	return of(base, 0);
}

types::ArrayType *types::ArrayType::of(Type& base, seq_int_t count) const
{
	return ArrayType::get(&base, count);
}

types::ArrayType *types::ArrayType::get(Type *base, seq_int_t count)
{
	return new types::ArrayType(base, count);
}
