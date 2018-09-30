#include <queue>
#include "seq/seq.h"

using namespace seq;
using namespace llvm;

Expr::Expr(types::Type *type) : SrcObject(), type(type)
{
}

Expr::Expr() : Expr(types::Void)
{
}

Value *Expr::codegen(BaseFunc *base, BasicBlock *&block)
{
	SEQ_TRY(
		return codegen0(base, block);
	);
}

types::Type *Expr::getType() const
{
	SEQ_TRY(
		return getType0();
	);
}

void Expr::resolveTypes()
{
}

types::Type *Expr::getType0() const
{
	return type;
}

void Expr::ensure(types::Type *type)
{
	types::Type *actual = getType();
	if (!types::is(type, actual))
		throw exc::SeqException("expected '" + type->getName() + "', got '" + getType()->getName() + "'", getSrcInfo());
}

Expr *Expr::clone(Generic *ref)
{
	return this;
}

std::string Expr::getName() const
{
	return name;
}

BlankExpr::BlankExpr() : Expr(types::Void)
{
}

types::Type *BlankExpr::getType0() const
{
	throw exc::SeqException("misplaced '_'");
}

Value *BlankExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	throw exc::SeqException("misplaced '_'");
}

TypeExpr::TypeExpr(types::Type *type) : Expr(type)
{
	name = "type";
}

Value *TypeExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	throw exc::SeqException("misplaced type expression");
}

ValueExpr::ValueExpr(types::Type *type, Value *val) : Expr(type), val(val)
{
}

Value *ValueExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	return val;
}

IntExpr::IntExpr(seq_int_t n) : Expr(types::Int), n(n)
{
}

Value *IntExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	LLVMContext& context = block->getContext();
	return ConstantInt::get(getType()->getLLVMType(context), (uint64_t)n, true);
}

seq_int_t IntExpr::value() const
{
	return n;
}

FloatExpr::FloatExpr(double f) : Expr(types::Float), f(f)
{
}

Value *FloatExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	LLVMContext& context = block->getContext();
	return ConstantFP::get(getType()->getLLVMType(context), f);
}

BoolExpr::BoolExpr(bool b) : Expr(types::Bool), b(b)
{
}

Value *BoolExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	LLVMContext& context = block->getContext();
	return ConstantInt::get(getType()->getLLVMType(context), b ? 1 : 0);
}

StrExpr::StrExpr(std::string s) : Expr(types::Str), s(std::move(s))
{
}

Value *StrExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();
	BasicBlock *preambleBlock = base->getPreamble();

	GlobalVariable *strVar = new GlobalVariable(*module,
	                                            llvm::ArrayType::get(IntegerType::getInt8Ty(context), s.length() + 1),
	                                            true,
	                                            GlobalValue::PrivateLinkage,
	                                            ConstantDataArray::getString(context, s),
	                                            "str_literal");
	strVar->setAlignment(1);

	IRBuilder<> builder(preambleBlock);
	Value *str = builder.CreateBitCast(strVar, IntegerType::getInt8PtrTy(context));
	Value *len = ConstantInt::get(seqIntLLVM(context), s.length());
	return types::Str->make(str, len, preambleBlock);
}

SeqExpr::SeqExpr(std::string s) : Expr(types::Seq), s(std::move(s))
{
}

Value *SeqExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	LLVMContext& context = block->getContext();
	Module *module = block->getModule();
	BasicBlock *preambleBlock = base->getPreamble();

	GlobalVariable *seqVar = new GlobalVariable(*module,
	                                            llvm::ArrayType::get(IntegerType::getInt8Ty(context), s.length() + 1),
	                                            true,
	                                            GlobalValue::PrivateLinkage,
	                                            ConstantDataArray::getString(context, s),
	                                            "seq_literal");
	seqVar->setAlignment(1);

	IRBuilder<> builder(preambleBlock);
	Value *seq = builder.CreateBitCast(seqVar, IntegerType::getInt8PtrTy(context));
	Value *len = ConstantInt::get(seqIntLLVM(context), s.length());
	return types::Seq->make(seq, len, preambleBlock);
}

VarExpr::VarExpr(Var *var) : var(var)
{
}

Value *VarExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	return var->load(base, block);
}

types::Type *VarExpr::getType0() const
{
	return var->getType();
}

VarExpr *VarExpr::clone(Generic *ref)
{
	return new VarExpr(var->clone(ref));
}

