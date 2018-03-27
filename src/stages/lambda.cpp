#include <string>
#include "common.h"
#include "exc.h"
#include "lambda.h"

using namespace seq;
using namespace llvm;

LambdaNode::LambdaNode(std::initializer_list<LambdaNode *> children) : children(children)
{
}

IdentNode::IdentNode() : LambdaNode({}), v(nullptr)
{
}

Value *IdentNode::codegen(BasicBlock *block, bool isFloat) const
{
	return v;
}

struct ConstIntNode : LambdaNode {
	seq_int_t n;
	explicit ConstIntNode(seq_int_t n) : LambdaNode({}), n(n) {}
	Value *codegen(BasicBlock *block, bool isFloat) const override
	{
		LLVMContext& context = block->getContext();
		if (isFloat)
			return ConstantFP::get(Type::getDoubleTy(context), (double)n);
		else
			return ConstantInt::get(seqIntLLVM(context), (uint64_t)n);
	}
};

struct ConstFloatNode : LambdaNode {
	double f;
	explicit ConstFloatNode(double f) : LambdaNode({}), f(f) {}
	Value *codegen(BasicBlock *block, bool isFloat) const override
	{
		if (!isFloat)
			throw exc::SeqException("floating-point literal in integer lambda");

		LLVMContext& context = block->getContext();
		return ConstantFP::get(Type::getDoubleTy(context), f);
	}
};

struct AddNode : LambdaNode {
	AddNode(LambdaNode *a, LambdaNode *b) : LambdaNode({a, b}) {}
	Value *codegen(BasicBlock *block, bool isFloat) const override
	{
		IRBuilder<> builder(block);
		if (isFloat)
			return builder.CreateFAdd(children[0]->codegen(block, true),
			                          children[1]->codegen(block, true));
		else
			return builder.CreateAdd(children[0]->codegen(block, false),
			                         children[1]->codegen(block, false));
	}
};

struct SubNode : LambdaNode {
	SubNode(LambdaNode *a, LambdaNode *b) : LambdaNode({a, b}) {}
	Value *codegen(BasicBlock *block, bool isFloat) const override
	{
		IRBuilder<> builder(block);
		if (isFloat)
			return builder.CreateFSub(children[0]->codegen(block, true),
			                          children[1]->codegen(block, true));
		else
			return builder.CreateSub(children[0]->codegen(block, false),
			                         children[1]->codegen(block, false));
	}
};

struct MulNode : LambdaNode {
	MulNode(LambdaNode *a, LambdaNode *b) : LambdaNode({a, b}) {}
	Value *codegen(BasicBlock *block, bool isFloat) const override
	{
		IRBuilder<> builder(block);
		if (isFloat)
			return builder.CreateFMul(children[0]->codegen(block, true),
			                          children[1]->codegen(block, true));
		else
			return builder.CreateMul(children[0]->codegen(block, false),
			                         children[1]->codegen(block, false));
	}
};

struct DivNode : LambdaNode {
	DivNode(LambdaNode *a, LambdaNode *b) : LambdaNode({a, b}) {}
	Value *codegen(BasicBlock *block, bool isFloat) const override
	{
		IRBuilder<> builder(block);
		if (isFloat)
			return builder.CreateFDiv(children[0]->codegen(block, true),
			                          children[1]->codegen(block, true));
		else
			return builder.CreateSDiv(children[0]->codegen(block, false),
			                          children[1]->codegen(block, false));
	}
};

LambdaContext::LambdaContext() : lambda(nullptr)
{
	root = arg = new IdentNode();
}

Function *LambdaContext::codegen(Module *module, bool isFloat)
{
	LLVMContext& context = module->getContext();

	static int counter = 0;
	lambda = cast<Function>(
	           module->getOrInsertFunction(
	             "lambda" + std::to_string(counter++),
	             isFloat ? Type::getDoubleTy(context) : seqIntLLVM(context),
	             isFloat ? Type::getDoubleTy(context) : seqIntLLVM(context)));

	arg->v = lambda->arg_begin();
	BasicBlock *entry = BasicBlock::Create(context, "entry", lambda);
	Value *result = root->codegen(entry, isFloat);
	IRBuilder<> builder(entry);
	builder.CreateRet(result);

	return lambda;
}

LambdaContextProxy::operator LambdaContext&()
{
	return *new LambdaContext();
}

LambdaContext& seq::operator+(LambdaContext& lambda, LambdaNode& node)
{
	lambda.root = new AddNode(lambda.root, &node);
	return lambda;
}

LambdaContext& seq::operator-(LambdaContext& lambda, LambdaNode& node)
{
	lambda.root = new SubNode(lambda.root, &node);
	return lambda;
}

LambdaContext& seq::operator*(LambdaContext& lambda, LambdaNode& node)
{
	lambda.root = new MulNode(lambda.root, &node);
	return lambda;
}

LambdaContext& seq::operator/(LambdaContext& lambda, LambdaNode& node)
{
	lambda.root = new DivNode(lambda.root, &node);
	return lambda;
}

