#include <cstdlib>
#include <iostream>
#include <vector>
#include "stage.h"
#include "types.h"

using namespace seq;
using namespace llvm;
using seq::SeqData;

types::Type::Type(std::string name,
                  types::Type *parent,
                  SeqData key,
                  std::string printName,
                  void *print) :
    name(name), parent(parent), key(key), printFunc(nullptr),
    printName(std::move(printName)), print(print)
{
}

types::Type::Type(std::string name, types::Type *parent, SeqData key) :
    Type(name, parent, key, "", nullptr)
{
}

types::Type::Type(std::string name, Type *parent) :
    Type(name, parent, SeqData::NONE)
{
}

void types::Type::callPrint(std::shared_ptr<std::map<SeqData, Value *>> outs, BasicBlock *block)
{
	if (print == nullptr || getKey() == SeqData::NONE)
		throw exc::SeqException("cannot print specified type");

	if (!printFunc) {
		printFunc = cast<Function>(
	                  block->getModule()->getOrInsertFunction(
		                printName,
		                llvm::Type::getVoidTy(block->getContext()),
		                getLLVMType(block->getContext())));

		printFunc->setCallingConv(CallingConv::C);
	}

	auto iter = outs->find(getKey());

	if (iter == outs->end())
		throw exc::SeqException("pipeline error");

	IRBuilder<> builder(block);
	std::vector<Value *> args = {iter->second};
	builder.CreateCall(printFunc, args, "");
}

void types::Type::finalizePrint(ExecutionEngine *eng)
{
	eng->addGlobalMapping(printFunc, print);
}

Value *types::Type::codegenLoad(Module *module,
                                LLVMContext& context,
                                BasicBlock *block,
                                Value *ptr,
                                Value *idx)
{
	if (size() == 0)
		throw exc::SeqException("cannot load given type");

	IRBuilder<> builder(block);
	Value *ptrActual = builder.CreateLoad(ptr);
	return builder.CreateLoad(builder.CreateGEP(ptrActual, idx));
}

void types::Type::codegenStore(Module *module,
                               LLVMContext& context,
                               BasicBlock *block,
                               Value *ptr,
                               Value *idx,
                               Value *val)
{
	if (size() == 0)
		throw exc::SeqException("cannot store given type");

	IRBuilder<> builder(block);
	Value *ptrActual = builder.CreateLoad(ptr);
	builder.CreateStore(val, builder.CreateGEP(ptrActual, idx));
}

void types::Seq::callPrint(std::shared_ptr<std::map<SeqData, Value *>> outs, BasicBlock *block)
{
	if (print == nullptr)
		throw exc::SeqException("cannot print specified type");

	if (!printFunc) {
		printFunc = cast<Function>(
		              block->getModule()->getOrInsertFunction(
		                printName,
		                llvm::Type::getVoidTy(block->getContext()),
		                IntegerType::getInt8PtrTy(block->getContext()),
		                IntegerType::getInt32Ty(block->getContext())));

		printFunc->setCallingConv(CallingConv::C);
	}

	auto seqiter = outs->find(SeqData::SEQ);
	auto leniter = outs->find(SeqData::LEN);

	if (seqiter == outs->end() || leniter == outs->end())
		throw exc::SeqException("pipeline error");

	IRBuilder<> builder(block);
	std::vector<Value *> args = {seqiter->second, leniter->second};
	builder.CreateCall(printFunc, args, "");
}