FuncExpr::FuncExpr(BaseFunc *func, Expr *orig, std::vector<types::Type *> types) :
    func(func), types(std::move(types)), orig(orig)
{
	name = "func";
}

FuncExpr::FuncExpr(BaseFunc *func, std::vector<types::Type *> types) :
    FuncExpr(func, nullptr, std::move(types))
{
}

bool FuncExpr::isParameterized()
{
	return !types.empty();
}

BaseFunc *FuncExpr::getFunc()
{
	return func;
}

void FuncExpr::resolveTypes()
{
	auto *f = dynamic_cast<Func *>(func);
	if (f) {
		if (f->unrealized() && !types.empty()) {
			orig = new FuncExpr(func, types);
			func = f->realize(types);
		}
	} else if (!types.empty()) {
		throw exc::SeqException("cannot type-instantiate non-generic function");
	}

	func->resolveTypes();
}

Value *FuncExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	func->codegen(block->getModule());
	return func->getFunc();
}

types::Type *FuncExpr::getType0() const
{
	return func->getFuncType();
}

Expr *FuncExpr::clone(Generic *ref)
{
	if (orig)
		return orig->clone(ref);

	std::vector<types::Type *> typesCloned;
	for (auto *type : types)
		typesCloned.push_back(type->clone(ref));
	return new FuncExpr(func->clone(ref), typesCloned);
}

ArrayExpr::ArrayExpr(types::Type *type, Expr *count) :
    Expr(types::ArrayType::get(type)), count(count)
{
}

void ArrayExpr::resolveTypes()
{
	count->resolveTypes();
}

Value *ArrayExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	auto *type = dynamic_cast<types::ArrayType *>(getType());
	assert(type != nullptr);
	count->ensure(types::Int);

	Value *len = count->codegen(base, block);
	Value *ptr = type->indexType()->alloc(len, block);
	Value *arr = type->make(ptr, len, block);
	return arr;
}

ArrayExpr *ArrayExpr::clone(Generic *ref)
{
	return new ArrayExpr(getType()->clone(ref)->getBaseType(0), count->clone(ref));
}

RecordExpr::RecordExpr(std::vector<Expr *> exprs, std::vector<std::string> names) :
    exprs(std::move(exprs)), names(std::move(names))
{
}

void RecordExpr::resolveTypes()
{
	for (auto *expr : exprs)
		expr->resolveTypes();
}

Value *RecordExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	LLVMContext& context = block->getContext();
	types::Type *type = getType();
	Value *rec = UndefValue::get(type->getLLVMType(context));
	unsigned idx = 0;

	IRBuilder<> builder(block);
	for (auto *expr : exprs) {
		Value *val = expr->codegen(base, block);
		builder.SetInsertPoint(block);  // recall: 'codegen' can change the block
		rec = builder.CreateInsertValue(rec, val, idx++);
	}

	return rec;
}

types::Type *RecordExpr::getType0() const
{
	std::vector<types::Type *> types;
	for (auto *expr : exprs)
		types.push_back(expr->getType());
	return names.empty() ? types::RecordType::get(types) : types::RecordType::get(types, names);
}

RecordExpr *RecordExpr::clone(Generic *ref)
{
	std::vector<Expr *> exprsCloned;
	for (auto *expr : exprs)
		exprsCloned.push_back(expr->clone(ref));
	return new RecordExpr(exprsCloned, names);
}

UOpExpr::UOpExpr(Op op, Expr *lhs) :
    Expr(), op(std::move(op)), lhs(lhs)
{
}

void UOpExpr::resolveTypes()
{
	lhs->resolveTypes();
}

Value *UOpExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	auto spec = lhs->getType()->findUOp(op.symbol);
	Value *lhs = this->lhs->codegen(base, block);
	IRBuilder<> builder(block);
	return spec.codegen(lhs, nullptr, builder);
}

types::Type *UOpExpr::getType0() const
{
	return lhs->getType()->findUOp(op.symbol).outType;
}

UOpExpr *UOpExpr::clone(Generic *ref)
{
	return new UOpExpr(op, lhs->clone(ref));
}

BOpExpr::BOpExpr(Op op, Expr *lhs, Expr *rhs) :
    Expr(), op(std::move(op)), lhs(lhs), rhs(rhs)
{
}

void BOpExpr::resolveTypes()
{
	lhs->resolveTypes();
	rhs->resolveTypes();
}

