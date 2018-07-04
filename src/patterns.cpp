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

Pattern *Pattern::clone()
{
	return this;
}

Wildcard::Wildcard() :
    Pattern(&types::Any), var(new Var(true)), result(nullptr)
{
}

void Wildcard::validate(types::Type *type)
{
}

Wildcard *Wildcard::clone()
{
	return new Wildcard();
}

Var *Wildcard::getVar()
{
	return var;
}

Value *Wildcard::codegen(BaseFunc *base,
                         types::Type *type,
                         Value *val,
                         BasicBlock*& block)
{
	LLVMContext& context = block->getContext();
	result = type->storeInAlloca(base, val, block, true);
	auto& p = BaseStage::make(&types::Any, type);
	p.setBase(base);
	p.result = result;
	*var = p;
	return ConstantInt::get(IntegerType::getInt1Ty(context), 1);
}

BoundPattern::BoundPattern(Pattern *pattern) :
    Pattern(&types::Any), var(new Var(true)), result(nullptr), pattern(pattern)
{
}

void BoundPattern::validate(types::Type *type)
{
	pattern->validate(type);
}

BoundPattern *BoundPattern::clone()
{
	return new BoundPattern(pattern->clone());
}

Var *BoundPattern::getVar()
{
	return var;
}

Value *BoundPattern::codegen(BaseFunc *base,
                             types::Type *type,
                             Value *val,
                             BasicBlock*& block)
{
	result = type->storeInAlloca(base, val, block, true);
	auto& p = BaseStage::make(&types::Any, type);
	p.setBase(base);
	p.result = result;
	*var = p;
	return pattern->codegen(base, type, val, block);
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

RecordPattern *RecordPattern::clone()
{
	std::vector<Pattern *> patternsCloned;
	for (auto *elem : patterns)
		patternsCloned.push_back(elem->clone());
	return new RecordPattern(patternsCloned);
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

OrPattern *OrPattern::clone()
{
	std::vector<Pattern *> patternsCloned;
	for (auto *elem : patterns)
		patternsCloned.push_back(elem->clone());
	return new OrPattern(patternsCloned);
}
