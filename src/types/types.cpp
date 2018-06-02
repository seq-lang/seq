#include <cstdlib>
#include <iostream>
#include <vector>
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::Type::Type(std::string name, types::Type *parent, SeqData key) :
    name(std::move(name)), parent(parent), key(key)
{
}

types::Type::Type(std::string name, Type *parent) :
    Type(std::move(name), parent, SeqData::NONE)
{
}

llvm::Type *types::Type::getFuncType(LLVMContext& context, Type *outType)
{
	return FunctionType::get(outType->getLLVMType(context), getLLVMType(context), false);
}

Function *types::Type::makeFuncOf(Module *module, Type *outType)
{
	static int idx = 1;
	LLVMContext& context = module->getContext();

	return cast<Function>(
	         module->getOrInsertFunction(
	           getName() + "Func" + std::to_string(idx++),
	           outType->getLLVMType(context),
	           getLLVMType(context)));
}

Value *types::Type::setFuncArgs(Function *func,
                                BasicBlock *block)
{
	if (getKey() == SeqData::NONE)
		throw exc::SeqException("cannot initialize arguments of function of type '" + getName() + "'");

	Value *arg = func->arg_begin();
	return makeAlloca(arg, block);
}

Value *types::Type::callFuncOf(Value *func,
                               Value *arg,
                               BasicBlock *block)
{
	IRBuilder<> builder(block);
	return builder.CreateCall(func, arg);
}

Value *types::Type::loadFromAlloca(BaseFunc *base,
                                   Value *var,
                                   BasicBlock *block)
{
	IRBuilder<> builder(block);
	return builder.CreateLoad(var);
}

Value *types::Type::storeInAlloca(BaseFunc *base,
                                  Value *self,
                                  BasicBlock *block,
                                  bool storeDefault)
{
	LLVMContext& context = base->getContext();
	BasicBlock *preambleBlock = base->getPreamble();
	IRBuilder<> builder(block);

	Value *var = makeAlloca(getLLVMType(context), preambleBlock);
	builder.CreateStore(self, var);

	if (storeDefault) {
		builder.SetInsertPoint(preambleBlock);
		builder.CreateStore(defaultValue(preambleBlock), var);
	}

	return var;
}

Value *types::Type::eq(BaseFunc *base,
                       Value *self,
                       Value *other,
                       BasicBlock *block)
{
	throw exc::SeqException("type '" + getName() + "' does not support equality checks");
}

llvm::Value *types::Type::copy(BaseFunc *base,
                               Value *self,
                               BasicBlock *block)
{
	if (!vtable.copy || getKey() == SeqData::NONE)
		throw exc::SeqException("cannot copy type '" + getName() + "'");

	Function *copyFunc = cast<Function>(
	                       block->getModule()->getOrInsertFunction(
	                         copyFuncName(),
	                         getLLVMType(block->getContext()),
	                         getLLVMType(block->getContext())));

	copyFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);
	return builder.CreateCall(copyFunc, {self});
}

void types::Type::finalizeCopy(Module *module, ExecutionEngine *eng)
{
	Function *copyFunc = module->getFunction(copyFuncName());
	if (copyFunc)
		eng->addGlobalMapping(copyFunc, vtable.print);
}

void types::Type::print(BaseFunc *base,
                        Value *self,
                        BasicBlock *block)
{
	if (!vtable.print || getKey() == SeqData::NONE)
		throw exc::SeqException("cannot print type '" + getName() + "'");

	Function *printFunc = cast<Function>(
	                        block->getModule()->getOrInsertFunction(
	                          printFuncName(),
	                          llvm::Type::getVoidTy(block->getContext()),
	                          getLLVMType(block->getContext())));

	printFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);
	builder.CreateCall(printFunc, {self});
}

void types::Type::finalizePrint(Module *module, ExecutionEngine *eng)
{
	Function *printFunc = module->getFunction(printFuncName());
	if (printFunc)
		eng->addGlobalMapping(printFunc, vtable.print);
}