Value *BOpExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	LLVMContext& context = block->getContext();

	/*
	 * && and || are special-cased because of short-circuiting
	 */
	if (op == bop("&&") || op == bop("||")) {
		const bool isAnd = (op == bop("&&"));

		lhs->ensure(types::Bool);
		rhs->ensure(types::Bool);
		Value *lhs = this->lhs->codegen(base, block);

		BasicBlock *b1 = BasicBlock::Create(context, "", block->getParent());

		IRBuilder<> builder(block);
		lhs = builder.CreateTrunc(lhs, IntegerType::getInt1Ty(context));
		BranchInst *branch = builder.CreateCondBr(lhs, b1, b1);  // one branch changed below

		Value *rhs = this->rhs->codegen(base, b1);
		builder.SetInsertPoint(b1);

		BasicBlock *b2 = BasicBlock::Create(context, "", block->getParent());
		builder.CreateBr(b2);
		builder.SetInsertPoint(b2);

		Type *boolTy = types::Bool->getLLVMType(context);
		Value *t = ConstantInt::get(boolTy, 1);
		Value *f = ConstantInt::get(boolTy, 0);

		PHINode *result = builder.CreatePHI(boolTy, 2);
		result->addIncoming(isAnd ? f : t, block);
		result->addIncoming(rhs, b1);

		branch->setSuccessor(isAnd ? 1 : 0, b2);
		block = b2;
		return result;
	} else {
		auto spec = lhs->getType()->findBOp(op.symbol, rhs->getType());
		Value *lhs = this->lhs->codegen(base, block);
		Value *rhs = this->rhs->codegen(base, block);
		IRBuilder<> builder(block);
		return spec.codegen(lhs, rhs, builder);
	}
}

types::Type *BOpExpr::getType0() const
{
	if (op == bop("&&") || op == bop("||"))
		return types::Bool;
	else
		return lhs->getType()->findBOp(op.symbol, rhs->getType()).outType;
}

BOpExpr *BOpExpr::clone(Generic *ref)
{
	return new BOpExpr(op, lhs->clone(ref), rhs->clone(ref));
}

ArrayLookupExpr::ArrayLookupExpr(Expr *arr, Expr *idx) :
    arr(arr), idx(idx)
{
}

void ArrayLookupExpr::resolveTypes()
{
	arr->resolveTypes();
	idx->resolveTypes();
}

Value *ArrayLookupExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	idx->ensure(types::Int);

	types::Type *type = arr->getType();
	auto *idxLit = dynamic_cast<IntExpr *>(idx);

	// check if this is a record lookup
	if (type->isGeneric(types::RecordType::get({})) && idxLit) {
		GetElemExpr e(arr, idxLit->value() + 1);
		return e.codegen0(base, block);
	}

	Value *arr = this->arr->codegen(base, block);
	Value *idx = this->idx->codegen(base, block);
	return this->arr->getType()->indexLoad(base, arr, idx, block);
}

types::Type *ArrayLookupExpr::getType0() const
{
	types::Type *type = arr->getType();
	auto *idxLit = dynamic_cast<IntExpr *>(idx);

	// check if this is a record lookup
	if (type->isGeneric(types::RecordType::get({})) && idxLit)
		return type->getBaseType((unsigned)idxLit->value());

	return arr->getType()->indexType();
}

ArrayLookupExpr *ArrayLookupExpr::clone(Generic *ref)
{
	return new ArrayLookupExpr(arr->clone(ref), idx->clone(ref));
}

ArraySliceExpr::ArraySliceExpr(Expr *arr, Expr *from, Expr *to) :
    arr(arr), from(from), to(to)
{
}

void ArraySliceExpr::resolveTypes()
{
	arr->resolveTypes();
	from->resolveTypes();
	to->resolveTypes();
}

Value *ArraySliceExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	assert(from || to);
	if (from) from->ensure(types::Int);
	if (to) to->ensure(types::Int);

	Value *arr = this->arr->codegen(base, block);

	if (!from) {
		Value *to = this->to->codegen(base, block);
		return this->arr->getType()->indexSliceNoFrom(base, arr, to, block);
	} else if (!to) {
		Value *from = this->from->codegen(base, block);
		return this->arr->getType()->indexSliceNoTo(base, arr, from, block);
	} else {
		Value *from = this->from->codegen(base, block);
		Value *to = this->to->codegen(base, block);
		return this->arr->getType()->indexSlice(base, arr, from, to, block);
	}
}

