#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::GenericType::GenericType() :
    Type("Generic", types::BaseType::get()), aboutToBeRealized(false),
    genericName(), type(nullptr)
{
}

void types::GenericType::markAboutToBeRealized()
{
	aboutToBeRealized = true;
}

void types::GenericType::unmarkAboutToBeRealized()
{
	aboutToBeRealized = false;
}

void types::GenericType::setName(std::string name)
{
	genericName = std::move(name);
}

void types::GenericType::realize(types::Type *type)
{
	assert(!this->type);
	this->type = type;
}

bool types::GenericType::realized() const
{
	return type != nullptr;
}

void types::GenericType::ensure() const
{
	if (!realized())
		throw exc::SeqException("generic type '" + genericName + "' not yet realized");
}

types::Type *types::GenericType::getType() const
{
	ensure();
	auto *genType = dynamic_cast<types::GenericType *>(type);
	return genType ? genType->getType() : type;
}

std::string types::GenericType::getName() const
{
	if (!type)
		return genericName.empty() ? "<Generic>" : genericName;
	return type->getName();
}

types::Type *types::GenericType::getParent() const
{
	ensure();
	return type->getParent();
}

bool types::GenericType::isAbstract() const
{
	ensure();
	return type->isAbstract();
}

types::VTable& types::GenericType::getVTable()
{
	ensure();
	return type->getVTable();
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

Value *types::GenericType::storeInAlloca(BaseFunc *base,
                                         Value *self,
                                         BasicBlock *block)
{
	ensure();
	return type->storeInAlloca(base, self, block);
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

Value *types::GenericType::indexSlice(BaseFunc *base,
                                      Value *self,
                                      Value *from,
                                      Value *to,
                                      BasicBlock *block)
{
	ensure();
	return type->indexSlice(base, self, from, to, block);
}

Value *types::GenericType::indexSliceNoFrom(BaseFunc *base,
                                            Value *self,
                                            Value *to,
                                            BasicBlock *block)
{
	ensure();
	return type->indexSliceNoFrom(base, self, to, block);
}

Value *types::GenericType::indexSliceNoTo(BaseFunc *base,
                                          Value *self,
                                          Value *to,
                                          BasicBlock *block)
{
	ensure();
	return type->indexSliceNoTo(base, self, to, block);
}

types::Type* types::GenericType::indexType() const
{
	ensure();
	return type->indexType();
}

types::Type* types::GenericType::subscriptType() const
{
	ensure();
	return type->subscriptType();
}

Value *types::GenericType::call(BaseFunc *base,
                                Value *self,
                                const std::vector<Value *>& args,
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

void types::GenericType::addMethod(std::string name, BaseFunc *func, bool force)
{
	ensure();
	type->addMethod(name, func, force);
}

Value *types::GenericType::staticMemb(const std::string& name, BasicBlock *block)
{
	ensure();
	return type->staticMemb(name, block);
}

types::Type *types::GenericType::staticMembType(const std::string& name)
{
	ensure();
	return type->staticMembType(name);
}

Value *types::GenericType::defaultValue(BasicBlock *block)
{
	ensure();
	return type->defaultValue(block);
}

Value *types::GenericType::construct(BaseFunc *base,
                                     const std::vector<Value *>& args,
                                     BasicBlock *block)
{
	ensure();
	return type->construct(base, args, block);
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

OpSpec types::GenericType::findUOp(const std::string& symbol)
{
	ensure();
	return type->findUOp(symbol);
}

OpSpec types::GenericType::findBOp(const std::string& symbol, types::Type *rhsType)
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
	if (!realized())
		return this == type;

	// reduce argument to realized type
	types::GenericType *g1 = nullptr;
	while ((g1 = dynamic_cast<types::GenericType *>(type)) && g1->realized())
		type = g1->type;

	// reduce ourselves to realized type
	types::GenericType *g2 = nullptr;
	types::Type *self = this->type;
	while ((g2 = dynamic_cast<types::GenericType *>(self)) && g2->realized())
		self = g2->type;

	return self->is(type);
}

bool types::GenericType::isGeneric(types::Type *type) const
{
	ensure();
	return this->type->isGeneric(type);
}

unsigned types::GenericType::numBaseTypes() const
{
	return realized() ? type->numBaseTypes() : 0;
}

types::Type *types::GenericType::getBaseType(unsigned idx) const
{
	ensure();
	return type->getBaseType(idx);
}

types::Type *types::GenericType::getCallType(const std::vector<Type *>& inTypes)
{
	ensure();
	return type->getCallType(inTypes);
}

types::Type *types::GenericType::getConstructType(const std::vector<Type *>& inTypes)
{
	ensure();
	return type->getConstructType(inTypes);
}

Type *types::GenericType::getLLVMType(LLVMContext& context) const
{
	ensure();
	return type->getLLVMType(context);
}

seq_int_t types::GenericType::size(Module *module) const
{
	ensure();
	return type->size(module);
}

types::RecordType *types::GenericType::asRec()
{
	ensure();
	return type->asRec();
}

types::RefType *types::GenericType::asRef()
{
	ensure();
	return type->asRef();
}

types::GenType *types::GenericType::asGen()
{
	ensure();
	return type->asGen();
}

types::OptionalType *types::GenericType::asOpt()
{
	ensure();
	return type->asOpt();
}

types::GenericType *types::GenericType::get()
{
	return new GenericType();
}

types::GenericType *types::GenericType::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (types::GenericType *)ref->getClone(this);

	if (!aboutToBeRealized && !realized())
		return this;

	auto *x = types::GenericType::get();
	ref->addClone(this, x);
	x->setName(genericName);
	if (type) x->realize(type->clone(ref));
	return x;
}

bool types::GenericType::findInType(types::Type *type, std::vector<unsigned>& path)
{
	if (type->is(this))
		return true;

	const unsigned numBases = type->numBaseTypes();
	for (unsigned i = 0; i < numBases; i++) {
		path.push_back(i);
		if (findInType(type->getBaseType(i), path))
			return true;
		path.pop_back();
	}

	return false;
}

Generic::Generic(bool performCaching) :
    performCaching(performCaching), root(this), generics(), cloneCache()
{
}

bool Generic::unrealized()
{
	for (auto *generic : generics) {
		if (generic->realized())
			return false;
	}
	return true;
}

std::vector<types::Type *> Generic::getRealizedTypes() const
{
	std::vector<types::Type *> types;
	for (auto *generic : generics)
		types.push_back(generic->getType());
	return types;
}

bool Generic::is(Generic *other) const
{
	return typeMatch<types::GenericType>(generics, other->generics);
}

Generic *Generic::findCachedRealizedType(std::vector<types::Type *> types) const
{
	for (auto& v : root->realizationCache) {
		if (typeMatch<>(v.first, types)) {
			return v.second;
		}
	}

	return nullptr;
}

void Generic::setCloneBase(Generic *x, Generic *ref)
{
	std::vector<types::GenericType *> genericsCloned;
	for (auto *generic : generics)
		genericsCloned.push_back(generic->clone(ref));

	x->generics = genericsCloned;
	x->root = root;
}

void Generic::addGenerics(int count)
{
	generics.clear();
	root->realizationCache.clear();

	for (int i = 0; i < count; i++)
		generics.push_back(types::GenericType::get());

	std::vector<types::Type *> types;
	for (auto *generic : generics)
		types.push_back(generic);

	root->realizationCache.emplace_back(types, this);
}

unsigned Generic::numGenerics() const
{
	return (unsigned)generics.size();
}

types::GenericType *Generic::getGeneric(int idx) const
{
	assert((unsigned)idx < generics.size());
	return generics[idx];
}

bool Generic::seenClone(void *p)
{
	return cloneCache.find(p) != cloneCache.end();
}

void *Generic::getClone(void *p)
{
	auto iter = cloneCache.find(p);
	assert(iter != cloneCache.end());
	return iter->second;
}

void Generic::addClone(void *p, void *clone)
{
	assert(!seenClone(p));
	cloneCache.insert({p, clone});
}

Generic *Generic::realizeGeneric(std::vector<types::Type *> types)
{
	if (types.size() != generics.size())
		throw exc::SeqException("expected " + std::to_string(generics.size()) +
		                        " type parameters, but got " + std::to_string(types.size()));

	if (performCaching) {
		Generic *cached = findCachedRealizedType(types);

		if (cached)
			return cached;
	}

	for (auto *generic : generics)
		generic->markAboutToBeRealized();

	Generic *x = clone(this);
	root->realizationCache.emplace_back(types, x);

	for (unsigned i = 0; i < types.size(); i++)
		x->generics[i]->realize(types[i]);

	for (auto *generic : generics)
		generic->unmarkAboutToBeRealized();

	cloneCache.clear();
	return x;
}

std::vector<types::Type *> Generic::deduceTypesFromArgTypes(const std::vector<types::Type *>& inTypes,
                                                            const std::vector<types::Type *>& argTypes)
{
	assert(unrealized());

	if (argTypes.size() != inTypes.size())
		throw exc::SeqException("expected " + std::to_string(inTypes.size()) + " arguments, " +
		                        "but got " + std::to_string(argTypes.size()));

	std::vector<types::Type *> types(numGenerics(), nullptr);
	std::vector<unsigned> path;

	for (unsigned i = 0; i < types.size(); i++) {
		for (unsigned j = 0; j < argTypes.size(); j++) {
			if (!argTypes[j])
				continue;

			path.clear();
			types::Type *inType = inTypes[j];

			if (getGeneric(i)->findInType(inType, path)) {
				types::Type *argType = argTypes[j];

				/*
				 * OK, we found the generic type nested in `inType`;
				 * now extract the corresponding type from `argType`.
				 */
				bool match = true;
				for (unsigned k : path) {
					if (argType->numBaseTypes() <= k) {
						match = false;
						break;
					}

					inType = inType->getBaseType(k);
					argType = argType->getBaseType(k);
				}

				if (match) {
					types[i] = argType;
					break;
				}
			}
		}
	}

	for (auto *type : types) {
		if (!type)
			throw exc::SeqException("cannot deduce all type parameters for generic '" + genericName() + "'");
	}

	return types;
}
