#include "seq/seq.h"

using namespace seq;
using namespace llvm;

static std::string getNameFromTypes(std::vector<types::Type *> types)
{
	std::string name = "(";

	for (auto *type : types) {
		name += type->getName();
		if (type != types.back())
			name += ", ";
	}

	name += ")";
	return name;
}

types::RecordType::RecordType(std::vector<Type *> types, std::vector<std::string> names) :
    Type(getNameFromTypes(types), BaseType::get()),
    types(std::move(types)), names(std::move(names))
{
	assert(this->names.empty() || this->names.size() == this->types.size());
}

types::RecordType::RecordType(std::initializer_list<Type *> types) :
    Type(getNameFromTypes(types), BaseType::get()),
    types(types), names()
{
}

bool types::RecordType::empty() const
{
	return types.empty();
}

std::vector<types::Type *> types::RecordType::getTypes()
{
	return types;
}

void types::RecordType::serialize(BaseFunc *base,
                                  Value *self,
                                  Value *fp,
                                  BasicBlock *block)
{
	IRBuilder<> builder(block);

	for (unsigned i = 0; i < types.size(); i++) {
		Value *elem = builder.CreateExtractValue(self, i);
		types[i]->serialize(base, elem, fp, block);
	}
}

Value *types::RecordType::deserialize(BaseFunc *base,
                                      Value *fp,
                                      BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);
	Value *self = UndefValue::get(getLLVMType(context));

	for (unsigned i = 0; i < types.size(); i++) {
		Value *elem = types[i]->deserialize(base, fp, block);
		self = builder.CreateInsertValue(self, elem, i);
	}

	return self;
}

static seq_int_t getIdxSafe(Value *idx, const seq_int_t max)
{
	if (auto *constIdx = dyn_cast<ConstantInt>(idx)) {
		const seq_int_t idxReal = constIdx->getSExtValue();

		if (idxReal < 1 || idxReal > max)
			throw exc::SeqException("index into record out of bounds");

		return idxReal - 1;  // 1-based to 0-based
	} else {
		throw exc::SeqException("index into record must be constant");
	}
}

Value *types::RecordType::indexLoad(BaseFunc *base,
                                    Value *self,
                                    Value *idx,
                                    BasicBlock *block)
{
	const seq_int_t idxReal = getIdxSafe(idx, (seq_int_t)types.size());
	Type *type = types[idxReal];

	LLVMContext& context = base->getContext();
	IRBuilder<> builder(block);

	Value *recPtr = storeInAlloca(base, self, block);
	Value *elemPtr = builder.CreateGEP(recPtr,
	                                   {ConstantInt::get(IntegerType::getInt32Ty(context), 0),
	                                    ConstantInt::get(IntegerType::getInt32Ty(context), (uint64_t)idxReal)});
	return type->load(base, elemPtr, zeroLLVM(context), block);
}

void types::RecordType::indexStore(BaseFunc *base,
                                   Value *self,
                                   Value *idx,
                                   Value *val,
                                   BasicBlock *block)
{
	const seq_int_t idxReal = getIdxSafe(idx, (seq_int_t)types.size());
	Type *type = types[idxReal];

	LLVMContext& context = base->getContext();
	IRBuilder<> builder(block);

	Value *recPtr = storeInAlloca(base, self, block);
	Value *elemPtr = builder.CreateGEP(recPtr,
	                                   {ConstantInt::get(IntegerType::getInt32Ty(context), 0),
	                                    ConstantInt::get(IntegerType::getInt32Ty(context), (uint64_t)idxReal)});
	type->store(base, val, elemPtr, zeroLLVM(context), block);
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


Value *types::RecordType::construct(BaseFunc *base,
                                    const std::vector<Value *>& args,
                                    BasicBlock *block)
{
	Value *val = defaultValue(block);
	for (unsigned i = 0; i < args.size(); i++)
		val = setMemb(val, std::to_string(i+1), args[i], block);
	return val;
}

bool types::RecordType::isAtomic() const
{
	for (auto *type : types) {
		if (!type->isAtomic())
			return false;
	}
	return true;
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

types::Type *types::RecordType::getBaseType(seq_int_t idx) const
{
	if (idx < 1 || idx > (seq_int_t)types.size())
		throw exc::SeqException("invalid index into record (must be constant and in-bounds)");

	return types[idx - 1];
}

types::Type *types::RecordType::getConstructType(const std::vector<Type *>& inTypes)
{
	if (inTypes.size() != types.size())
		throw exc::SeqException("expected " + std::to_string(types.size()) + " arguments, " +
		                        "but got " + std::to_string(inTypes.size()));

	for (unsigned i = 0; i < inTypes.size(); i++) {
		if (!types::is(inTypes[i], types[i]))
			throw exc::SeqException("expected " + types[i]->getName() +
			                        ", but got " + inTypes[i]->getName());
	}

	return this;
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

seq_int_t types::RecordType::size(Module *module) const
{
	return module->getDataLayout().getTypeAllocSize(getLLVMType(module->getContext()));
}

types::RecordType& types::RecordType::of(std::initializer_list<std::reference_wrapper<Type>> types) const
{
	std::vector<Type *> typesPtr;
	for (auto& type : types)
		typesPtr.push_back(&type.get());

	return *RecordType::get(typesPtr);
}

types::RecordType *types::RecordType::get(std::vector<Type *> types, std::vector<std::string> names)
{
	return new RecordType(std::move(types), std::move(names));
}

types::RecordType *types::RecordType::get(std::initializer_list<Type *> types)
{
	return new RecordType(types);
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

	x->types = typesCloned;
	x->names = names;
	x->name = getNameFromTypes(typesCloned);
	return x;
}