types::Type *ArraySliceExpr::getType0() const
{
	return arr->getType();
}

ArraySliceExpr *ArraySliceExpr::clone(Generic *ref)
{
	return new ArraySliceExpr(arr->clone(ref), from->clone(ref), to->clone(ref));
}

GetElemExpr::GetElemExpr(Expr *rec, std::string memb) :
    rec(rec), memb(std::move(memb))
{
}

GetElemExpr::GetElemExpr(Expr *rec, seq_int_t idx) :
    GetElemExpr(rec, std::to_string(idx))
{
	assert(idx >= 1);
}

Expr *GetElemExpr::getRec()
{
	return rec;
}

std::string GetElemExpr::getMemb()
{
	return memb;
}

void GetElemExpr::resolveTypes()
{
	rec->resolveTypes();
}

llvm::Value *GetElemExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	Value *rec = this->rec->codegen(base, block);
	return this->rec->getType()->memb(rec, memb, block);
}

types::Type *GetElemExpr::getType0() const
{
	return rec->getType()->membType(memb);
}

GetElemExpr *GetElemExpr::clone(Generic *ref)
{
	return new GetElemExpr(rec->clone(ref), memb);
}

GetStaticElemExpr::GetStaticElemExpr(types::Type *type, std::string memb) :
    Expr(), type(type), memb(std::move(memb))
{
}

types::Type *GetStaticElemExpr::getTypeInExpr() const
{
	return type;
}

std::string GetStaticElemExpr::getMemb() const
{
	return memb;
}

Value *GetStaticElemExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	return type->staticMemb(memb, block);
}

types::Type *GetStaticElemExpr::getType0() const
{
	return type->staticMembType(memb);
}

GetStaticElemExpr *GetStaticElemExpr::clone(Generic *ref)
{
	return new GetStaticElemExpr(type->clone(ref), memb);
}

MethodExpr::MethodExpr(Expr *expr,
                       std::string name,
                       std::vector<types::Type *> types,
                       Expr *orig) :
    Expr(), expr(expr), name(std::move(name)), types(std::move(types)), orig(orig)
{
}

void MethodExpr::resolveTypes()
{
	expr->resolveTypes();
}

Value *MethodExpr::codegen0(BaseFunc *base, llvm::BasicBlock*& block)
{
	types::Type *type = expr->getType();
	auto *func = dynamic_cast<Func *>(type->getMethod(name));

	if (!func)
		throw exc::SeqException("method '" + name + "' of type '" + type->getName() + "' is not generic");

	Value *self = expr->codegen(base, block);

	if (!types.empty())
		func = func->realize(types);

	Value *method = FuncExpr(func).codegen(base, block);
	return getType0()->make(self, method, block);
}

types::MethodType *MethodExpr::getType0() const
{
	types::Type *type = expr->getType();
	auto *func = dynamic_cast<Func *>(type->getMethod(name));

	if (!func)
		throw exc::SeqException("method '" + name + "' of type '" + type->getName() + "' is not generic");

	if (!types.empty())
		func = func->realize(types);

	return types::MethodType::get(expr->getType(), func->getFuncType());
}

Expr *MethodExpr::clone(Generic *ref)
{
	if (orig)
		return orig->clone(ref);

	std::vector<types::Type *> typesCloned;
	for (auto *type : types)
		typesCloned.push_back(type->clone(ref));

	return new MethodExpr(expr->clone(ref), name, typesCloned);
}

CallExpr::CallExpr(Expr *func, std::vector<Expr *> args) :
    func(func), args(std::move(args))
{
}

Expr *CallExpr::getFuncExpr() const
{
	return func;
}

void CallExpr::setFuncExpr(Expr *func)
{
	this->func = func;
}

void CallExpr::resolveTypes()
{
	func->resolveTypes();
	for (auto *arg : args)
		arg->resolveTypes();
}

static Func *getFuncFromFuncExpr(Expr *func)
{
	auto *funcExpr = dynamic_cast<FuncExpr *>(func);
	if (funcExpr) {
		auto *f = dynamic_cast<Func *>(funcExpr->getFunc());
		if (f) return f;
	}

	return nullptr;
}

