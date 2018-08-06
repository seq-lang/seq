#include "seq/seq.h"
#include "seq/patterns.h"

using namespace seq;
using namespace llvm;

Pattern::Pattern(types::Type *type) : type(type)
{
}

void Pattern::validate(types::Type *type)
{
	if (!type->is(this->type))
		throw exc::SeqException("pattern type mismatch: expected " + this->type->getName() +
		                        " but got " + type->getName());
}

bool Pattern::isCatchAll()
{
	return false;
}

Pattern *Pattern::clone(types::RefType *ref)
{
	return this;
}

Wildcard::Wildcard() :
    Pattern(&types::Any), var(new Cell())
{
}

void Wildcard::validate(types::Type *type)
{
}

bool Wildcard::isCatchAll()
{
	return true;
}

Wildcard *Wildcard::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Wildcard *)ref->getClone(this);

	auto *x = new Wildcard();
	ref->addClone(this, x);
	delete x->var;
	x->var = var->clone(ref);
	return x;
}

Cell *Wildcard::getVar()
{
	return var;
}

Value *Wildcard::codegen(BaseFunc *base,
                         types::Type *type,
                         Value *val,
                         BasicBlock*& block)
{
	LLVMContext& context = block->getContext();
	var->setType(type);
	var->store(base, val, block);
	return ConstantInt::get(IntegerType::getInt1Ty(context), 1);
}

BoundPattern::BoundPattern(Pattern *pattern) :
    Pattern(&types::Any), var(new Cell()), pattern(pattern)
{
}

void BoundPattern::validate(types::Type *type)
{
	pattern->validate(type);
}

bool BoundPattern::isCatchAll()
{
	return pattern->isCatchAll();
}

BoundPattern *BoundPattern::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (BoundPattern *)ref->getClone(this);

	auto *x = new BoundPattern(pattern->clone(ref));
	ref->addClone(this, x);
	delete x->var;
	x->var = var->clone(ref);
	return x;
}

Cell *BoundPattern::getVar()
{
	return var;
}

Value *BoundPattern::codegen(BaseFunc *base,
                             types::Type *type,
                             Value *val,
                             BasicBlock*& block)
{
	var->setType(type);
	var->store(base, val, block);
	return pattern->codegen(base, type, val, block);
}

StarPattern::StarPattern() : Pattern(&types::Any)
{
}

void StarPattern::validate(types::Type *type)
{
}

Value *StarPattern::codegen(BaseFunc *base,
                            types::Type *type,
                            Value *val,
                            BasicBlock*& block)
{
	assert(0);
	return nullptr;
}

IntPattern::IntPattern(seq_int_t val) :
    Pattern(&types::Int), val(val)
{
}

BoolPattern::BoolPattern(bool val) :
    Pattern(&types::Bool), val(val)
{
}

StrPattern::StrPattern(std::string val) :
    Pattern(&types::Str), val(std::move(val))
{
}

Value *IntPattern::codegen(BaseFunc *base,
                           types::Type *type,
                           Value *val,
                           BasicBlock*& block)
{
	validate(type);
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);
	Value *pat = ConstantInt::get(types::Int.getLLVMType(context), (uint64_t)this->val, true);
	return builder.CreateICmpEQ(val, pat);
}

Value *BoolPattern::codegen(BaseFunc *base,
                            types::Type *type,
                            Value *val,
                            BasicBlock*& block)
{
	validate(type);
	LLVMContext& context = block->getContext();
	IRBuilder<> builder(block);
	Value *pat = ConstantInt::get(types::Bool.getLLVMType(context), (uint64_t)this->val);
	return builder.CreateFCmpOEQ(val, pat);
}

Value *StrPattern::codegen(BaseFunc *base,
                           types::Type *type,
                           Value *val,
                           BasicBlock*& block)
{
	validate(type);
	Value *pat = StrExpr(this->val).codegen(base, block);
	return types::Str.eq(base, val, pat, block);
}

