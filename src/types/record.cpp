#include "seq/func.h"
#include "seq/base.h"
#include "seq/record.h"

using namespace seq;
using namespace llvm;

static std::string getNameFromTypes(std::vector<types::Type *> types)
{
	std::string name;

	for (auto *type : types)
		name += type->getName();

	name += "Record";
	return name;
}

types::RecordType::RecordType(std::vector<Type *> types, std::vector<std::string> names) :
    Type(getNameFromTypes(types), BaseType::get(), SeqData::RECORD),
    types(std::move(types)), names(std::move(names))
{
	if (!this->names.empty() && this->names.size() != this->types.size())
		throw exc::SeqException("type and name vectors differ in length");
}

types::RecordType::RecordType(std::initializer_list<Type *> types) :
    Type(getNameFromTypes(types), BaseType::get(), SeqData::RECORD),
    types(types), names()
{
}

void types::RecordType::serialize(BaseFunc *base,
                                  Value *self,
                                  Value *fp,
                                  BasicBlock *block)
{
	IRBuilder<> builder(block);

	for (int i = 0; i < types.size(); i++) {
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

	for (int i = 0; i < types.size(); i++) {
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

	for (int i = 0; i < types.size(); i++) {
		Value *elem = types[i]->defaultValue(block);
		IRBuilder<> builder(block);
		self = builder.CreateInsertValue(self, elem, i);
	}

	return self;
}

void types::RecordType::initFields()
{
	if (!vtable.fields.empty())
		return;

	assert(names.empty() || names.size() == types.size());

	for (int i = 0; i < types.size(); i++) {
		vtable.fields.insert({std::to_string(i+1), {i, types[i]}});

		if (!names.empty() && !names[i].empty())
			vtable.fields.insert({names[i], {i, types[i]}});
	}
}

bool types::RecordType::isGeneric(Type *type) const
{
	return dynamic_cast<types::RecordType *>(type) != nullptr;
}

types::Type *types::RecordType::getBaseType(seq_int_t idx) const
{
	if (idx < 1 || idx > (seq_int_t)types.size())
		throw exc::SeqException("invalid index into Record (must be constant and in-bounds)");

	return types[idx - 1];
}

Type *types::RecordType::getLLVMType(LLVMContext& context) const
{
	std::vector<llvm::Type *> body;
	for (auto& type : types)
		body.push_back(type->getLLVMType(context));

	return StructType::get(context, body);
}

seq_int_t types::RecordType::size(Module *module) const
{
	std::unique_ptr<DataLayout> layout(new DataLayout(module));
	return layout->getTypeAllocSize(getLLVMType(module->getContext()));
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