static bool getFullCallTypesForPartial(Func *func,
                                       types::PartialFuncType *parType,
                                       const std::vector<types::Type *>& argTypes,
                                       std::vector<types::Type *>& typesFull)
{
	if (func && func->numGenerics() > 0 && func->unrealized() && parType) {
		// painful process of organizing types correctly...
		std::vector<types::Type *> callTypes = parType->getCallTypes();

		unsigned next = 0;
		for (auto *type : callTypes) {
			if (type) {
				typesFull.push_back(type);
			} else {
				if (next < argTypes.size())
					typesFull.push_back(argTypes[next++]);
				else
					return false;
			}
		}

		return true;
	}

	return false;
}

static void deduceTypeParametersIfNecessary(Expr*& func, const std::vector<types::Type *>& argTypes)
{
	/*
	 * Important note: If we're able to deduce type parameters, we change the structure
	 * of the AST by replacing functions etc. However, in order for generics/cloning to
	 * work properly, we need to preserve the original AST. For this reason, both FuncExpr
	 * and MethodExpr take an optional 'orig' argument representing the original Expr to
	 * be returned when cloning.
	 */

	try {
		// simple call
		Func *f = getFuncFromFuncExpr(func);
		if (f && f->numGenerics() > 0 && f->unrealized())
			func = new FuncExpr(f->realize(f->deduceTypesFromArgTypes(argTypes)), func);

		// partial call I -- known partial
		auto *partialExpr = dynamic_cast<PartialCallExpr *>(func);
		if (partialExpr) {
			auto *parType = dynamic_cast<types::PartialFuncType *>(partialExpr->getType());
			Func *g = getFuncFromFuncExpr(partialExpr->getFuncExpr());
			std::vector<types::Type *> typesFull;

			if (getFullCallTypesForPartial(g, parType, argTypes, typesFull))
				partialExpr->setFuncExpr(
				  new FuncExpr(g->realize(g->deduceTypesFromArgTypes(typesFull)), partialExpr->getFuncExpr()));
		}

		// partial call II -- partial masquerading as regular call
		auto *callExpr = dynamic_cast<CallExpr *>(func);
		if (callExpr) {
			auto *parType = dynamic_cast<types::PartialFuncType *>(callExpr->getType());
			Func *g = getFuncFromFuncExpr(callExpr->getFuncExpr());
			std::vector<types::Type *> typesFull;

			if (getFullCallTypesForPartial(g, parType, argTypes, typesFull))
				callExpr->setFuncExpr(
				  new FuncExpr(g->realize(g->deduceTypesFromArgTypes(typesFull)), callExpr->getFuncExpr()));
		}

		// method call
		auto *elemExpr = dynamic_cast<GetElemExpr *>(func);
		if (elemExpr) {
			std::string name = elemExpr->getMemb();
			types::Type *type = elemExpr->getRec()->getType();
			if (type->hasMethod(name)) {
				auto *g = dynamic_cast<Func *>(type->getMethod(name));
				if (g && g->numGenerics() > 0 && g->unrealized()) {
					std::vector<types::Type *> typesFull(argTypes);
					typesFull.insert(typesFull.begin(), type);  // methods take 'self' as first argument
					func = new MethodExpr(elemExpr->getRec(), name, g->deduceTypesFromArgTypes(typesFull), func);
				}
			}
		}

		// static method call
		auto *elemStaticExpr = dynamic_cast<GetStaticElemExpr *>(func);
		if (elemStaticExpr) {
			std::string name = elemStaticExpr->getMemb();
			types::Type *type = elemStaticExpr->getTypeInExpr();
			if (type->hasMethod(name)) {
				auto *g = dynamic_cast<Func *>(type->getMethod(name));
				if (g && g->numGenerics() > 0 && g->unrealized())
					func = new FuncExpr(g->realize(g->deduceTypesFromArgTypes(argTypes)), func);
			}
		}
	} catch(exc::SeqException&) {
		/*
		 * We weren't able to deduce type parameters now,
		 * but we may be able to do so later, so ignore
		 * any exceptions.
		 */
	}
}

Value *CallExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	types::Type *type = getType();  // validates call
	Value *f = func->codegen(base, block);
	std::vector<Value *> x;
	for (auto *e : args)
		x.push_back(e->codegen(base, block));

	// check if this is really a partial function
	Func *f0 = getFuncFromFuncExpr(func);
	if (f0 && f0->getFuncType()->argCount() > x.size()) {
		auto *partial = dynamic_cast<types::PartialFuncType *>(type);
		assert(partial);
		return partial->make(f, x, block);
	}

	return func->getType()->call(base, f, x, block);
}

