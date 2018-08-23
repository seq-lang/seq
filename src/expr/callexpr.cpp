#include "seq/seq.h"
#include "seq/callexpr.h"

using namespace seq;
using namespace llvm;

CallExpr::CallExpr(Expr *func, std::vector<Expr *> args) :
    func(func), args(std::move(args))
{
}

Value *CallExpr::codegen(BaseFunc *base, BasicBlock*& block)
{
	getType();  // validates call
	Value *f = func->codegen(base, block);
	std::vector<Value *> x;
	for (auto *e : args)
		x.push_back(e->codegen(base, block));
	return func->getType()->call(base, f, x, block);
}

types::Type *CallExpr::getType() const
{
	std::vector<types::Type *> types;
	for (auto *e : args)
		types.push_back(e->getType());

	// type parameter deduction if calling generic function:
	auto *funcExpr = dynamic_cast<FuncExpr *>(func);
	if (funcExpr && !funcExpr->isParameterized()) {
		auto *f = dynamic_cast<Func *>(funcExpr->getFunc());
		if (f && f->numGenerics() > 0 && f->unrealized())
			func = new FuncExpr(f->realize(f->deduceTypesFromArgTypes(types)));
	}

	auto *elemExpr = dynamic_cast<GetElemExpr *>(func);
	if (elemExpr) {
		std::string name = elemExpr->getMemb();
		types::Type *type = elemExpr->getRec()->getType();
		if (type->hasMethod(name)) {
			auto *f = dynamic_cast<Func *>(type->getMethod(name));
			if (f && f->numGenerics() > 0 && f->unrealized()) {
				std::vector<types::Type *> typesFull(types);
				typesFull.insert(typesFull.begin(), type);  // methods take 'self' as first argument
				func = new MethodExpr(elemExpr->getRec(), name, f->deduceTypesFromArgTypes(typesFull));
			}
		}
	}

	return func->getType()->getCallType(types);
}

CallExpr *CallExpr::clone(Generic *ref)
{
	std::vector<Expr *> argsCloned;
	for (auto *arg : args)
		argsCloned.push_back(arg->clone(ref));
	return new CallExpr(func->clone(ref), argsCloned);
}
