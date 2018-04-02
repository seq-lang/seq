#include <cstdlib>
#include <iostream>
#include <vector>
#include "seq.h"

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

Function *types::Type::makeFuncOf(Module *module,
                                  ValMap outs,
                                  Type *outType)
{
	static int idx = 1;
	LLVMContext& context = module->getContext();

	Function *func = cast<Function>(
	                   module->getOrInsertFunction(
	                     getName() + "Func" + std::to_string(idx++),
	                     outType->getLLVMType(context),
	                     PointerType::get(getLLVMType(context), 0)));

	auto args = func->arg_begin();
	Value *arg = args;
	outs->insert({getKey(), arg});
	return func;
}

Value *types::Type::callFuncOf(llvm::Function *func,
		                       ValMap outs,
                               llvm::BasicBlock *block)
{
	IRBuilder<> builder(block);
	Value *input = getSafe(outs, getKey());
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

	if (!vtable.printFunc) {
		vtable.printFunc = cast<Function>(
		                     block->getModule()->getOrInsertFunction(
		                       "print" + getName(),
		                       llvm::Type::getVoidTy(block->getContext()),
		                       getLLVMType(block->getContext())));

		vtable.printFunc->setCallingConv(CallingConv::C);
	}

	IRBuilder<> builder(block);
	std::vector<Value *> args = {builder.CreateLoad(getSafe(outs, getKey()))};
	builder.CreateCall(vtable.printFunc, args, "");
}

void types::Type::finalizePrint(ExecutionEngine *eng)
{
	eng->addGlobalMapping(vtable.printFunc, vtable.print);
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

	if (!vtable.serializeFunc) {
		vtable.serializeFunc = cast<Function>(
		                         block->getModule()->getOrInsertFunction(
		                           "serialize" + getName(),
		                           llvm::Type::getVoidTy(context),
		                           getLLVMType(context),
		                           IntegerType::getInt8PtrTy(context)));

		vtable.serializeFunc->setCallingConv(CallingConv::C);
	}

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
	builder.CreateCall(vtable.serializeFunc, args, "");
}

void types::Type::finalizeSerialize(ExecutionEngine *eng)
{
	eng->addGlobalMapping(vtable.serializeFunc, vtable.serialize);
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

	if (!vtable.deserializeFunc) {
		vtable.deserializeFunc = cast<Function>(
		                           block->getModule()->getOrInsertFunction(
		                             "deserialize" + getName(),
		                             getLLVMType(context),
		                             IntegerType::getInt8PtrTy(context)));

		vtable.deserializeFunc->setCallingConv(CallingConv::C);
	}

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
	Value *result = builder.CreateCall(vtable.deserializeFunc, args, "");

	Value *resultVar = makeAlloca(getLLVMType(context), preambleBlock);
	builder.CreateStore(result, resultVar);

	outs->insert({getKey(), resultVar});
}

void types::Type::finalizeDeserialize(ExecutionEngine *eng)
{
	eng->addGlobalMapping(vtable.deserializeFunc, vtable.deserialize);
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

	if (!vtable.serializeArrayFunc) {
		vtable.serializeArrayFunc = cast<Function>(
		                              block->getModule()->getOrInsertFunction(
		                                "serialize" + getName() + "Array",
		                                llvm::Type::getVoidTy(context),
		                                PointerType::get(getLLVMType(context), 0),
		                                seqIntLLVM(context),
		                                IntegerType::getInt8PtrTy(context)));

		vtable.serializeArrayFunc->setCallingConv(CallingConv::C);
	}

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
	builder.CreateCall(vtable.serializeArrayFunc, args, "");
}

void types::Type::finalizeSerializeArray(ExecutionEngine *eng)
{
	eng->addGlobalMapping(vtable.serializeArrayFunc, vtable.serializeArray);
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

	if (!vtable.deserializeArrayFunc) {
		vtable.deserializeArrayFunc = cast<Function>(
		                                block->getModule()->getOrInsertFunction(
		                                  "deserialize" + getName() + "Array",
		                                  PointerType::get(getLLVMType(context), 0),
		                                  IntegerType::getInt8PtrTy(context),
		                                  PointerType::get(seqIntLLVM(context), 0)));

		vtable.deserializeArrayFunc->setCallingConv(CallingConv::C);
	}

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
	Value *ptr = builder.CreateCall(vtable.deserializeArrayFunc, args, "");

	builder.CreateStore(ptr, ptrVar);

	outs->insert({SeqData::ARRAY, ptrVar});
	outs->insert({SeqData::LEN, lenVar});
}

void types::Type::finalizeDeserializeArray(ExecutionEngine *eng)
{
	eng->addGlobalMapping(vtable.deserializeArrayFunc, vtable.deserializeArray);
}

Value *types::Type::codegenAlloc(BaseFunc *base,
                                 seq_int_t count,
                                 BasicBlock *block)
{
	if (size() == 0)
		throw exc::SeqException("cannot create array of type '" + getName() + "'");

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	if (!vtable.allocFunc) {
		vtable.allocFunc = cast<Function>(
		                     module->getOrInsertFunction(
		                       "malloc",
		                       IntegerType::getInt8PtrTy(context),
		                       IntegerType::getIntNTy(context, sizeof(size_t)*8)));
	}

	IRBuilder<> builder(block);

	std::vector<Value *> args = {
	  ConstantInt::get(IntegerType::getIntNTy(context, sizeof(size_t)*8), (unsigned)(count*size()))};
	Value *mem = builder.CreateCall(vtable.allocFunc, args);
	return builder.CreatePointerCast(mem, PointerType::get(getLLVMType(context), 0));
}

void types::Type::finalizeAlloc(ExecutionEngine *eng)
{
	eng->addGlobalMapping(vtable.allocFunc, (void *)std::malloc);
}

void types::Type::codegenLoad(BaseFunc *base,
                              ValMap outs,
                              BasicBlock *block,
                              Value *ptr,
                              Value *idx)
{
	if (size() == 0 || getKey() == SeqData::NONE)
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
	if (size() == 0|| getKey() == SeqData::NONE)
		throw exc::SeqException("cannot store type '" + getName() + "'");

	IRBuilder<> builder(block);
	Value *val = builder.CreateLoad(getSafe(outs, getKey()));
	builder.CreateStore(val, builder.CreateGEP(ptr, idx));
}

bool types::Type::isChildOf(types::Type *type)
{
	return (getName() == type->getName()) || (parent && parent->isChildOf(type));
}

std::string types::Type::getName() const
{
	return name;
}

SeqData types::Type::getKey() const
{
	return key;
}

Type *types::Type::getLLVMType(LLVMContext& context)
{
	throw exc::SeqException("cannot instantiate '" + getName() + "' class");
}

seq_int_t types::Type::size() const
{
	return 0;
}

Mem& types::Type::operator[](seq_int_t size)
{
	return Mem::make(this, size);
}
