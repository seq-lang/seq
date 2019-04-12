#include "seq/seq.h"

using namespace seq;
using namespace llvm;

types::RecordType::RecordType(std::vector<Type *> types, std::vector<std::string> names, std::string name) :
    Type(name, BaseType::get(), false, !name.empty()), types(std::move(types)), names(std::move(names))
{
	assert(this->names.empty() || this->names.size() == this->types.size());
}

void types::RecordType::setContents(std::vector<Type *> types, std::vector<std::string> names)
{
	this->types = std::move(types);
	this->names = std::move(names);
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

	std::string name = "tuple[";

	for (unsigned i = 0; i < types.size(); i++) {
		name += types[i]->getName();
		if (i < types.size() - 1)
			name += ",";
	}

	name += "]";
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

	if (!name.empty()) {
		auto *rec = dynamic_cast<types::RecordType *>(type);
		assert(rec);
		if (name != rec->name)
			return false;
	}

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

		{"__str__", {}, Str, SEQ_MAGIC_CAPT(self, args, b) {
			LLVMContext& context = b.getContext();
			BasicBlock *block = b.GetInsertBlock();
			Module *module = block->getModule();
			const std::string strName = "seq." + getName() + ".__str__";
			Function *str = module->getFunction(strName);

			if (!str) {
				str = cast<Function>(module->getOrInsertFunction(strName,
				                                                 Str->getLLVMType(context),
				                                                 getLLVMType(context)));
				str->setLinkage(GlobalValue::PrivateLinkage);
				str->setPersonalityFn(makePersonalityFunc(module));

				Value *arg = str->arg_begin();
				BasicBlock *entry = BasicBlock::Create(context, "entry", str);
				b.SetInsertPoint(entry);
				Value *len = ConstantInt::get(seqIntLLVM(context), types.size());
				Value *strs = b.CreateAlloca(Str->getLLVMType(context), len);

				for (unsigned i = 0; i < types.size(); i++) {
					Value *v = memb(arg, std::to_string(i+1), entry);
					// won't create new block since no try-catch:
					Value *s = types[i]->strValue(v, entry, nullptr);
					Value *dest = b.CreateGEP(strs, b.getInt32(i));
					b.CreateStore(s, dest);
				};

				auto *strReal = cast<Function>(
				                  module->getOrInsertFunction(
				                    "seq_str_tuple",
				                    Str->getLLVMType(context),
				                    Str->getLLVMType(context)->getPointerTo(),
				                    seqIntLLVM(context)));
				strReal->setDoesNotThrow();
				Value *res = b.CreateCall(strReal, {strs, len});
				b.CreateRet(res);
			}

			b.SetInsertPoint(block);
			return b.CreateCall(str, self);
		}},

		{"__iter__", {}, GenType::get(types.empty() ? Void : types[0]), SEQ_MAGIC_CAPT(self, args, b) {
			if (types.empty())
				throw exc::SeqException("cannot iterate over empty tuple");

			for (auto *type : types) {
				if (!types::is(type, types[0]))
					throw exc::SeqException("cannot iterate over heterogeneous tuple");
			}

			BasicBlock *block = b.GetInsertBlock();
			Module *module = block->getModule();
			const std::string iterName = "seq." + getName() + ".__iter__";
			Function *iter = module->getFunction(iterName);

			if (!iter) {
				Func iterFunc;
				iterFunc.setName(iterName);
				iterFunc.setIns({this});
				iterFunc.setOut(types[0]);
				iterFunc.setArgNames({"self"});

				VarExpr arg(iterFunc.getArgVar("self"));
				Block *body = iterFunc.getBlock();
				for (unsigned i = 0; i < types.size(); i++) {
					auto *yield = new Yield(new GetElemExpr(&arg, i + 1));
					yield->setBase(&iterFunc);
					body->add(yield);
					if (i == 0)
						iterFunc.sawYield(yield);
				}

				iterFunc.resolveTypes();
				iterFunc.codegen(module);
				iter = iterFunc.getFunc();
			}

			return b.CreateCall(iter, self);
		}},

		{"__len__", {}, Int, SEQ_MAGIC_CAPT(self, args, b) {
			return ConstantInt::get(seqIntLLVM(b.getContext()), types.size(), true);
		}},

		{"__eq__", {this}, Bool, SEQ_MAGIC_CAPT(self, args, b) {
			BasicBlock *block = b.GetInsertBlock();
			Value *result = b.getInt8(1);
			for (unsigned i = 0; i < types.size(); i++) {
				Value *val1 = memb(self, std::to_string(i+1), block);
				Value *val2 = memb(args[0], std::to_string(i+1), block);
				types::Type *eqType = types[i]->magicOut("__eq__", {types[i]});
				Value *eq = types[i]->callMagic("__eq__", {types[i]}, val1, {val2}, block, nullptr);
				eq = eqType->boolValue(eq, block, nullptr);
				result = b.CreateAnd(result, eq);
			};
			return result;
		}},

		{"__hash__", {}, Int, SEQ_MAGIC_CAPT(self, args, b) {
			// hash_combine combine algorithm used in boost
			LLVMContext& context = b.getContext();
			BasicBlock *block = b.GetInsertBlock();
			Value *seed = zeroLLVM(context);
			Value *phi = ConstantInt::get(seqIntLLVM(context), 0x9e3779b9);
			for (unsigned i = 0; i < types.size(); i++) {
				Value *val = memb(self, std::to_string(i+1), block);
				if (!types[i]->magicOut("__hash__", {})->is(Int))
					throw exc::SeqException("__hash__ for type '" + types[i]->getName() + "' does return an 'int'");
				Value *hash = types[i]->callMagic("__hash__", {}, val, {}, block, nullptr);
				Value *p1 = b.CreateShl(seed, 6);
				Value *p2 = b.CreateLShr(seed, 2);
				hash = b.CreateAdd(hash, phi);
				hash = b.CreateAdd(hash, p1);
				hash = b.CreateAdd(hash, p2);
				seed = b.CreateXor(seed, hash);
			};
			return seed;
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

	auto *x = types::RecordType::get({}, {}, name);
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