types::Type *CallExpr::getType0() const
{
	std::vector<types::Type *> types;
	for (auto *e : args)
		types.push_back(e->getType());

	// check if this is really a partial function
	Func *f = getFuncFromFuncExpr(func);
	int missingArgs = 0;
	if (f && (missingArgs = (int)f->getFuncType()->argCount() - (int)types.size()) > 0) {
		for (int i = 0; i < missingArgs; i++)
			types.insert(types.begin(), nullptr);

		deduceTypeParametersIfNecessary(func, types);
		return types::PartialFuncType::get(func->getType(), types);
	}

	deduceTypeParametersIfNecessary(func, types);
	return func->getType()->getCallType(types);
}

CallExpr *CallExpr::clone(Generic *ref)
{
	std::vector<Expr *> argsCloned;
	for (auto *arg : args)
		argsCloned.push_back(arg->clone(ref));
	return new CallExpr(func->clone(ref), argsCloned);
}

PartialCallExpr::PartialCallExpr(Expr *func, std::vector<Expr *> args) :
    func(func), args(std::move(args))
{
}

Expr *PartialCallExpr::getFuncExpr() const
{
	return func;
}

void PartialCallExpr::setFuncExpr(Expr *func)
{
	this->func = func;
}

void PartialCallExpr::resolveTypes()
{
	func->resolveTypes();
	for (auto *arg : args) {
		if (arg)
			arg->resolveTypes();
	}
}

Value *PartialCallExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	types::PartialFuncType *par = getType0();

	Value *f = func->codegen(base, block);
	std::vector<Value *> x;
	for (auto *e : args) {
		if (e)
			x.push_back(e->codegen(base, block));
	}

	return par->make(f, x, block);
}

types::PartialFuncType *PartialCallExpr::getType0() const
{
	std::vector<types::Type *> types;
	for (auto *e : args)
		types.push_back(e ? e->getType() : nullptr);

	deduceTypeParametersIfNecessary(func, types);
	return types::PartialFuncType::get(func->getType(), types);
}

PartialCallExpr *PartialCallExpr::clone(seq::Generic *ref)
{
	std::vector<Expr *> argsCloned;
	for (auto *arg : args)
		argsCloned.push_back(arg ? arg->clone(ref) : nullptr);
	return new PartialCallExpr(func->clone(ref), argsCloned);
}

CondExpr::CondExpr(Expr *cond, Expr *ifTrue, Expr *ifFalse) :
    Expr(), cond(cond), ifTrue(ifTrue), ifFalse(ifFalse)
{
}

void CondExpr::resolveTypes()
{
	cond->resolveTypes();
	ifTrue->resolveTypes();
	ifFalse->resolveTypes();
}

Value *CondExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	cond->ensure(types::Bool);

	LLVMContext& context = block->getContext();

	Value *cond = this->cond->codegen(base, block);
	IRBuilder<> builder(block);
	cond = builder.CreateTrunc(cond, IntegerType::getInt1Ty(context));

	BasicBlock *b1 = BasicBlock::Create(context, "", block->getParent());
	BranchInst *branch0 = builder.CreateCondBr(cond, b1, b1);  // we set false-branch below

	Value *ifTrue = this->ifTrue->codegen(base, b1);
	builder.SetInsertPoint(b1);
	BranchInst *branch1 = builder.CreateBr(b1);  // changed below

	BasicBlock *b2 = BasicBlock::Create(context, "", block->getParent());
	branch0->setSuccessor(1, b2);
	Value *ifFalse = this->ifFalse->codegen(base, b2);
	builder.SetInsertPoint(b2);
	BranchInst *branch2 = builder.CreateBr(b2);  // changed below

	block = BasicBlock::Create(context, "", block->getParent());
	branch1->setSuccessor(0, block);
	branch2->setSuccessor(0, block);
	builder.SetInsertPoint(block);
	PHINode *result = builder.CreatePHI(getType()->getLLVMType(context), 2);
	result->addIncoming(ifTrue, b1);
	result->addIncoming(ifFalse, b2);
	return result;
}

types::Type *CondExpr::getType0() const
{
	types::Type *trueType = ifTrue->getType();
	types::Type *falseType = ifFalse->getType();
	if (!types::is(trueType, falseType))
		throw exc::SeqException("inconsistent types '" + trueType->getName() + "' and '" +
		                        falseType->getName() + "' in conditional expression");

	return trueType;
}