void types::Type::serialize(BaseFunc *base,
                            Value *self,
                            Value *fp,
                            BasicBlock *block)
{
	if (getKey() == SeqData::NONE)
		throw exc::SeqException("type '" + getName() + "' cannot be serialized");

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	Function *writeFunc = cast<Function>(
	                        module->getOrInsertFunction(
	                          IO_WRITE_FUNC_NAME,
	                          llvm::Type::getVoidTy(context),
	                          IntegerType::getInt8PtrTy(context),
	                          seqIntLLVM(context),
	                          seqIntLLVM(context),
	                          IntegerType::getInt8PtrTy(context)));

	writeFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);
	Value *selfPtr = storeInAlloca(base, self, block);
	Value *ptrVal = builder.CreatePointerCast(selfPtr, IntegerType::getInt8PtrTy(context));
	Value *sizeVal = ConstantInt::get(seqIntLLVM(context), (uint64_t)size(module));
	builder.CreateCall(writeFunc, {ptrVal, sizeVal, oneLLVM(context), fp});
}

void types::Type::finalizeSerialize(Module *module, ExecutionEngine *eng)
{
	Function *writeFunc = module->getFunction(IO_WRITE_FUNC_NAME);
	if (writeFunc)
		eng->addGlobalMapping(writeFunc, (void *)util::io::io_write);
}

Value *types::Type::deserialize(BaseFunc *base,
                                Value *fp,
                                BasicBlock *block)
{
	if (getKey() == SeqData::NONE)
		throw exc::SeqException("type '" + getName() + "' cannot be serialized");

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();
	BasicBlock *preambleBlock = base->getPreamble();

	Function *readFunc = cast<Function>(
	                       module->getOrInsertFunction(
	                         IO_READ_FUNC_NAME,
	                         llvm::Type::getVoidTy(context),
	                         IntegerType::getInt8PtrTy(context),
	                         seqIntLLVM(context),
	                         seqIntLLVM(context),
	                         IntegerType::getInt8PtrTy(context)));

	readFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);
	Value *resultVar = makeAlloca(getLLVMType(context), preambleBlock);
	Value *sizeVal = ConstantInt::get(seqIntLLVM(context), (uint64_t)size(module));
	builder.CreateCall(readFunc, {resultVar, sizeVal, oneLLVM(context), fp});
	return builder.CreateLoad(resultVar);
}

void types::Type::finalizeDeserialize(Module *module, ExecutionEngine *eng)
{
	Function *readFunc = module->getFunction(IO_READ_FUNC_NAME);
	if (readFunc)
		eng->addGlobalMapping(readFunc, (void *)util::io::io_read);
}

Value *types::Type::alloc(BaseFunc *base,
                          Value *count,
                          BasicBlock *block)
{
	if (size(block->getModule()) == 0)
		throw exc::SeqException("cannot create array of type '" + getName() + "'");

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	Function *allocFunc = cast<Function>(
	                        module->getOrInsertFunction(
	                          allocFuncName(),
	                          IntegerType::getInt8PtrTy(context),
	                          IntegerType::getIntNTy(context, sizeof(size_t)*8)));

	IRBuilder<> builder(block);

	Value *elemSize = ConstantInt::get(seqIntLLVM(context), (uint64_t)size(block->getModule()));
	Value *fullSize = builder.CreateMul(count, elemSize);
	fullSize = builder.CreateBitCast(fullSize, IntegerType::getIntNTy(context, sizeof(size_t)*8));
	Value *mem = builder.CreateCall(allocFunc, {fullSize});
	return builder.CreatePointerCast(mem, PointerType::get(getLLVMType(context), 0));
}

Value *types::Type::alloc(BaseFunc *base,
                          seq_int_t count,
                          BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	return alloc(base, ConstantInt::get(seqIntLLVM(context), (uint64_t)count, true), block);
}

void types::Type::finalizeAlloc(Module *module, ExecutionEngine *eng)
{
	Function *allocFunc = module->getFunction(allocFuncName());
	if (allocFunc)
		eng->addGlobalMapping(allocFunc, (void *)std::malloc);
}

