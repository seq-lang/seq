#include <cassert>
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::RefType::RefType(std::string name) :
    Type(std::move(name), BaseType::get(), SeqData::REF), idx(0), contents(nullptr), methods(), generics(),
    typeCached(StructType::create(getLLVMContext(), name)), cloneCache(), realizationCache()
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

void types::RefType::finalizeLLVMType()
{
	assert(contents);
	contents->addLLVMTypesToStruct(typeCached);
}

void types::RefType::addMethod(std::string name, Func *func)
{
	if (methods.find(name) != methods.end())
		throw exc::SeqException("duplicate method '" + name + "'");

	methods.insert({name, func});
}

void types::RefType::addGenerics(unsigned count)
{
	assert(generics.empty());
	for (unsigned i = 0; i < count; i++)
		generics.push_back(types::GenericType::get(this, i));

	std::vector<types::Type *> types;
	for (auto *generic : generics)
		types.push_back(generic);

	realizationCache.emplace_back(types, this);
}

void types::RefType::setGeneric(unsigned idx, types::Type *type)
{
	if (idx >= generics.size())
		throw exc::SeqException("too many type specifiers for class '" + getName() + "'");

	generics[idx]->realize(type);
}

types::GenericType *types::RefType::getGeneric(unsigned idx)
{
	assert(idx < generics.size());
	return generics[idx];
}

bool types::RefType::seenClone(void *p)
{
	return cloneCache.find(p) != cloneCache.end();
}

void *types::RefType::getClone(void *p)
{
	auto iter = cloneCache.find(p);
	assert(iter != cloneCache.end());
	return iter->second;
}

void types::RefType::addClone(void *p, void *clone)
{
	assert(!seenClone(p));
	cloneCache.insert({p, clone});
}

types::RefType *types::RefType::realize(std::vector<types::Type *> types)
{
	if (types.size() != generics.size())
		throw exc::SeqException("expected " + std::to_string(generics.size()) +
		                        " type paramters, but got " + std::to_string(types.size()));

	// see if we've encountered this realization before:
	for (auto& v : realizationCache) {
		assert(v.first.size() == types.size());
		bool match = true;
		for (unsigned i = 0; i < types.size(); i++) {
			if (!types[i]->is(v.first[i])) {
				match = false;
				break;
			}
		}

		if (match)
			return v.second;
	}

	types::RefType *x = clone(this);
	cloneCache.clear();

	for (unsigned i = 0; i < types.size(); i++)
		x->setGeneric(i, types[i]);

	x->finalizeLLVMType();
	realizationCache.emplace_back(types, x);
	return x;
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
	return ConstantPointerNull::get(cast<PointerType>(getLLVMType(block->getContext())));
}

