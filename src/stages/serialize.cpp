#include "seq/serialize.h"

using namespace seq;
using namespace llvm;

Serialize::Serialize(std::string filename) :
    Stage("ser", types::AnyType::get(), types::VoidType::get()), filename(filename)
{
	name += "('" + filename + "')";
}

void Serialize::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();

	Function *openFunc = cast<Function>(
	                       module->getOrInsertFunction(
	                         IO_OPEN_WRITE_FUNC_NAME,
	                         IntegerType::getInt8PtrTy(context),
	                         IntegerType::getInt8PtrTy(context)));

	Function *closeFunc = cast<Function>(
	                        module->getOrInsertFunction(
	                          IO_CLOSE_FUNC_NAME,
	                          llvm::Type::getVoidTy(context),
	                          IntegerType::getInt8PtrTy(context)));

	block = prev->getAfter();

	GlobalVariable *fileVar = new GlobalVariable(*module,
	                                             llvm::ArrayType::get(IntegerType::getInt8Ty(context),
	                                                                  filename.length() + 1),
	                                             true,
	                                             GlobalValue::PrivateLinkage,
	                                             ConstantDataArray::getString(context, filename),
	                                             "file");
	fileVar->setAlignment(1);

	IRBuilder<> builder(block);
	Value *fp = builder.CreateCall(openFunc, {builder.CreateGEP(fileVar, zeroLLVM(context))});
	Value *val = builder.CreateLoad(prev->result);
	prev->getOutType()->serialize(getBase(), val, fp, block);
	builder.CreateCall(closeFunc, {fp});
	codegenNext(module);
	prev->setAfter(getAfter());
}

void Serialize::finalize(Module *module, ExecutionEngine *eng)
{
	Function *openFunc = module->getFunction(IO_OPEN_WRITE_FUNC_NAME);
	if (openFunc)
		eng->addGlobalMapping(openFunc, (void *)util::io::io_openw);

	Function *closeFunc = module->getFunction(IO_CLOSE_FUNC_NAME);
	if (closeFunc)
		eng->addGlobalMapping(closeFunc, (void *)util::io::io_close);

	if (prev)
		prev->getOutType()->finalizeSerialize(module, eng);

	Stage::finalize(module, eng);
}

Serialize& Serialize::make(std::string filename)
{
	return *new Serialize(std::move(filename));
}

Deserialize::Deserialize(types::Type *type, std::string filename) :
    Stage("deser", types::AnyType::get(), type), type(type), filename(filename)
{
	name += "('" + filename + "')";

	if (type->getKey() == SeqData::NONE)
		throw exc::SeqException("cannot deserialize type '" + type->getName() + "'");
}

void Deserialize::codegen(Module *module)
{
	ensurePrev();
	validate();

	LLVMContext& context = module->getContext();

	Function *openFunc = cast<Function>(
	                       module->getOrInsertFunction(
	                         IO_OPEN_READ_FUNC_NAME,
	                         IntegerType::getInt8PtrTy(context),
	                         IntegerType::getInt8PtrTy(context)));

	Function *closeFunc = cast<Function>(
	                        module->getOrInsertFunction(
	                          IO_CLOSE_FUNC_NAME,
	                          llvm::Type::getVoidTy(context),
	                          IntegerType::getInt8PtrTy(context)));

	block = prev->getAfter();

	GlobalVariable *fileVar = new GlobalVariable(*module,
	                                             llvm::ArrayType::get(IntegerType::getInt8Ty(context),
	                                                                  filename.length() + 1),
	                                             true,
	                                             GlobalValue::PrivateLinkage,
	                                             ConstantDataArray::getString(context, filename),
	                                             "file");
	fileVar->setAlignment(1);

	IRBuilder<> builder(block);
	Value *fp = builder.CreateCall(openFunc, {builder.CreateGEP(fileVar, zeroLLVM(context))});
	Value *val = type->deserialize(getBase(), fp, block);
	builder.CreateCall(closeFunc, {fp});
	result = type->storeInAlloca(getBase(), val, block, true);

	codegenNext(module);
	prev->setAfter(getAfter());
}

void Deserialize::finalize(Module *module, ExecutionEngine *eng)
{
	Function *openFunc = module->getFunction(IO_OPEN_READ_FUNC_NAME);
	if (openFunc)
		eng->addGlobalMapping(openFunc, (void *)util::io::io_openr);

	Function *closeFunc = module->getFunction(IO_CLOSE_FUNC_NAME);
	if (closeFunc)
		eng->addGlobalMapping(closeFunc, (void *)util::io::io_close);

	type->finalizeDeserialize(module, eng);
	Stage::finalize(module, eng);
}

Deserialize& Deserialize::make(types::Type *type, std::string filename)
{
	return *new Deserialize(type, std::move(filename));
}