RecordPattern::RecordPattern(std::vector<Pattern *> patterns) :
    Pattern(&types::Any), patterns(std::move(patterns))
{
}

void RecordPattern::validate(types::Type *type)
{
	auto *rec = dynamic_cast<types::RecordType *>(type);

	if (!rec)
		throw exc::SeqException("cannot match record pattern with non-record value");

	std::vector<types::Type *> types = rec->getTypes();

	if (types.size() != patterns.size())
		throw exc::SeqException("record element count mismatch in pattern");

	for (unsigned i = 0; i < types.size(); i++)
		patterns[i]->validate(types[i]);
}

Value *RecordPattern::codegen(BaseFunc *base,
                              types::Type *type,
                              Value *val,
                              BasicBlock*& block)
{
	validate(type);
	LLVMContext& context = block->getContext();
	Value *result = ConstantInt::get(IntegerType::getInt1Ty(context), 1);

	for (unsigned i = 0; i < patterns.size(); i++) {
		std::string m = std::to_string(i+1);
		Value *sub = type->memb(val, m, block);
		Value *subRes = patterns[i]->codegen(base, type->membType(m), sub, block);
		IRBuilder<> builder(block);
		result = builder.CreateAnd(result, subRes);
	}

	return result;
}

bool RecordPattern::isCatchAll()
{
	for (auto *pattern : patterns) {
		if (!pattern->isCatchAll())
			return false;
	}
	return true;
}

RecordPattern *RecordPattern::clone(types::RefType *ref)
{
	std::vector<Pattern *> patternsCloned;
	for (auto *elem : patterns)
		patternsCloned.push_back(elem->clone(ref));
	return new RecordPattern(patternsCloned);
}

ArrayPattern::ArrayPattern(std::vector<Pattern *> patterns) :
    Pattern(&types::Any), patterns(std::move(patterns))
{
}

void ArrayPattern::validate(types::Type *type)
{
	if (!type->isGeneric(&types::Array))
		throw exc::SeqException("cannot match array pattern with non-array value");

	types::Type *baseType = type->getBaseType(0);
	for (auto *pattern : patterns)
		pattern->validate(baseType);
}

Value *ArrayPattern::codegen(BaseFunc *base,
                             types::Type *type,
                             Value *val,
                             BasicBlock*& block)
{
	validate(type);
	LLVMContext& context = block->getContext();

	bool hasStar = false;
	unsigned star = 0;

	for (unsigned i = 0; i < patterns.size(); i++) {
		if (dynamic_cast<StarPattern *>(patterns[i])) {
			assert(!hasStar);
			star = i;
			hasStar = true;
		}
	}

	Value *len = type->memb(val, "len", block);
	Value *lenMatch = nullptr;
	BasicBlock *startBlock = block;
	IRBuilder<> builder(block);

	if (hasStar) {
		Value *minLen = ConstantInt::get(seqIntLLVM(context), patterns.size() - 1);
		lenMatch = builder.CreateICmpSGE(len, minLen);
	} else {
		Value *expectedLen = ConstantInt::get(seqIntLLVM(context), patterns.size());
		lenMatch = builder.CreateICmpEQ(len, expectedLen);
	}

	block = BasicBlock::Create(context, "", block->getParent());  // block for checking array contents
	BranchInst *branch = builder.CreateCondBr(lenMatch, block, block);

	builder.SetInsertPoint(block);
	Value *result = ConstantInt::get(IntegerType::getInt1Ty(context), 1);

	if (hasStar) {
		for (unsigned i = 0; i < star; i++) {
			Value *idx = ConstantInt::get(seqIntLLVM(context), i);
			Value *sub = type->indexLoad(base, val, idx, block);
			Value *subRes = patterns[i]->codegen(base, type->getBaseType(0), sub, block);
			builder.SetInsertPoint(block);  // recall that pattern codegen can change the block
			result = builder.CreateAnd(result, subRes);
		}

		for (unsigned i = star + 1; i < patterns.size(); i++) {
			Value *idx = ConstantInt::get(seqIntLLVM(context), i);
			idx = builder.CreateAdd(idx, len);
			idx = builder.CreateSub(idx, ConstantInt::get(seqIntLLVM(context), patterns.size()));

			Value *sub = type->indexLoad(base, val, idx, block);
			Value *subRes = patterns[i]->codegen(base, type->getBaseType(0), sub, block);
			builder.SetInsertPoint(block);  // recall that pattern codegen can change the block
			result = builder.CreateAnd(result, subRes);
		}
	} else {
		for (unsigned i = 0; i < patterns.size(); i++) {
			Value *idx = ConstantInt::get(seqIntLLVM(context), i);
			Value *sub = type->indexLoad(base, val, idx, block);
			Value *subRes = patterns[i]->codegen(base, type->getBaseType(0), sub, block);
			builder.SetInsertPoint(block);  // recall that pattern codegen can change the block
			result = builder.CreateAnd(result, subRes);
		}
	}

	BasicBlock *checkBlock = block;

	block = BasicBlock::Create(context, "", block->getParent());  // final result block
	builder.CreateBr(block);
	branch->setSuccessor(1, block);

	builder.SetInsertPoint(block);
	PHINode *resultFinal = builder.CreatePHI(IntegerType::getInt1Ty(context), 2);
	resultFinal->addIncoming(ConstantInt::get(IntegerType::getInt1Ty(context), 0), startBlock);  // length didn't match
	resultFinal->addIncoming(result, checkBlock);  // result of checking array elements

	return resultFinal;
}

