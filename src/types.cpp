#include <cstdlib>
#include <iostream>
#include <vector>
#include "stage.h"
#include "types.h"

using namespace seq::types;
using seq::SeqData;

Type::Type(Type *parent, SeqData key, std::string printName, void *print) :
    parent(parent), key(key), printFunc(nullptr),
    printName(std::move(printName)), print(print)
{
}

Type::Type(Type *parent, SeqData key) : Type(parent, key, "", nullptr)
{
}

Type::Type(Type *parent) : Type(parent, SeqData::NONE)
{
}

void Type::callPrint(std::shared_ptr<std::map<SeqData, llvm::Value *>> outs, llvm::BasicBlock *block)
{
	if (print == nullptr || getKey() == SeqData::NONE)
		throw exc::SeqException("cannot print specified type");

	if (!printFunc) {
		printFunc = llvm::cast<llvm::Function>(
	                  block->getModule()->getOrInsertFunction(
		              printName,
		              llvm::Type::getVoidTy(block->getContext()),
		              getLLVMType(block->getContext())));

		printFunc->setCallingConv(llvm::CallingConv::C);
	}

	auto iter = outs->find(getKey());

	if (iter == outs->end())
		throw exc::SeqException("pipeline error");

	llvm::IRBuilder<> builder(block);
	std::vector<llvm::Value *> args = {iter->second};
	builder.CreateCall(printFunc, args, "");
}

void Type::finalizePrint(llvm::ExecutionEngine *eng)
{
	eng->addGlobalMapping(printFunc, print);
}

void Seq::callPrint(std::shared_ptr<std::map<SeqData, llvm::Value *>> outs, llvm::BasicBlock *block)
{
	if (print == nullptr)
		throw exc::SeqException("cannot print specified type");

	if (!printFunc) {
		printFunc = llvm::cast<llvm::Function>(
		              block->getModule()->getOrInsertFunction(
		              printName,
		              llvm::Type::getVoidTy(block->getContext()),
		              llvm::IntegerType::getInt8PtrTy(block->getContext()),
		              llvm::IntegerType::getInt32Ty(block->getContext())));

		printFunc->setCallingConv(llvm::CallingConv::C);
	}

	auto seqiter = outs->find(SeqData::SEQ);
	auto leniter = outs->find(SeqData::LEN);

	if (seqiter == outs->end() || leniter == outs->end())
		throw exc::SeqException("pipeline error");

	llvm::IRBuilder<> builder(block);
	std::vector<llvm::Value *> args = {seqiter->second, leniter->second};
	builder.CreateCall(printFunc, args, "");
}
