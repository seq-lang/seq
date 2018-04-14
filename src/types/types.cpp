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

void types::Type::setFuncArgs(Function *func,
                              ValMap outs,
                              BasicBlock *block)
{
	if (getKey() == SeqData::NONE)
		throw exc::SeqException("cannot initialize arguments of function of type '" + getName() + "'");

	Value *arg = func->arg_begin();
	Value *var = makeAlloca(arg, block);
	outs->insert({getKey(), var});
}

Value *types::Type::callFuncOf(Function *func,
		                       ValMap outs,
                               BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *input = builder.CreateLoad(getSafe(outs, getKey()));
	std::vector<Value *> args = {input};
	return builder.CreateCall(func, args);
}

Value *types::Type::pack(BaseFunc *base,
                         ValMap outs,
                         BasicBlock *block)
{
	IRBuilder<> builder(block);
	return builder.CreateLoad(getSafe(outs, getKey()));
}

void types::Type::unpack(BaseFunc *base,
                         Value *value,
                         ValMap outs,
                         BasicBlock *block)
{
	LLVMContext& context = base->getContext();
	BasicBlock *preambleBlock = base->getPreamble();
	IRBuilder<> builder(block);

	Value *var = makeAlloca(getLLVMType(context), preambleBlock);
	builder.CreateStore(value, var);
	outs->insert({getKey(), var});
}

void types::Type::callCopy(BaseFunc *base,
                           ValMap ins,
                           ValMap outs,
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
	std::vector<Value *> args = {builder.CreateLoad(getSafe(ins, getKey()))};
	Value *result = builder.CreateCall(copyFunc, args, "");
	unpack(base, result, outs, block);
}

void types::Type::finalizeCopy(Module *module, ExecutionEngine *eng)
{
	Function *copyFunc = module->getFunction(copyFuncName());
	if (copyFunc)
		eng->addGlobalMapping(copyFunc, vtable.print);
}

Value *types::Type::checkEq(BaseFunc *base,
                            ValMap ins1,
                            ValMap ins2,
                            BasicBlock *block)
{
	throw exc::SeqException("type '" + getName() + "' does not support equality checks");
}

void types::Type::callPrint(BaseFunc *base,
                            ValMap outs,
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
	std::vector<Value *> args = {builder.CreateLoad(getSafe(outs, getKey()))};
	builder.CreateCall(printFunc, args, "");
}

void types::Type::finalizePrint(Module *module, ExecutionEngine *eng)
{
	Function *printFunc = module->getFunction(printFuncName());
	if (printFunc)
		eng->addGlobalMapping(printFunc, vtable.print);
}

void types::Type::callSerialize(BaseFunc *base,
                                ValMap outs,
                                BasicBlock *block,
                                std::string file)
{
	if (!vtable.serialize || getKey() == SeqData::NONE)
		throw exc::SeqException("type '" + getName() + "' cannot be serialized");

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	Function *serializeFunc = cast<Function>(
	                            block->getModule()->getOrInsertFunction(
	                               serializeFuncName(),
	                               llvm::Type::getVoidTy(context),
	                               getLLVMType(context),
	                               IntegerType::getInt8PtrTy(context)));

	serializeFunc->setCallingConv(CallingConv::C);

	GlobalVariable *fileVar = new GlobalVariable(*module,
	                                             llvm::ArrayType::get(IntegerType::getInt8Ty(context),
	                                                                  file.length() + 1),
	                                             true,
	                                             GlobalValue::PrivateLinkage,
	                                             ConstantDataArray::getString(context, file),
	                                             "file");
	fileVar->setAlignment(1);

	IRBuilder<> builder(block);
	Value *filename = builder.CreateGEP(fileVar, ConstantInt::get(seqIntLLVM(context), 0, false));
	std::vector<Value *> args = {builder.CreateLoad(getSafe(outs, getKey())), filename};
	builder.CreateCall(serializeFunc, args, "");
}

void types::Type::finalizeSerialize(Module *module, ExecutionEngine *eng)
{
	Function *serializeFunc = module->getFunction(serializeFuncName());
	if (serializeFunc)
		eng->addGlobalMapping(serializeFunc, vtable.serialize);
}

