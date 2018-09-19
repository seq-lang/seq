#include <cassert>
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::RefType::RefType(std::string name) :
    Type(std::move(name), BaseType::get()), Generic(true),
    done(false), root(this), cache(), pendingRealizations(),
    contents(types::RecordType::get({}))
{
}

void types::RefType::setDone()
{
	assert(this == root && !done);
	done = true;

	for (auto& pair : pendingRealizations)
		pair.second->realize(realize(pair.first));

	pendingRealizations.clear();
}

void types::RefType::setContents(types::RecordType *contents)
{
	this->contents = contents;
}

std::string types::RefType::getName() const
{
	std::string params = contents->getName();
	return Type::getName() + "[" + params.substr(1, params.length() - 2) + "]";
}

std::string types::RefType::genericName()
{
	return getName();
}

types::Type *types::RefType::realize(std::vector<types::Type *> types)
{
	assert(this == root);

	auto *cached = dynamic_cast<types::RefType *>(findCachedRealizedType(types));

	if (cached)
		return cached;

	if (!done) {
		types::GenericType *proxy = types::GenericType::get();
		pendingRealizations.emplace_back(types, proxy);
		return proxy;
	}

	Generic *x = realizeGeneric(types);
	auto *ref = dynamic_cast<types::RefType *>(x);
	assert(ref);

	for (auto& method : ref->getVTable().methods)
		method.second->resolveTypes();

	return ref;
}

std::vector<types::Type *> types::RefType::deduceTypesFromArgTypes(std::vector<types::Type *> argTypes)
{
	assert(unrealized());
	std::vector<types::Type *> inTypes = contents->getTypes();

	if (argTypes.size() != inTypes.size())
		throw exc::SeqException("expected " + std::to_string(inTypes.size()) + " constructor arguments, " +
		                        "but got " + std::to_string(argTypes.size()));

	std::vector<types::Type *> types(numGenerics(), nullptr);

	for (unsigned i = 0; i < argTypes.size(); i++) {
		auto *genericType = dynamic_cast<types::GenericType *>(inTypes[i]);
		if (genericType) {
			int idx = findGenericParameter(genericType);
			if (idx >= 0 && !types[idx] && argTypes[i])
				types[idx] = argTypes[i];
		}
	}

	for (auto *type : types) {
		if (!type)
			throw exc::SeqException("cannot deduce all type parameters for construction of generic class '" + getName() + "'");
	}

	return types;
}

Value *types::RefType::memb(Value *self,
                            const std::string& name,
                            BasicBlock *block)
{
	initFields();
	auto iter = getVTable().methods.find(name);

	if (iter != getVTable().methods.end()) {
		FuncExpr e(iter->second);
		auto *type = dynamic_cast<FuncType *>(e.getType());
		assert(type);
		Value *func = e.codegen(nullptr, block);
		return MethodType::get(this, type)->make(self, func, block);
	}

	assert(contents);
	IRBuilder<> builder(block);
	Value *x = builder.CreateLoad(self);

	try {
		return contents->memb(x, name, block);
	} catch (exc::SeqException& e) {
		throw exc::SeqException("type '" + getName() + "' has no member '" + name + "'");
	}
}