LambdaContext& seq::operator+(LambdaNode& node, LambdaContext& lambda)
{
	lambda.root = new AddNode(&node, lambda.root);
	return lambda;
}

LambdaContext& seq::operator-(LambdaNode& node, LambdaContext& lambda)
{
	lambda.root = new SubNode(&node, lambda.root);
	return lambda;
}

LambdaContext& seq::operator*(LambdaNode& node, LambdaContext& lambda)
{
	lambda.root = new MulNode(&node, lambda.root);
	return lambda;
}

LambdaContext& seq::operator/(LambdaNode& node, LambdaContext& lambda)
{
	lambda.root = new DivNode(&node, lambda.root);
	return lambda;
}

LambdaContext& seq::operator+(LambdaContext& lambda, int n)
{
	LambdaNode& node = *new ConstIntNode(n);
	return lambda + node;
}

LambdaContext& seq::operator-(LambdaContext& lambda, int n)
{
	LambdaNode& node = *new ConstIntNode(n);
	return lambda - node;
}

LambdaContext& seq::operator*(LambdaContext& lambda, int n)
{
	LambdaNode& node = *new ConstIntNode(n);
	return lambda * node;
}

LambdaContext& seq::operator/(LambdaContext& lambda, int n)
{
	LambdaNode& node = *new ConstIntNode(n);
	return lambda / node;
}

LambdaContext& seq::operator+(int n, LambdaContext& lambda)
{
	LambdaNode& node = *new ConstIntNode(n);
	return node + lambda;
}

LambdaContext& seq::operator-(int n, LambdaContext& lambda)
{
	LambdaNode& node = *new ConstIntNode(n);
	return node - lambda;
}

LambdaContext& seq::operator*(int n, LambdaContext& lambda)
{
	LambdaNode& node = *new ConstIntNode(n);
	return node * lambda;
}

LambdaContext& seq::operator/(int n, LambdaContext& lambda)
{
	LambdaNode& node = *new ConstIntNode(n);
	return node / lambda;
}

LambdaContext& seq::operator+(LambdaContext& lambda, double f)
{
	LambdaNode& node = *new ConstFloatNode(f);
	return lambda + node;
}

LambdaContext& seq::operator-(LambdaContext& lambda, double f)
{
	LambdaNode& node = *new ConstFloatNode(f);
	return lambda - node;
}

LambdaContext& seq::operator*(LambdaContext& lambda, double f)
{
	LambdaNode& node = *new ConstFloatNode(f);
	return lambda * node;
}

LambdaContext& seq::operator/(LambdaContext& lambda, double f)
{
	LambdaNode& node = *new ConstFloatNode(f);
	return lambda / node;
}

LambdaContext& seq::operator+(double f, LambdaContext& lambda)
{
	LambdaNode& node = *new ConstFloatNode(f);
	return node + lambda;
}

LambdaContext& seq::operator-(double f, LambdaContext& lambda)
{
	LambdaNode& node = *new ConstFloatNode(f);
	return node - lambda;
}

LambdaContext& seq::operator*(double f, LambdaContext& lambda)
{
	LambdaNode& node = *new ConstFloatNode(f);
	return node * lambda;
}

LambdaContext& seq::operator/(double f, LambdaContext& lambda)
{
	LambdaNode& node = *new ConstFloatNode(f);
	return node / lambda;
}

LambdaContext& seq::operator+(LambdaContext& lambda1, LambdaContext& lambda2)
{
	return lambda1 + *lambda2.root;
}

LambdaContext& seq::operator-(LambdaContext& lambda1, LambdaContext& lambda2)
{
	return lambda1 - *lambda2.root;
}

LambdaContext& seq::operator*(LambdaContext& lambda1, LambdaContext& lambda2)
{
	return lambda1 * *lambda2.root;
}

LambdaContext& seq::operator/(LambdaContext& lambda1, LambdaContext& lambda2)
{
	return lambda1 / *lambda2.root;
}

LambdaStage::LambdaStage(LambdaContext& lambda) :
    Stage("lambda", types::VoidType::get(), types::VoidType::get()),
    isFloat(false), lambda(lambda)
{
}

void LambdaStage::validate()
{
	if (prev)
		in = out = prev->getOutType();

	if (in->isChildOf(types::IntType::get()))
		isFloat = false;
	else if (in->isChildOf(types::FloatType::get()))
		isFloat = true;
	else
		throw exc::ValidationException(*this);

	Stage::validate();
}

void LambdaStage::codegen(Module *module)
{
	ensurePrev();
	validate();

	const auto key = isFloat ? SeqData::FLOAT : SeqData::INT;
	Function *func = lambda.codegen(module, isFloat);
	block = prev->block;
	IRBuilder<> builder(block);
	std::vector<Value *> args = {getSafe(prev->outs, key)};
	Value *result = builder.CreateCall(func, args);

	outs->insert({key, result});

	codegenNext(module);
	prev->setAfter(getAfter());
}

LambdaStage& LambdaStage::make(LambdaContext& lambda)
{
	return *new LambdaStage(lambda);
}