ArrayPattern *ArrayPattern::clone(types::RefType *ref)
{
	std::vector<Pattern *> patternsCloned;
	for (auto *elem : patterns)
		patternsCloned.push_back(elem->clone(ref));
	return new ArrayPattern(patternsCloned);
}

SeqPattern::SeqPattern(std::string pattern) :
    Pattern(&types::Seq), pattern(std::move(pattern))
{
}

Value *SeqPattern::codegen(BaseFunc *base,
                           types::Type *type,
                           Value *val,
                           BasicBlock*& block)
{
	validate(type);
	LLVMContext& context = block->getContext();

	std::vector<char> patterns;

	bool hasStar = false;
	unsigned star = 0;

	for (char c : pattern) {
		if (isspace(c))
			continue;

		switch (c) {
			case 'A':
			case 'C':
			case 'G':
			case 'T':
			case 'N':
			case '_':
				patterns.push_back(c);
				break;
			case '.':
				if (!hasStar) {
					star = (unsigned)patterns.size();
					hasStar = true;
					patterns.push_back('\0');
				}
				break;
			default:
				assert(0);
		}
	}

	Value *ptr = type->memb(val, "ptr", block);
	Value *len = type->memb(val, "len", block);
	Value *lenMatch = nullptr;
	BasicBlock *startBlock = block;
	IRBuilder<> builder(block);

	if (hasStar) {
		Value *minLen = ConstantInt::get(seqIntLLVM(context), patterns.size() - 1);
		lenMatch = builder.CreateICmpSGE(len, minLen);
	} else {
		Value *expectedLen = ConstantInt::get(seqIntLLVM(context), patterns.size());
		lenMatch = builder.CreateICmpEQ(len, expectedLen);
	}

	block = BasicBlock::Create(context, "", block->getParent());  // block for checking array contents
	BranchInst *branch = builder.CreateCondBr(lenMatch, block, block);

	builder.SetInsertPoint(block);
	Value *result = ConstantInt::get(IntegerType::getInt1Ty(context), 1);

	if (hasStar) {
		for (unsigned i = 0; i < star; i++) {
			if (patterns[i] == '_') continue;
			Value *idx = ConstantInt::get(seqIntLLVM(context), i);
			Value *sub = builder.CreateLoad(builder.CreateGEP(ptr, idx));
			Value *c = ConstantInt::get(IntegerType::getInt8Ty(context), (uint64_t)patterns[i]);
			Value *subRes = builder.CreateICmpEQ(sub, c);
			builder.SetInsertPoint(block);  // recall that pattern codegen can change the block
			result = builder.CreateAnd(result, subRes);
		}

		for (unsigned i = star + 1; i < patterns.size(); i++) {
			if (patterns[i] == '_') continue;
			Value *idx = ConstantInt::get(seqIntLLVM(context), i);
			idx = builder.CreateAdd(idx, len);
			idx = builder.CreateSub(idx, ConstantInt::get(seqIntLLVM(context), patterns.size()));

			Value *sub = builder.CreateLoad(builder.CreateGEP(ptr, idx));
			Value *c = ConstantInt::get(IntegerType::getInt8Ty(context), (uint64_t)patterns[i]);
			Value *subRes = builder.CreateICmpEQ(sub, c);
			builder.SetInsertPoint(block);  // recall that pattern codegen can change the block
			result = builder.CreateAnd(result, subRes);
		}
	} else {
		for (unsigned i = 0; i < patterns.size(); i++) {
			if (patterns[i] == '_') continue;
			Value *idx = ConstantInt::get(seqIntLLVM(context), i);
			Value *sub = builder.CreateLoad(builder.CreateGEP(ptr, idx));
			Value *c = ConstantInt::get(IntegerType::getInt8Ty(context), (uint64_t)patterns[i]);
			Value *subRes = builder.CreateICmpEQ(sub, c);
			builder.SetInsertPoint(block);  // recall that pattern codegen can change the block
			result = builder.CreateAnd(result, subRes);
		}
	}

	BasicBlock *checkBlock = block;

	block = BasicBlock::Create(context, "", block->getParent());  // final result block
	builder.CreateBr(block);
	branch->setSuccessor(1, block);

	builder.SetInsertPoint(block);
	PHINode *resultFinal = builder.CreatePHI(IntegerType::getInt1Ty(context), 2);
	resultFinal->addIncoming(ConstantInt::get(IntegerType::getInt1Ty(context), 0), startBlock);  // length didn't match
	resultFinal->addIncoming(result, checkBlock);  // result of checking array elements

	return resultFinal;
}