void types::RefType::initOps()
{
	if (!vtable.ops.empty())
		return;

	vtable.ops = {
		{uop("!"), this, &Bool, [](Value *lhs, Value *rhs, IRBuilder<> &b) {
			return b.CreateZExt(b.CreateIsNull(lhs), Bool.getLLVMType(b.getContext()));
		}}
	};
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

bool types::RefType::is(types::Type *type) const
{
	return this == type;
}

types::Type *types::RefType::getBaseType(seq_int_t idx) const
{
	assert(contents);
	return contents->getBaseType(idx);
}

Type *types::RefType::getLLVMType(llvm::LLVMContext& context) const
{
	assert(typeCached);
	return PointerType::get(typeCached, 0);
}

seq_int_t types::RefType::size(Module *module) const
{
	return sizeof(void *);
}

Value *types::RefType::make(BasicBlock *block) const
{
	assert(contents);
	LLVMContext& context = block->getContext();
	Value *val = contents->defaultValue(block);
	Value *ref = contents->alloc(1, block);
	IRBuilder<> builder(block);
	val = builder.CreateBitCast(val, typeCached);
	ref = builder.CreateBitCast(ref, getLLVMType(context));
	builder.CreateStore(val, ref);
	return ref;
}

types::RefType *types::RefType::get(std::string name)
{
	return new RefType(std::move(name));
}

types::RefType *types::RefType::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (types::RefType *)ref->getClone(this);

	types::RefType *x = types::RefType::get(name + "." + std::to_string(idx + 1));
	ref->addClone(this, x);
	x->idx = idx + 1;
	x->setContents(contents->clone(ref));

	std::map<std::string, Func *> methodsCloned;
	std::vector<GenericType *> genericsCloned;

	for (auto& method : methods)
		methodsCloned.insert({method.first, method.second->clone(ref)});

	for (auto *generic : generics)
		genericsCloned.push_back(generic->clone(ref));

	x->methods = methodsCloned;
	x->generics = genericsCloned;
	return x;
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

types::MethodType *types::MethodType::clone(types::RefType *ref)
{
	return MethodType::get(self->clone(ref), func->clone(ref));
}

types::GenericType::GenericType(types::RefType *ref, unsigned idx) :
    Type("Generic", BaseType::get()), ref(ref), idx(idx), type(nullptr)
{
}

void types::GenericType::realize(types::Type *type)
{
	this->type = type;
	name = type->getName();
	parent = type->getParent();
	key = type->getKey();
	vtable = type->getVTable();
}

void types::GenericType::release()
{
	type = nullptr;
}

void types::GenericType::ensure() const
{
	if (!type)
		throw exc::SeqException("generic type not yet realized");
}

std::string types::GenericType::copyFuncName()
{
	ensure();
	return type->copyFuncName();
}

std::string types::GenericType::printFuncName()
{
	ensure();
	return type->printFuncName();
}

std::string types::GenericType::allocFuncName()
{
	ensure();
	return type->allocFuncName();
}

Value *types::GenericType::loadFromAlloca(BaseFunc *base,
                                          Value *var,
                                          BasicBlock *block)
{
	ensure();
	return type->loadFromAlloca(base, var, block);
}

Value *types::GenericType::storeInAlloca(BaseFunc *base,
                                         Value *self,
                                         BasicBlock *block,
                                         bool storeDefault)
{
	ensure();
	return type->storeInAlloca(base, self, block, storeDefault);
}

Value *types::GenericType::eq(BaseFunc *base,
                              Value *self,
                              Value *other,
                              BasicBlock *block)
{
	ensure();
	return type->eq(base, self, other, block);
}

llvm::Value *types::GenericType::copy(BaseFunc *base,
                                      Value *self,
                                      BasicBlock *block)
{
	ensure();
	return type->copy(base, self, block);
}

void types::GenericType::finalizeCopy(Module *module, ExecutionEngine *eng)
{
	ensure();
	type->finalizeCopy(module, eng);
}

void types::GenericType::print(BaseFunc *base,
                               Value *self,
                               BasicBlock *block)
{
	ensure();
	type->print(base, self, block);
}

void types::GenericType::finalizePrint(Module *module, ExecutionEngine *eng)
{
	ensure();
	type->finalizePrint(module, eng);
}

void types::GenericType::serialize(BaseFunc *base,
                                   Value *self,
                                   Value *fp,
                                   BasicBlock *block)
{
	ensure();
	type->serialize(base, self, fp, block);
}

void types::GenericType::finalizeSerialize(Module *module, ExecutionEngine *eng)
{
	ensure();
	type->finalizeSerialize(module, eng);
}

Value *types::GenericType::deserialize(BaseFunc *base,
                                       Value *fp,
                                       BasicBlock *block)
{
	ensure();
	return type->deserialize(base, fp, block);
}

void types::GenericType::finalizeDeserialize(Module *module, ExecutionEngine *eng)
{
	ensure();
	type->finalizeDeserialize(module, eng);
}

Value *types::GenericType::alloc(Value *count, BasicBlock *block)
{
	ensure();
	return type->alloc(count, block);
}

Value *types::GenericType::alloc(seq_int_t count, BasicBlock *block)
{
	ensure();
	return type->alloc(count, block);
}

void types::GenericType::finalizeAlloc(Module *module, ExecutionEngine *eng)
{
	ensure();
	type->finalizeAlloc(module, eng);
}

Value *types::GenericType::load(BaseFunc *base,
                                Value *ptr,
                                Value *idx,
                                BasicBlock *block)
{
	ensure();
	return type->load(base, ptr, idx, block);
}

void types::GenericType::store(BaseFunc *base,
                               Value *self,
                               Value *ptr,
                               Value *idx,
                               BasicBlock *block)
{
	ensure();
	type->store(base, self, ptr, idx, block);
}

Value *types::GenericType::indexLoad(BaseFunc *base,
                                     Value *self,
                                     Value *idx,
                                     BasicBlock *block)
{
	ensure();
	return type->indexLoad(base, self, idx, block);
}

void types::GenericType::indexStore(BaseFunc *base,
                                    Value *self,
                                    Value *idx,
                                    Value *val,
                                    BasicBlock *block)
{
	ensure();
	type->indexStore(base, self, idx, val, block);
}

Value *types::GenericType::call(BaseFunc *base,
                                Value *self,
                                std::vector<Value *> args,
                                BasicBlock *block)
{
	ensure();
	return type->call(base, self, args, block);
}

Value *types::GenericType::memb(Value *self,
                                const std::string& name,
                                BasicBlock *block)
{
	ensure();
	return type->memb(self, name, block);
}

types::Type *types::GenericType::membType(const std::string& name)
{
	ensure();
	return type->membType(name);
}

Value *types::GenericType::setMemb(Value *self,
                                   const std::string& name,
                                   Value *val,
                                   BasicBlock *block)
{
	ensure();
	return type->setMemb(self, name, val, block);
}

Value *types::GenericType::defaultValue(BasicBlock *block)
{
	ensure();
	return type->defaultValue(block);
}

void types::GenericType::initOps()
{
	ensure();
	type->initOps();
}

void types::GenericType::initFields()
{
	ensure();
	type->initFields();
}

OpSpec types::GenericType::findUOp(const std::string &symbol)
{
	ensure();
	return type->findUOp(symbol);
}

OpSpec types::GenericType::findBOp(const std::string &symbol, types::Type *rhsType)
{
	ensure();
	return type->findBOp(symbol, rhsType);
}

bool types::GenericType::isAtomic() const
{
	ensure();
	return type->isAtomic();
}

bool types::GenericType::is(types::Type *type) const
{
	if (!this->type)
		return this == type;

	return this->type->is(type);
}

bool types::GenericType::isGeneric(types::Type *type) const
{
	ensure();
	return this->type->isGeneric(type);
}

bool types::GenericType::isChildOf(types::Type *type) const
{
	ensure();
	return this->type->isChildOf(type);
}

types::Type *types::GenericType::getBaseType(seq_int_t idx) const
{
	ensure();
	return type->getBaseType(idx);
}

types::Type *types::GenericType::getCallType(std::vector<Type *> inTypes)
{
	ensure();
	return type->getCallType(inTypes);
}

Type *types::GenericType::getLLVMType(LLVMContext& context) const
{
	// this is just the easiest approach: return a type we won't use
	if (!type)
		return seqIntLLVM(context);

	return type->getLLVMType(context);
}

seq_int_t types::GenericType::size(Module *module) const
{
	ensure();
	return type->size(module);
}

Mem& types::GenericType::operator[](seq_int_t size)
{
	ensure();
	return (*type)[size];
}

types::GenericType *types::GenericType::get(types::RefType *ref, unsigned idx)
{
	return new GenericType(ref, idx);
}

types::GenericType *types::GenericType::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (types::GenericType *)ref->getClone(this);

	auto *x = types::GenericType::get(this->ref->clone(ref), idx);
	ref->addClone(this, x);
	if (type) x->realize(type->clone(ref));
	return x;
}
