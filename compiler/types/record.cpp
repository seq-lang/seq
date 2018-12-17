#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::RecordType::RecordType(std::vector<Type *> types, std::vector<std::string> names, std::string name) :
    Type(std::move(name), BaseType::get()), types(std::move(types)), names(std::move(names))
{
	assert(this->names.empty() || this->names.size() == this->types.size());
}

bool types::RecordType::empty() const
{
	return types.empty();
}

std::vector<types::Type *> types::RecordType::getTypes()
{
	return types;
}

std::string types::RecordType::getName() const
{
	if (!name.empty())
		return name;

	std::string name = "(";

	for (unsigned i = 0; i < types.size(); i++) {
		name += types[i]->getName();
		if (i < types.size() - 1)
			name += ",";
	}

	name += ")";
	return name;
}

Value *types::RecordType::defaultValue(BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	Value *self = UndefValue::get(getLLVMType(context));

	for (unsigned i = 0; i < types.size(); i++) {
		Value *elem = types[i]->defaultValue(block);
		IRBuilder<> builder(block);
		self = builder.CreateInsertValue(self, elem, i);
	}

	return self;
}

bool types::RecordType::isAtomic() const
{
	for (auto *type : types) {
		if (!type->isAtomic())
			return false;
	}
	return true;
}

bool types::RecordType::is(types::Type *type) const
{
	unsigned b = numBaseTypes();

	if (!isGeneric(type) || b != type->numBaseTypes())
		return false;

	for (unsigned i = 0; i < b; i++) {
		if (!types::is(getBaseType(i), type->getBaseType(i)))
			return false;
	}

	return true;
}

void types::RecordType::initOps()
{
	if (!vtable.magic.empty())
		return;

	vtable.magic = {
		{"__init__", types, this, SEQ_MAGIC_CAPT(self, args, b) {
			Value *val = defaultValue(b.GetInsertBlock());
			for (unsigned i = 0; i < args.size(); i++)
				val = setMemb(val, std::to_string(i+1), args[i], b.GetInsertBlock());
			return val;
		}},
	};
}

void types::RecordType::initFields()
{
	if (!getVTable().fields.empty())
		return;

	assert(names.empty() || names.size() == types.size());

	for (unsigned i = 0; i < types.size(); i++) {
		getVTable().fields.insert({std::to_string(i+1), {i, types[i]}});

		if (!names.empty() && !names[i].empty())
			getVTable().fields.insert({names[i], {i, types[i]}});
	}
}

unsigned types::RecordType::numBaseTypes() const
{
	return (unsigned)types.size();
}

types::Type *types::RecordType::getBaseType(unsigned idx) const
{
	return types[idx];
}

Type *types::RecordType::getLLVMType(LLVMContext& context) const
{
	std::vector<llvm::Type *> body;
	for (auto& type : types)
		body.push_back(type->getLLVMType(context));

	return StructType::get(context, body);
}

void types::RecordType::addLLVMTypesToStruct(StructType *structType)
{
	std::vector<llvm::Type *> body;
	for (auto& type : types)
		body.push_back(type->getLLVMType(structType->getContext()));
	structType->setBody(body);
}

size_t types::RecordType::size(Module *module) const
{
	return module->getDataLayout().getTypeAllocSize(getLLVMType(module->getContext()));
}

types::RecordType *types::RecordType::asRec()
{
	return this;
}

types::RecordType *types::RecordType::clone(Generic *ref)
{
	if (ref->seenClone(this))
		return (types::RecordType *)ref->getClone(this);

	auto *x = types::RecordType::get({}, {});
	ref->addClone(this, x);

	std::vector<Type *> typesCloned;
	for (auto *type : types)
		typesCloned.push_back(type->clone(ref));

	std::vector<MagicOverload> overloadsCloned;
	for (auto& magic : getVTable().overloads)
		overloadsCloned.push_back({magic.name, magic.func->clone(ref)});

	std::map<std::string, BaseFunc *> methodsCloned;
	for (auto& method : getVTable().methods)
		methodsCloned.insert({method.first, method.second->clone(ref)});

	x->types = typesCloned;
	x->names = names;
	x->getVTable().overloads = overloadsCloned;
	x->getVTable().methods = methodsCloned;
	return x;
}

types::RecordType *types::RecordType::get(std::vector<Type *> types, std::vector<std::string> names, std::string name)
{
	return new RecordType(std::move(types), std::move(names), std::move(name));
}