CondExpr *CondExpr::clone(Generic *ref)
{
	return new CondExpr(cond->clone(ref), ifTrue->clone(ref), ifFalse->clone(ref));
}

MatchExpr::MatchExpr() :
    Expr(), value(nullptr), patterns(), exprs()
{
}

void MatchExpr::setValue(Expr *value)
{
	assert(!this->value);
	this->value = value;
}

void MatchExpr::addCase(Pattern *pattern, Expr *expr)
{
	patterns.push_back(pattern);
	exprs.push_back(expr);
}

void MatchExpr::resolveTypes()
{
	assert(value);
	value->resolveTypes();

	for (auto *pattern : patterns)
		pattern->resolveTypes(value->getType());

	for (auto *expr : exprs)
		expr->resolveTypes();
}

Value *MatchExpr::codegen0(BaseFunc *base, BasicBlock *&block)
{
	assert(!patterns.empty());
	assert(patterns.size() == exprs.size() && value);

	LLVMContext& context = block->getContext();
	Function *func = block->getParent();

	IRBuilder<> builder(block);
	types::Type *valType = value->getType();
	types::Type *resType = getType();

	bool seenCatchAll = false;
	for (auto *pattern : patterns) {
		pattern->resolveTypes(valType);
		if (pattern->isCatchAll())
			seenCatchAll = true;
	}

	if (!seenCatchAll)
		throw exc::SeqException("match expression missing catch-all pattern");

	Value *val = value->codegen(base, block);

	std::vector<std::pair<BranchInst *, Value *>> binsts;

	for (unsigned i = 0; i < patterns.size(); i++) {
		Value *cond = patterns[i]->codegen(base, valType, val, block);

		builder.SetInsertPoint(block);  // recall: expr codegen can change the block
		block = BasicBlock::Create(context, "", func);  // match block
		BranchInst *binst1 = builder.CreateCondBr(cond, block, block);  // we set false-branch below

		Value *result = exprs[i]->codegen(base, block);
		builder.SetInsertPoint(block);
		BranchInst *binst2 = builder.CreateBr(block);  // we reset this below
		binsts.emplace_back(binst2, result);

		block = BasicBlock::Create(context, "", func);  // mismatch block (eval next pattern)
		binst1->setSuccessor(1, block);
	}

	builder.SetInsertPoint(block);
	builder.CreateUnreachable();

	block = BasicBlock::Create(context, "", func);
	builder.SetInsertPoint(block);

	PHINode *result = builder.CreatePHI(resType->getLLVMType(context), (unsigned)patterns.size());
	for (auto& binst : binsts) {
		binst.first->setSuccessor(0, block);
		result->addIncoming(binst.second, binst.first->getParent());
	}

	return result;
}

types::Type *MatchExpr::getType0() const
{
	assert(!exprs.empty());
	types::Type *type = exprs[0]->getType();

	for (auto *expr : exprs) {
		if (!types::is(type, expr->getType()))
			throw exc::SeqException("inconsistent result types in match expression");
	}

	return type;
}

MatchExpr *MatchExpr::clone(Generic *ref)
{
	auto *x = new MatchExpr();

	std::vector<Pattern *> patternsCloned;
	std::vector<Expr *> exprsCloned;

	for (auto *pattern : patterns)
		patternsCloned.push_back(pattern->clone(ref));

	for (auto *expr : exprs)
		exprsCloned.push_back(expr->clone(ref));

	if (value) x->value = value->clone(ref);
	x->patterns = patternsCloned;
	x->exprs = exprsCloned;

	return x;
}

ConstructExpr::ConstructExpr(types::Type *type, std::vector<Expr *> args) :
    Expr(), type(type), args(std::move(args))
{
}

void ConstructExpr::resolveTypes()
{
	for (auto *arg : args)
		arg->resolveTypes();
}

Value *ConstructExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	getType();  // validates construction
	std::vector<Value *> vals;
	for (auto *arg : args)
		vals.push_back(arg->codegen(base, block));
	return type->construct(base, vals, block);
}

types::Type *ConstructExpr::getType0() const
{
	std::vector<types::Type *> types;
	for (auto *arg : args)
		types.push_back(arg->getType());

	// type parameter deduction if constructing generic class:
	auto *ref = dynamic_cast<types::RefType *>(type);
	if (ref && ref->numGenerics() > 0 && ref->unrealized())
		type = ref->realize(ref->deduceTypesFromArgTypes(types));

	return type->getConstructType(types);
}

