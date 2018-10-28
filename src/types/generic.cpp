#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::GenericType::GenericType() :
    Type("generic", types::BaseType::get()), aboutToBeRealized(false),
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

Value *types::GenericType::alloc(Value *count, BasicBlock *block)
{
	ensure();
	return type->alloc(count, block);
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

bool types::GenericType::hasMethod(const std::string& name)
{
	ensure();
	return type->hasMethod(name);
}

void types::GenericType::addMethod(std::string name, BaseFunc *func, bool force)
{
	ensure();
	type->addMethod(name, func, force);
}

BaseFunc *types::GenericType::getMethod(const std::string& name)
{
	ensure();
	return type->getMethod(name);
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

Value *types::GenericType::boolValue(Value *self, BasicBlock *block)
{
	ensure();
	return type->boolValue(self, block);
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

types::Type *types::GenericType::magicOut(const std::string& name, std::vector<types::Type *> args)
{
	ensure();
	return type->magicOut(name, args);
}

Value *types::GenericType::callMagic(const std::string& name,
                                     std::vector<types::Type *> argTypes,
                                     Value *self,
                                     std::vector<Value *> args,
                                     BasicBlock *block)
{
	ensure();
	return type->callMagic(name, argTypes, self, args, block);
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

Type *types::GenericType::getLLVMType(LLVMContext& context) const
{
	ensure();
	return type->getLLVMType(context);
}

size_t types::GenericType::size(Module *module) const
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

static bool findInTypeHelper(types::GenericType *gen,
                             types::Type *type,
                             std::vector<unsigned>& path,
                             std::vector<types::Type *>& seen)
{
	if (type->is(gen))
		return true;

	for (auto *saw : seen) {
		if (saw == type)
			return false;
	}

	const unsigned numBases = type->numBaseTypes();
	for (unsigned i = 0; i < numBases; i++) {
		path.push_back(i);
		seen.push_back(type);
		if (findInTypeHelper(gen, type->getBaseType(i), path, seen))
			return true;
		path.pop_back();
		seen.pop_back();
	}

	return false;
}

bool types::GenericType::findInType(types::Type *type, std::vector<unsigned>& path)
{
	std::vector<types::Type *> seen;
	return findInTypeHelper(this, type, path, seen);
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
	assert(numGenerics() > 0 && unrealized());

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
