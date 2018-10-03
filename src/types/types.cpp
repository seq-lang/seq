#include <cstdlib>
#include <iostream>
#include <vector>
#include <typeinfo>
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::Type::Type(std::string name, types::Type *parent, bool abstract) :
    name(std::move(name)), parent(parent), abstract(abstract)
{
}

std::string types::Type::getName() const
{
	return name;
}

types::Type *types::Type::getParent() const
{
	return parent;
}

bool types::Type::isAbstract() const
{
	return abstract;
}

types::VTable& types::Type::getVTable()
{
	return vtable;
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

Value *types::Type::storeInAlloca(BaseFunc *base,
                                  Value *self,
                                  BasicBlock *block)
{
	return storeInAlloca(base, self, block, false);
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
	if (!getVTable().copy || isAbstract())
		throw exc::SeqException("cannot copy type '" + getName() + "'");

	auto *copyFunc = cast<Function>(
	                   block->getModule()->getOrInsertFunction(
	                     getVTable().copyName,
	                     getLLVMType(block->getContext()),
	                     getLLVMType(block->getContext())));

	copyFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);
	return builder.CreateCall(copyFunc, {self});
}

void types::Type::finalizeCopy(Module *module, ExecutionEngine *eng)
{
	Function *copyFunc = module->getFunction(getVTable().copyName);
	if (copyFunc)
		eng->addGlobalMapping(copyFunc, getVTable().copy);
}

void types::Type::print(BaseFunc *base,
                        Value *self,
                        BasicBlock *block)
{
	if (!getVTable().print || isAbstract())
		throw exc::SeqException("cannot print type '" + getName() + "'");

	auto *printFunc = cast<Function>(
	                    block->getModule()->getOrInsertFunction(
	                      getVTable().printName,
	                      llvm::Type::getVoidTy(block->getContext()),
	                      getLLVMType(block->getContext())));

	printFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);
	builder.CreateCall(printFunc, {self});
}

void types::Type::finalizePrint(Module *module, ExecutionEngine *eng)
{
	Function *printFunc = module->getFunction(getVTable().printName);
	if (printFunc)
		eng->addGlobalMapping(printFunc, getVTable().print);
}