OptPattern::OptPattern(Pattern *pattern) :
    Pattern(&types::Any), pattern(pattern)
{
}

void OptPattern::validate(types::Type *type)
{
	auto *optType = dynamic_cast<types::OptionalType *>(type);
	if (!optType)
		throw exc::SeqException("cannot match optional pattern against non-optional value");

	if (pattern)
		pattern->validate(optType->getBaseType(0));
}

Value *OptPattern::codegen(BaseFunc *base,
                           types::Type *type,
                           Value *val,
                           BasicBlock*& block)
{
	validate(type);
	LLVMContext& context = block->getContext();

	auto *optType = dynamic_cast<types::OptionalType *>(type);
	assert(optType);

	if (!pattern) {  // no pattern means we're matching the empty optional pattern
		Value *has = optType->has(val, block);
		IRBuilder<> builder(block);
		return builder.CreateNot(has);
	}

	Value *hasResult = optType->has(val, block);
	BasicBlock *startBlock = block;
	IRBuilder<> builder(block);
	block = BasicBlock::Create(context, "", block->getParent());  // pattern eval block
	BranchInst *branch = builder.CreateCondBr(hasResult, block, block);

	Value *had = optType->val(val, block);
	Value *patternResult = pattern->codegen(base, optType->getBaseType(0), had, block);
	BasicBlock *checkBlock = block;
	builder.SetInsertPoint(block);

	block = BasicBlock::Create(context, "", block->getParent());  // final result block
	builder.CreateBr(block);
	branch->setSuccessor(1, block);

	builder.SetInsertPoint(block);
	PHINode *resultFinal = builder.CreatePHI(IntegerType::getInt1Ty(context), 2);
	resultFinal->addIncoming(ConstantInt::get(IntegerType::getInt1Ty(context), 0), startBlock);  // no value
	resultFinal->addIncoming(patternResult, checkBlock);  // result of pattern match

	return resultFinal;
}