Value *types::Type::load(BaseFunc *base,
                         Value *ptr,
                         Value *idx,
                         BasicBlock *block)
{
	if (size(block->getModule()) == 0 || getKey() == SeqData::NONE)
		throw exc::SeqException("cannot load type '" + getName() + "'");

	IRBuilder<> builder(block);
	return builder.CreateLoad(builder.CreateGEP(ptr, idx));
}

void types::Type::store(BaseFunc *base,
                        Value *self,
                        Value *ptr,
                        Value *idx,
                        BasicBlock *block)
{
	if (size(block->getModule()) == 0|| getKey() == SeqData::NONE)
		throw exc::SeqException("cannot store type '" + getName() + "'");

	IRBuilder<> builder(block);
	builder.CreateStore(self, builder.CreateGEP(ptr, idx));
}

Value *types::Type::indexLoad(BaseFunc *base,
                              Value *self,
                              Value *idx,
                              BasicBlock *block)
{
	throw exc::SeqException("cannot index into type '" + getName() + "'");
}

void types::Type::indexStore(BaseFunc *base,
                             Value *self,
                             Value *idx,
                             Value *val,
                             BasicBlock *block)
{
	throw exc::SeqException("cannot index into type '" + getName() + "'");
}

Value *types::Type::call(BaseFunc *base,
                         Value *self,
                         Value *arg,
                         BasicBlock *block)
{
	throw exc::SeqException("cannot call type '" + getName() + "'");
}

Value *types::Type::memb(Value *self,
                         const std::string& name,
                         BasicBlock *block)
{
	initFields();
	auto iter = vtable.fields.find(name);

	if (iter == vtable.fields.end())
		throw exc::SeqException("type '" + getName() + "' has no member '" + name + "'");

	IRBuilder<> builder(block);
	return builder.CreateExtractValue(self, iter->second.first);
}

Value *types::Type::setMemb(Value *self,
                            const std::string& name,
                            Value *val,
                            BasicBlock *block)
{
	initFields();
	auto iter = vtable.fields.find(name);

	if (iter == vtable.fields.end())
		throw exc::SeqException("type '" + getName() + "' has no member '" + name + "'");

	IRBuilder<> builder(block);
	return builder.CreateInsertValue(self, val, iter->second.first);
}

Value *types::Type::defaultValue(BasicBlock *block)
{
	throw exc::SeqException("type '" + getName() + "' has no default value");
}

void types::Type::initOps()
{
}

void types::Type::initFields()
{
}

OpSpec types::Type::findUOp(const std::string &symbol)
{
	initOps();
	Op op = uop(symbol);

	for (auto& e : vtable.ops) {
		if (e.op == op)
			return e;
	}

	throw exc::SeqException("type '" + getName() + "' does not support operator '" + symbol + "'");
}

OpSpec types::Type::findBOp(const std::string &symbol, types::Type *rhsType)
{
	initOps();
	Op op = bop(symbol);

	for (auto& e : vtable.ops) {
		if (e.op == op && rhsType->isChildOf(e.rhsType))
			return e;
	}

	throw exc::SeqException(
	  "type '" + getName() + "' does not support operator '" +
	    symbol + "' applied to type '" + rhsType->getName() + "'");
}

bool types::Type::is(types::Type *type) const
{
	return getName() == type->getName();
}

bool types::Type::isGeneric(types::Type *type) const
{
	return is(type);
}

bool types::Type::isChildOf(types::Type *type) const
{
	return is(type) || (parent && parent->isChildOf(type));
}

std::string types::Type::getName() const
{
	return name;
}

SeqData types::Type::getKey() const
{
	return key;
}

types::Type *types::Type::getBaseType(seq_int_t idx) const
{
	throw exc::SeqException("type '" + getName() + "' has no base types");
}

types::Type *types::Type::getCallType(Type *inType)
{
	throw exc::SeqException("cannot call type '" + getName() + "'");
}

Type *types::Type::getLLVMType(LLVMContext& context) const
{
	throw exc::SeqException("cannot instantiate '" + getName() + "' class");
}

seq_int_t types::Type::size(Module *module) const
{
	return 0;
}

Mem& types::Type::operator[](seq_int_t size)
{
	return Mem::make(this, size);
}