void types::Type::serialize(BaseFunc *base,
                            Value *self,
                            Value *fp,
                            BasicBlock *block)
{
	if (isAbstract())
		throw exc::SeqException("type '" + getName() + "' cannot be serialized");

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	auto *writeFunc = cast<Function>(
	                    module->getOrInsertFunction(
	                      "seq_io_write",
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
	Function *writeFunc = module->getFunction("seq_io_write");
	if (writeFunc)
		eng->addGlobalMapping(writeFunc, (void *)seq_io_write);
}

Value *types::Type::deserialize(BaseFunc *base,
                                Value *fp,
                                BasicBlock *block)
{
	if (isAbstract())
		throw exc::SeqException("type '" + getName() + "' cannot be serialized");

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();
	BasicBlock *preambleBlock = base->getPreamble();

	auto *readFunc = cast<Function>(
	                   module->getOrInsertFunction(
	                     "seq_io_read",
	                     llvm::Type::getVoidTy(context),
	                     IntegerType::getInt8PtrTy(context),
	                     seqIntLLVM(context),
	                     seqIntLLVM(context),
	                     IntegerType::getInt8PtrTy(context)));

	readFunc->setCallingConv(CallingConv::C);

	IRBuilder<> builder(block);
	Value *resultVar = makeAlloca(getLLVMType(context), preambleBlock);
	Value *resultVarGeneric = builder.CreateBitCast(resultVar, IntegerType::getInt8PtrTy(context));
	Value *sizeVal = ConstantInt::get(seqIntLLVM(context), (uint64_t)size(module));
	builder.CreateCall(readFunc, {resultVarGeneric, sizeVal, oneLLVM(context), fp});
	return builder.CreateLoad(resultVar);
}

void types::Type::finalizeDeserialize(Module *module, ExecutionEngine *eng)
{
	Function *readFunc = module->getFunction("seq_io_read");
	if (readFunc)
		eng->addGlobalMapping(readFunc, (void *)seq_io_read);
}

Value *types::Type::alloc(Value *count, BasicBlock *block)
{
	if (size(block->getModule()) == 0)
		throw exc::SeqException("cannot create array of type '" + getName() + "'");

	LLVMContext& context = block->getContext();
	Module *module = block->getModule();

	auto *allocFunc = cast<Function>(
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

Value *types::Type::alloc(seq_int_t count, BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	return alloc(ConstantInt::get(seqIntLLVM(context), (uint64_t)count, true), block);
}

void types::Type::finalizeAlloc(Module *module, ExecutionEngine *eng)
{
	Function *allocFunc = module->getFunction(allocFuncName());
	if (allocFunc)
		eng->addGlobalMapping(allocFunc, (void *)(isAtomic() ? seq_alloc_atomic : seq_alloc));
}

Value *types::Type::load(BaseFunc *base,
                         Value *ptr,
                         Value *idx,
                         BasicBlock *block)
{
	if (size(block->getModule()) == 0 || isAbstract())
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
	if (size(block->getModule()) == 0 || isAbstract())
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

Value *types::Type::indexSlice(BaseFunc *base,
                               Value *self,
                               Value *from,
                               Value *to,
                               BasicBlock *block)
{
	throw exc::SeqException("cannot index into type '" + getName() + "'");
}

Value *types::Type::indexSliceNoFrom(BaseFunc *base,
                                     Value *self,
                                     Value *to,
                                     BasicBlock *block)
{
	throw exc::SeqException("cannot index into type '" + getName() + "'");
}

Value *types::Type::indexSliceNoTo(BaseFunc *base,
                                   Value *self,
                                   Value *to,
                                   BasicBlock *block)
{
	throw exc::SeqException("cannot index into type '" + getName() + "'");
}

types::Type *types::Type::indexType() const
{
	throw exc::SeqException("cannot index into type '" + getName() + "'");
}

types::Type *types::Type::subscriptType() const
{
	throw exc::SeqException("cannot index into type '" + getName() + "'");
}

Value *types::Type::call(BaseFunc *base,
                         Value *self,
                         const std::vector<Value *>& args,
                         BasicBlock *block)
{
	throw exc::SeqException("cannot call type '" + getName() + "'");
}

Value *types::Type::memb(Value *self,
                         const std::string& name,
                         BasicBlock *block)
{
	initFields();
	auto iter1 = getVTable().methods.find(name);

	if (iter1 != getVTable().methods.end()) {
		FuncExpr e(iter1->second);
		auto *type = dynamic_cast<FuncType *>(e.getType());
		assert(type);
		Value *func = e.codegen(nullptr, block);
		return MethodType::get(this, type)->make(self, func, block);
	}

	auto iter2 = getVTable().fields.find(name);

	if (iter2 == getVTable().fields.end())
		throw exc::SeqException("type '" + getName() + "' has no member '" + name + "'");

	IRBuilder<> builder(block);
	return builder.CreateExtractValue(self, iter2->second.first);
}

types::Type *types::Type::membType(const std::string& name)
{
	initFields();
	auto iter1 = getVTable().methods.find(name);

	if (iter1 != getVTable().methods.end()) {
		FuncExpr e(iter1->second);
		auto *type = dynamic_cast<FuncType *>(e.getType());
		assert(type);
		return MethodType::get(this, type);
	}

	auto iter2 = getVTable().fields.find(name);

	if (iter2 == getVTable().fields.end() || iter2->second.second->is(types::Void))
		throw exc::SeqException("type '" + getName() + "' has no member '" + name + "'");

	return iter2->second.second;
}

Value *types::Type::staticMemb(const std::string& name, BasicBlock *block)
{
	throw exc::SeqException("type '" + getName() + "' has no static member '" + name + "'");
}

types::Type *types::Type::staticMembType(const std::string& name)
{
	throw exc::SeqException("type '" + getName() + "' has no static member '" + name + "'");
}

Value *types::Type::setMemb(Value *self,
                            const std::string& name,
                            Value *val,
                            BasicBlock *block)
{
	initFields();
	auto iter = getVTable().fields.find(name);

	if (iter == getVTable().fields.end())
		throw exc::SeqException("type '" + getName() + "' has no assignable member '" + name + "'");

	IRBuilder<> builder(block);
	return builder.CreateInsertValue(self, val, iter->second.first);
}

bool types::Type::hasMethod(const std::string& name)
{
	return getVTable().methods.find(name) != getVTable().methods.end();
}

void types::Type::addMethod(std::string name, BaseFunc *func, bool force)
{
	if (hasMethod(name)) {
		if (force) {
			getVTable().methods[name] = func;
			return;
		} else {
			throw exc::SeqException("duplicate method '" + name + "'");
		}
	}

	if (getVTable().fields.find(name) != getVTable().fields.end())
		throw exc::SeqException("field '" + name + "' conflicts with method");

	getVTable().methods.insert({name, func});
}

BaseFunc *types::Type::getMethod(const std::string& name)
{
	auto iter = getVTable().methods.find(name);

	if (iter == getVTable().methods.end())
		throw exc::SeqException("type '" + getName() + "' has no method '" + name + "'");

	return iter->second;
}

Value *types::Type::defaultValue(BasicBlock *block)
{
	throw exc::SeqException("type '" + getName() + "' has no default value");
}

Value *types::Type::construct(BaseFunc *base,
                              const std::vector<Value *>& args,
                              BasicBlock *block)
{
	throw exc::SeqException("cannot construct type '" + getName() + "'");
}

void types::Type::initOps()
{
}

void types::Type::initFields()
{
}

OpSpec types::Type::findUOp(const std::string& symbol)
{
	initOps();
	Op op = uop(symbol);

	for (auto& e : getVTable().ops) {
		if (e.op == op)
			return e;
	}

	throw exc::SeqException("type '" + getName() + "' does not support operator '" + symbol + "'");
}

OpSpec types::Type::findBOp(const std::string& symbol, types::Type *rhsType)
{
	initOps();
	Op op = bop(symbol);

	for (auto& e : getVTable().ops) {
		if (e.op == op && types::is(rhsType, e.rhsType))
			return e;
	}

	throw exc::SeqException(
	  "type '" + getName() + "' does not support operator '" +
	    symbol + "' applied to type '" + rhsType->getName() + "'");
}

bool types::Type::isAtomic() const
{
	return true;
}

bool types::Type::is(types::Type *type) const
{
	return isGeneric(type);
}

bool types::Type::isGeneric(types::Type *type) const
{
	return typeid(*this) == typeid(*type);
}

unsigned types::Type::numBaseTypes() const
{
	return 0;
}

types::Type *types::Type::getBaseType(unsigned idx) const
{
	throw exc::SeqException("type '" + getName() + "' has no base types");
}

types::Type *types::Type::getCallType(const std::vector<Type *>& inTypes)
{
	throw exc::SeqException("cannot call type '" + getName() + "'");
}

types::Type *types::Type::getConstructType(const std::vector<Type *>& inTypes)
{
	throw exc::SeqException("cannot construct type '" + getName() + "'");
}

Type *types::Type::getLLVMType(LLVMContext& context) const
{
	throw exc::SeqException("cannot instantiate '" + getName() + "' class");
}

seq_int_t types::Type::size(Module *module) const
{
	return 0;
}

types::GenType *types::Type::asGen()
{
	return nullptr;
}

types::Type *types::Type::clone(Generic *ref)
{
	return this;
}

bool types::is(types::Type *type1, types::Type *type2)
{
	return type1->is(type2) || type2->is(type1);
}