void types::Type::callDeserialize(BaseFunc *base,
                                  ValMap outs,
                                  BasicBlock *block,
                                  std::string file)
{
	if (!vtable.deserialize || getKey() == SeqData::NONE)
		throw exc::SeqException("type '" + getName() + "' cannot be serialized");

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	Function *deserializeFunc = cast<Function>(
	                              block->getModule()->getOrInsertFunction(
	                                deserializeFuncName(),
	                                getLLVMType(context),
	                                IntegerType::getInt8PtrTy(context)));

	deserializeFunc->setCallingConv(CallingConv::C);

	GlobalVariable *fileVar = new GlobalVariable(*module,
	                                             llvm::ArrayType::get(IntegerType::getInt8Ty(context), file.length() + 1),
	                                             true,
	                                             GlobalValue::PrivateLinkage,
	                                             ConstantDataArray::getString(context, file),
	                                             "file");
	fileVar->setAlignment(1);

	IRBuilder<> builder(block);
	BasicBlock *preambleBlock = base->getPreamble();

	Value *filename = builder.CreateGEP(fileVar, ConstantInt::get(seqIntLLVM(context), 0, false));
	std::vector<Value *> args = {filename};
	Value *result = builder.CreateCall(deserializeFunc, args, "");

	Value *resultVar = makeAlloca(getLLVMType(context), preambleBlock);
	builder.CreateStore(result, resultVar);

	outs->insert({getKey(), resultVar});
}

void types::Type::finalizeDeserialize(Module *module, ExecutionEngine *eng)
{
	Function *deserializeFunc = module->getFunction(deserializeFuncName());
	if (deserializeFunc)
		eng->addGlobalMapping(deserializeFunc, vtable.deserialize);
}

void types::Type::callSerializeArray(BaseFunc *base,
                                     ValMap outs,
                                     BasicBlock *block,
                                     std::string file)
{
	if (!vtable.serializeArray)
		throw exc::SeqException("array of type '" + getName() + "' cannot be serialized");

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	Function *serializeArrayFunc = cast<Function>(
	                                 block->getModule()->getOrInsertFunction(
	                                   serializeArrayFuncName(),
	                                   llvm::Type::getVoidTy(context),
	                                   PointerType::get(getLLVMType(context), 0),
	                                   seqIntLLVM(context),
	                                   IntegerType::getInt8PtrTy(context)));

	serializeArrayFunc->setCallingConv(CallingConv::C);

	GlobalVariable *fileVar = new GlobalVariable(*module,
	                                             llvm::ArrayType::get(IntegerType::getInt8Ty(context), file.length() + 1),
	                                             true,
	                                             GlobalValue::PrivateLinkage,
	                                             ConstantDataArray::getString(context, file),
	                                             "file");
	fileVar->setAlignment(1);

	IRBuilder<> builder(block);
	Value *ptr = builder.CreateLoad(getSafe(outs, SeqData::ARRAY));
	Value *len = builder.CreateLoad(getSafe(outs, SeqData::LEN));

	Value *filename = builder.CreateGEP(fileVar, ConstantInt::get(seqIntLLVM(context), 0, false));
	std::vector<Value *> args = {ptr, len, filename};
	builder.CreateCall(serializeArrayFunc, args, "");
}

void types::Type::finalizeSerializeArray(Module *module, ExecutionEngine *eng)
{
	Function *serializeArrayFunc = module->getFunction(serializeArrayFuncName());
	if (serializeArrayFunc)
		eng->addGlobalMapping(serializeArrayFunc, vtable.serializeArray);
}