types::Type *types::RefType::membType(const std::string& name)
{
	initFields();
	auto iter = getVTable().methods.find(name);

	if (iter != getVTable().methods.end()) {
		FuncExpr e(iter->second);
		auto *type = dynamic_cast<FuncType *>(e.getType());
		assert(type);
		return MethodType::get(this, type);
	}

	try {
		return contents->membType(name);
	} catch (exc::SeqException& e) {
		throw exc::SeqException("type '" + getName() + "' has no member '" + name + "'");
	}
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

Value *types::RefType::staticMemb(const std::string& name, BasicBlock *block)
{
	auto iter = getVTable().methods.find(name);

	if (iter == getVTable().methods.end())
		return Type::staticMemb(name, block);

	FuncExpr e(iter->second);
	auto *type = dynamic_cast<FuncType *>(e.getType());
	assert(type);
	return e.codegen(nullptr, block);
}

types::Type *types::RefType::staticMembType(const std::string& name)
{
	auto iter = getVTable().methods.find(name);

	if (iter == getVTable().methods.end())
		return Type::staticMembType(name);

	FuncExpr e(iter->second);
	return e.getType();
}

Value *types::RefType::defaultValue(BasicBlock *block)
{
	return ConstantPointerNull::get(cast<PointerType>(getLLVMType(block->getContext())));
}

Value *types::RefType::construct(BaseFunc *base,
                                 const std::vector<Value *>& args,
                                 BasicBlock *block)
{
	return make(block, args);
}

void types::RefType::initOps()
{
	if (!vtable.ops.empty())
		return;

	vtable.ops = {
		{uop("!"), this, Bool, [](Value *lhs, Value *rhs, IRBuilder<>& b) {
			return b.CreateZExt(b.CreateIsNull(lhs), Bool->getLLVMType(b.getContext()));
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
	auto *ref = dynamic_cast<types::RefType *>(type);
	return ref && name == ref->name && Generic::is(ref);
}

types::Type *types::RefType::getBaseType(seq_int_t idx) const
{
	assert(contents);
	return contents->getBaseType(idx);
}

types::Type *types::RefType::getConstructType(const std::vector<Type *>& inTypes)
{
	std::vector<types::Type *> expTypes = contents->getTypes();

	if (inTypes.size() != expTypes.size())
		throw exc::SeqException("expected " + std::to_string(expTypes.size()) + " arguments, " +
		                        "but got " + std::to_string(inTypes.size()));

	for (unsigned i = 0; i < inTypes.size(); i++) {
		if (!types::is(inTypes[i], expTypes[i]))
			throw exc::SeqException("expected " + expTypes[i]->getName() +
			                        ", but got " + inTypes[i]->getName());
	}

	return this;
}

Type *types::RefType::getLLVMType(llvm::LLVMContext& context) const
{
	std::vector<types::Type *> types = getRealizedTypes();

	for (auto& v : root->cache) {
		if (typeMatch<>(v.first, types)) {
			return PointerType::get(v.second, 0);
		}
	}

	StructType *structType = StructType::create(context, name);
	root->cache.emplace_back(types, structType);
	contents->addLLVMTypesToStruct(structType);
	return PointerType::get(structType, 0);
}

seq_int_t types::RefType::size(Module *module) const
{
	return sizeof(void *);
}

Value *types::RefType::make(BasicBlock *block, std::vector<Value *> vals)
{
	assert(contents);
	LLVMContext& context = block->getContext();
	Value *val = contents->defaultValue(block);
	Value *ref = contents->alloc(1, block);
	IRBuilder<> builder(block);
	llvm::Type *type = getLLVMType(context);
	ref = builder.CreateBitCast(ref, type);
	val = builder.CreateBitCast(val, cast<PointerType>(type)->getElementType());
	builder.CreateStore(val, ref);

	if (!vals.empty()) {
		for (unsigned i = 0; i < vals.size(); i++)
			ref = setMemb(ref, std::to_string(i+1), vals[i], block);
	}

	return ref;
}

types::RefType *types::RefType::get(std::string name)
{
	return new RefType(std::move(name));
}

types::RefType *types::RefType::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (types::RefType *)ref->getClone(this);

	types::RefType *x = types::RefType::get(name);
	ref->addClone(this, x);
	setCloneBase(x, ref);

	x->setContents(contents->clone(ref));

	std::map<std::string, BaseFunc *> methodsCloned;
	for (auto& method : getVTable().methods)
		methodsCloned.insert({method.first, method.second->clone(ref)});

	x->getVTable().methods = methodsCloned;
	x->root = root;
	x->done = true;
	return x;
}

types::MethodType::MethodType(types::Type *self, FuncType *func) :
    RecordType({self, func}, {"self", "func"}), self(self), func(func)
{
}

Value *types::MethodType::call(BaseFunc *base,
                               Value *self,
                               const std::vector<Value *>& args,
                               BasicBlock *block)
{
	Value *x = memb(self, "self", block);
	Value *f = memb(self, "func", block);
	std::vector<Value *> argsFull(args);
	argsFull.insert(argsFull.begin(), x);
	return func->call(base, f, argsFull, block);
}

types::Type *types::MethodType::getCallType(const std::vector<Type *>& inTypes)
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

types::MethodType *types::MethodType::get(types::Type *self, types::FuncType *func)
{
	return new MethodType(self, func);
}

types::MethodType *types::MethodType::clone(Generic *ref)
{
	return MethodType::get(self->clone(ref), func->clone(ref));
}
