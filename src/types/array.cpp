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

Value *types::ArrayType::copy(BaseFunc *base,
                              Value *self,
                              BasicBlock *block)
{
	LLVMContext& context = block->getContext();

	Function *copyFunc = cast<Function>(
	                       block->getModule()->getOrInsertFunction(
	                         copyFuncName(),
	                         IntegerType::getInt8PtrTy(context),
	                         IntegerType::getInt8PtrTy(context),
	                         seqIntLLVM(context),
	                         seqIntLLVM(context)));

	copyFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);
	Value *ptr = Array.memb(self, "ptr", block);
	Value *len = Array.memb(self, "len", block);
	Value *elemSize = ConstantInt::get(seqIntLLVM(context), (uint64_t)getBaseType()->size(block->getModule()));
	std::vector<Value *> args = {ptr, len, elemSize};
	Value *copy = builder.CreateCall(copyFunc, args, "");
	return make(copy, len, block);
}

void types::ArrayType::serialize(BaseFunc *base,
                                 Value *self,
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
		Value *elem = getBaseType()->load(base, ptrArg, control, body);
		getBaseType()->serialize(&serializeBase, elem, fpArg, body);

		builder.CreateBr(loop);

		control->addIncoming(zeroLLVM(context), entry);
		control->addIncoming(next, body);

		BasicBlock *exit = BasicBlock::Create(context, "exit", serialize);
		builder.SetInsertPoint(exit);
		builder.CreateRetVoid();
		branch->setSuccessor(1, exit);

		builder.SetInsertPoint(entry);
		Int.serialize(&serializeBase, lenArg, fpArg, entry);
		builder.CreateBr(loop);
	}

	builder.SetInsertPoint(block);
	Value *ptr = Array.memb(self, "ptr", block);
	Value *len = Array.memb(self, "len", block);
	builder.CreateCall(serialize, {ptr, len, fp});
}

Value *types::ArrayType::deserialize(BaseFunc *base,
                                     Value *fp,
                                     BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

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

		BaseFuncLite deserializeBase(deserialize);
		Value *elemPtr = builder.CreateGEP(ptrArg, control);
		Value *elem = getBaseType()->deserialize(&deserializeBase, fpArg, body);
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
	Value *len = Int.deserialize(base, fp, block);
	Value *size = ConstantInt::get(seqIntLLVM(context), (uint64_t)getBaseType()->size(module));
	Value *bytes = builder.CreateMul(len, size);
	bytes = builder.CreateBitCast(bytes, IntegerType::getIntNTy(context, sizeof(size_t)*8));
	Value *ptr = builder.CreateCall(allocFunc, {bytes});
	builder.CreateCall(deserialize, {ptr, len, fp});
	return make(ptr, len, block);
}

Value *types::ArrayType::indexLoad(BaseFunc *base,
                                   Value *self,
                                   Value *idx,
                                   BasicBlock *block)
{
	Value *ptr = Array.memb(self, "ptr", block);
	return getBaseType()->load(base, ptr, idx, block);
}

void types::ArrayType::indexStore(BaseFunc *base,
                                  Value *self,
                                  Value *idx,
                                  Value *val,
                                  BasicBlock *block)
{
	Value *ptr = Array.memb(self, "ptr", block);
	getBaseType()->store(base, val, ptr, idx, block);
}

Value *types::ArrayType::defaultValue(BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Value *ptr = ConstantPointerNull::get(PointerType::get(getBaseType()->getLLVMType(context), 0));
	Value *len = zeroLLVM(context);
	return make(ptr, len, block);
}

bool types::ArrayType::isGeneric(Type *type) const
{
	return dynamic_cast<types::ArrayType *>(type) != nullptr;
}

void types::ArrayType::initFields()
{
	if (!vtable.fields.empty())
		return;

	vtable.fields = {
		{"len", {0, &Int}},
		{"ptr", {1, &Void}}
	};
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

Value *types::ArrayType::make(Value *ptr, Value *len, BasicBlock *block)
{
	LLVMContext& context = ptr->getContext();
	Value *self = UndefValue::get(getLLVMType(context));
	self = Array.setMemb(self, "ptr", ptr, block);
	self = Array.setMemb(self, "len", len, block);
	return self;
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