ConstructExpr *ConstructExpr::clone(Generic *ref)
{
	std::vector<Expr *> argsCloned;
	for (auto *arg : args)
		argsCloned.push_back(arg->clone(ref));
	return new ConstructExpr(type->clone(ref), argsCloned);
}

OptExpr::OptExpr(Expr *val) : Expr(), val(val)
{
}

void OptExpr::resolveTypes()
{
	val->resolveTypes();
}

Value *OptExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	Value *val = this->val->codegen(base, block);
	return ((types::OptionalType *)getType())->make(val, block);
}

types::Type *OptExpr::getType0() const
{
	return types::OptionalType::get(val->getType());
}

OptExpr *OptExpr::clone(Generic *ref)
{
	return new OptExpr(val->clone(ref));
}

DefaultExpr::DefaultExpr(types::Type *type) : Expr(type)
{
}

Value *DefaultExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	return getType()->defaultValue(block);
}

DefaultExpr *DefaultExpr::clone(Generic *ref)
{
	return new DefaultExpr(getType()->clone(ref));
}

PipeExpr::PipeExpr(std::vector<seq::Expr *> stages) :
    Expr(), stages(std::move(stages))
{
}

void PipeExpr::resolveTypes()
{
	for (auto *stage : stages)
		stage->resolveTypes();
}

static Value *codegenPipe(BaseFunc *base,
                          Value *val,
                          types::Type *type,
                          BasicBlock*& block,
                          std::queue<Expr *>& stages)
{
	if (stages.empty())
		return val;

	LLVMContext& context = block->getContext();
	Function *func = block->getParent();

	Expr *stage = stages.front();
	stages.pop();

	if (!val) {
		assert(!type);
		type = stage->getType();
		val = stage->codegen(base, block);
	} else {
		assert(val && type);
		ValueExpr arg(type, val);
		CallExpr call(stage, {&arg});  // do this through CallExpr for type-parameter deduction
		type = call.getType();
		val = call.codegen(base, block);
	}

	if (type->isGeneric(types::Gen) && stage != stages.back()) {
		auto *genType = dynamic_cast<types::GenType *>(type);
		assert(genType);
		Value *gen = val;
		IRBuilder<> builder(block);

		BasicBlock *loop = BasicBlock::Create(context, "pipe", func);
		builder.CreateBr(loop);

		builder.SetInsertPoint(loop);
		genType->resume(gen, loop);
		Value *cond = genType->done(gen, loop);
		BasicBlock *body = BasicBlock::Create(context, "body", func);
		BranchInst *branch = builder.CreateCondBr(cond, body, body);  // we set true-branch below

		block = body;
		type = genType->getBaseType(0);
		val = type->is(types::Void) ? nullptr : genType->promise(gen, block);

		codegenPipe(base, val, type, block, stages);

		builder.SetInsertPoint(block);
		builder.CreateBr(loop);

		BasicBlock *cleanup = BasicBlock::Create(context, "cleanup", func);
		branch->setSuccessor(0, cleanup);
		genType->destroy(gen, cleanup);

		builder.SetInsertPoint(cleanup);
		BasicBlock *exit = BasicBlock::Create(context, "exit", func);
		builder.CreateBr(exit);
		block = exit;
		return nullptr;
	} else {
		return codegenPipe(base, val, type, block, stages);
	}
}

Value *PipeExpr::codegen0(BaseFunc *base, BasicBlock*& block)
{
	std::queue<Expr *> queue;
	for (auto *stage : stages)
		queue.push(stage);

	return codegenPipe(base, nullptr, nullptr, block, queue);
}

types::Type *PipeExpr::getType0() const
{
	types::Type *type = nullptr;
	for (auto *stage : stages) {
		if (!type) {
			type = stage->getType();
			continue;
		}

		ValueExpr arg(type, nullptr);
		CallExpr call(stage, {&arg});  // do this through CallExpr for type-parameter deduction
		type = call.getType();

		if (stage != stages.back() && type->isGeneric(types::Gen))
			return types::Void;
	}
	assert(type);
	return type;
}

PipeExpr *PipeExpr::clone(Generic *ref)
{
	std::vector<Expr *> stagesCloned;
	for (auto *stage : stages)
		stagesCloned.push_back(stage->clone(ref));
	return new PipeExpr(stagesCloned);
}