void types::Type::callDeserializeArray(BaseFunc *base,
                                       ValMap outs,
                                       BasicBlock *block,
                                       std::string file)
{
	if (!vtable.deserializeArray)
		throw exc::SeqException("array of type '" + getName() + "' cannot be deserialized");

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	Function *deserializeArrayFunc = cast<Function>(
	                                   block->getModule()->getOrInsertFunction(
	                                     deserializeArrayFuncName(),
	                                     PointerType::get(getLLVMType(context), 0),
	                                     IntegerType::getInt8PtrTy(context),
	                                     PointerType::get(seqIntLLVM(context), 0)));

	deserializeArrayFunc->setCallingConv(CallingConv::C);

	GlobalVariable *fileVar = new GlobalVariable(*module,
	                                             llvm::ArrayType::get(IntegerType::getInt8Ty(context), file.length() + 1),
	                                             true,
	                                             GlobalValue::PrivateLinkage,
	                                             ConstantDataArray::getString(context, file),
	                                             "file");
	fileVar->setAlignment(1);

	IRBuilder<> builder(block);

	Value *filename = builder.CreateGEP(fileVar, ConstantInt::get(seqIntLLVM(context), 0));

	GlobalVariable *ptrVar = new GlobalVariable(*module,
	                                            PointerType::get(getLLVMType(context), 0),
	                                            false,
	                                            GlobalValue::PrivateLinkage,
	                                            nullptr,
	                                            "mem");
	ptrVar->setInitializer(
	  ConstantPointerNull::get(PointerType::get(getLLVMType(context), 0)));

	GlobalVariable *lenVar = new GlobalVariable(*module,
	                                            seqIntLLVM(context),
	                                            false,
	                                            GlobalValue::PrivateLinkage,
	                                            nullptr,
	                                            "len");
	lenVar->setInitializer(zeroLLVM(context));

	std::vector<Value *> args = {filename, lenVar};
	Value *ptr = builder.CreateCall(deserializeArrayFunc, args, "");

	builder.CreateStore(ptr, ptrVar);

	outs->insert({SeqData::ARRAY, ptrVar});
	outs->insert({SeqData::LEN, lenVar});
}

void types::Type::finalizeDeserializeArray(Module *module, ExecutionEngine *eng)
{
	Function *deserializeArrayFunc = module->getFunction(deserializeArrayFuncName());
	if (deserializeArrayFunc)
		eng->addGlobalMapping(deserializeArrayFunc, vtable.deserializeArray);
}

Value *types::Type::codegenAlloc(BaseFunc *base,
                                 seq_int_t count,
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

	std::vector<Value *> args = {
	  ConstantInt::get(IntegerType::getIntNTy(context, sizeof(size_t)*8), (unsigned)(count*size(block->getModule())))};
	Value *mem = builder.CreateCall(allocFunc, args);
	return builder.CreatePointerCast(mem, PointerType::get(getLLVMType(context), 0));
}

void types::Type::finalizeAlloc(Module *module, ExecutionEngine *eng)
{
	Function *allocFunc = module->getFunction(allocFuncName());
	if (allocFunc)
		eng->addGlobalMapping(allocFunc, (void *)std::malloc);
}

void types::Type::codegenLoad(BaseFunc *base,
                              ValMap outs,
                              BasicBlock *block,
                              Value *ptr,
                              Value *idx)
{
	if (size(block->getModule()) == 0 || getKey() == SeqData::NONE)
		throw exc::SeqException("cannot load type '" + getName() + "'");

	LLVMContext& context = base->getContext();
	BasicBlock *preambleBlock = base->getPreamble();
	IRBuilder<> builder(block);

	Value *var = makeAlloca(getLLVMType(context), preambleBlock);
	Value *val = builder.CreateLoad(builder.CreateGEP(ptr, idx));
	builder.CreateStore(val, var);
	outs->insert({getKey(), var});
}

void types::Type::codegenStore(BaseFunc *base,
                               ValMap outs,
                               BasicBlock *block,
                               Value *ptr,
                               Value *idx)
{
	if (size(block->getModule()) == 0|| getKey() == SeqData::NONE)
		throw exc::SeqException("cannot store type '" + getName() + "'");

	IRBuilder<> builder(block);
	Value *val = builder.CreateLoad(getSafe(outs, getKey()));
	builder.CreateStore(val, builder.CreateGEP(ptr, idx));
}

void types::Type::codegenIndexLoad(BaseFunc *base,
                                   ValMap outs,
                                   BasicBlock *block,
                                   Value *ptr,
                                   Value *idx)
{
	throw exc::SeqException("cannot index into type '" + getName() + "'");
}

void types::Type::codegenIndexStore(BaseFunc *base,
                                    ValMap outs,
                                    BasicBlock *block,
                                    Value *ptr,
                                    Value *idx)
{
	throw exc::SeqException("cannot index into type '" + getName() + "'");
}

bool types::Type::is(types::Type *type) const
{
	return getName() == type->getName();
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
