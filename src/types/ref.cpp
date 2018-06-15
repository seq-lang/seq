#include <cassert>
#include "seq/varexpr.h"
#include "seq/base.h"
#include "seq/ref.h"

using namespace seq;
using namespace llvm;

types::RefType::RefType(std::string name) :
    Type(std::move(name), BaseType::get()), contents(nullptr), methods()
{
}

types::RecordType *types::RefType::getContents()
{
	return contents;
}

void types::RefType::setContents(types::RecordType *contents)
{
	if (this->contents)
		throw exc::SeqException("cannot re-set contents of reference type");

	this->contents = contents;
}

void types::RefType::addMethod(std::string name, Func *func)
{
	if (methods.find(name) != methods.end())
		throw exc::SeqException("duplicate method '" + name + "'");

	methods.insert({name, func});
}

Value *types::RefType::memb(Value *self,
                            const std::string& name,
                            BasicBlock *block)
{
	initFields();
	auto iter = methods.find(name);

	if (iter != methods.end()) {
		FuncExpr e(iter->second);
		auto *type = dynamic_cast<FuncType *>(e.getType());
		assert(type);
		Value *func = e.codegen(nullptr, block);
		return MethodType::get(this, type)->make(self, func, block);
	}

	assert(contents);
	IRBuilder<> builder(block);
	Value *x = builder.CreateLoad(self);
	return contents->memb(x, name, block);
}

types::Type *types::RefType::membType(const std::string& name)
{
	initFields();
	auto iter = methods.find(name);

	if (iter != methods.end()) {
		FuncExpr e(iter->second);
		auto *type = dynamic_cast<FuncType *>(e.getType());
		assert(type);
		return MethodType::get(this, type);
	}

	return contents->membType(name);
}

Value *types::RefType::setMemb(Value *self,
                               const std::string& name,
                               Value *val,
                               BasicBlock *block)
{
	initFields();
	IRBuilder<> builder(block);
	Value *x = builder.CreateLoad(self);
	x = contents->setMemb(x, name, val, block);
	builder.CreateStore(x, self);
	return self;
}

Value *types::RefType::defaultValue(BasicBlock *block)
{
	assert(contents);
	LLVMContext& context = block->getContext();
	Value *val = contents->defaultValue(block);
	Value *ref = contents->alloc(1, block);
	IRBuilder<> builder(block);
	val = builder.CreateBitCast(val, contents->getLLVMTypeNamed(name, context));
	ref = builder.CreateBitCast(ref, getLLVMType(context));
	builder.CreateStore(val, ref);
	return ref;
}

void types::RefType::initFields()
{
	assert(contents);
	contents->initFields();
}

bool types::RefType::isAtomic() const
{
	return false;
}

types::Type *types::RefType::getBaseType(seq_int_t idx) const
{
	assert(contents);
	return contents->getBaseType(idx);
}

Type *types::RefType::getLLVMType(llvm::LLVMContext& context) const
{
	assert(contents);
	return PointerType::get(contents->getLLVMTypeNamed(name, context), 0);
}

seq_int_t types::RefType::size(Module *module) const
{
	return sizeof(void *);
}

types::RefType *types::RefType::get(std::string name)
{
	return new RefType(std::move(name));
}

types::MethodType::MethodType(types::RefType *self, FuncType *func) :
    RecordType({self, func}, {"self", "func"}), self(self), func(func)
{
}

Value *types::MethodType::call(BaseFunc *base,
                               Value *self,
                               std::vector<Value *> args,
                               BasicBlock *block)
{
	Value *x = memb(self, "self", block);
	Value *f = memb(self, "func", block);
	std::vector<Value *> argsFull(args);
	argsFull.insert(argsFull.begin(), x);
	return func->call(base, f, argsFull, block);
}

types::Type *types::MethodType::getCallType(std::vector<Type *> inTypes)
{
	std::vector<Type *> inTypesFull(inTypes);
	inTypesFull.insert(inTypesFull.begin(), self);
	return func->getCallType(inTypesFull);
}

Value *types::MethodType::make(Value *self, Value *func, BasicBlock *block)
{
	LLVMContext& context = self->getContext();
	Value *method = UndefValue::get(getLLVMType(context));
	method = setMemb(method, "self", self, block);
	method = setMemb(method, "func", func, block);
	return method;
}

types::MethodType *types::MethodType::get(types::RefType *self, types::FuncType *func)
{
	return new MethodType(self, func);
}