OptPattern *OptPattern::clone(types::RefType *ref)
{
	return new OptPattern(pattern->clone(ref));
}

RangePattern::RangePattern(seq_int_t a, seq_int_t b) :
    Pattern(&types::Int), a(a), b(b)
{
}

Value *RangePattern::codegen(BaseFunc *base,
                             types::Type *type,
                             Value *val,
                             BasicBlock*& block)
{
	validate(type);
	LLVMContext& context = block->getContext();

	Value *a = ConstantInt::get(seqIntLLVM(context), (uint64_t)this->a, true);
	Value *b = ConstantInt::get(seqIntLLVM(context), (uint64_t)this->b, true);

	IRBuilder<> builder(block);
	Value *c1 = builder.CreateICmpSLE(a, val);
	Value *c2 = builder.CreateICmpSLE(val, b);
	return builder.CreateAnd(c1, c2);
}

OrPattern::OrPattern(std::vector<Pattern *> patterns) :
    Pattern(&types::Any), patterns(std::move(patterns))
{
}

void OrPattern::validate(types::Type *type)
{
	for (auto *pattern : patterns)
		pattern->validate(type);
}

Value *OrPattern::codegen(BaseFunc *base,
                          types::Type *type,
                          Value *val,
                          BasicBlock*& block)
{
	validate(type);
	LLVMContext& context = block->getContext();
	Value *result = ConstantInt::get(IntegerType::getInt1Ty(context), 0);

	for (auto *pattern : patterns) {
		Value *subRes = pattern->codegen(base, type, val, block);
		IRBuilder<> builder(block);
		result = builder.CreateOr(result, subRes);
	}

	return result;
}

bool OrPattern::isCatchAll()
{
	for (auto *pattern : patterns) {
		if (pattern->isCatchAll())
			return true;
	}
	return false;
}

OrPattern *OrPattern::clone(types::RefType *ref)
{
	std::vector<Pattern *> patternsCloned;
	for (auto *elem : patterns)
		patternsCloned.push_back(elem->clone(ref));
	return new OrPattern(patternsCloned);
}

GuardedPattern::GuardedPattern(Pattern *pattern, Expr *guard) :
    Pattern(&types::Int), pattern(pattern), guard(guard)
{
}

void GuardedPattern::validate(types::Type *type)
{
	pattern->validate(type);
}

Value *GuardedPattern::codegen(BaseFunc *base,
                               types::Type *type,
                               Value *val,
                               BasicBlock*& block)
{
	validate(type);
	LLVMContext& context = block->getContext();

	Value *patternResult = pattern->codegen(base, type, val, block);
	BasicBlock *startBlock = block;
	IRBuilder<> builder(block);
	block = BasicBlock::Create(context, "", block->getParent());  // guard eval block
	BranchInst *branch = builder.CreateCondBr(patternResult, block, block);

	guard->ensure(&types::Bool);
	Value *guardResult = guard->codegen(base, block);
	BasicBlock *checkBlock = block;
	builder.SetInsertPoint(block);
	guardResult = builder.CreateTrunc(guardResult, IntegerType::getInt1Ty(context));

	block = BasicBlock::Create(context, "", block->getParent());  // final result block
	builder.CreateBr(block);
	branch->setSuccessor(1, block);

	builder.SetInsertPoint(block);
	PHINode *resultFinal = builder.CreatePHI(IntegerType::getInt1Ty(context), 2);
	resultFinal->addIncoming(ConstantInt::get(IntegerType::getInt1Ty(context), 0), startBlock);  // pattern didn't match
	resultFinal->addIncoming(guardResult, checkBlock);  // result of guard

	return resultFinal;
}

GuardedPattern *GuardedPattern::clone(types::RefType *ref)
{
	return new GuardedPattern(pattern->clone(ref), guard->clone(ref));
}
