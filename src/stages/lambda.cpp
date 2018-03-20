#include "common.h"
#include "lambda.h"

using namespace seq;
using namespace llvm;

LambdaNode::LambdaNode(std::initializer_list<LambdaNode *> children) : children(children)
{
}

IdentNode::IdentNode() : LambdaNode({}), v(nullptr)
{
}

Value *IdentNode::codegen(LLVMContext& context, BasicBlock *block) const
{
	return v;
}

struct ConstNode : LambdaNode {
	seq_int_t n;
	ConstNode(seq_int_t n) : LambdaNode({}), n(n) {}
	Value *codegen(LLVMContext& context, BasicBlock *block) const override
	{
		return ConstantInt::get(seqIntLLVM(context), (uint64_t)n);
	}
};

struct AddNode : LambdaNode {
	AddNode(LambdaNode *a, LambdaNode *b) : LambdaNode({a, b}) {}
	Value *codegen(LLVMContext& context, BasicBlock *block) const override
	{
		IRBuilder<> builder(block);
		return builder.CreateAdd(children[0]->codegen(context, block),
		                         children[1]->codegen(context, block));
	}
};

struct SubNode : LambdaNode {
	SubNode(LambdaNode *a, LambdaNode *b) : LambdaNode({a, b}) {}
	Value *codegen(LLVMContext& context, BasicBlock *block) const override
	{
		IRBuilder<> builder(block);
		return builder.CreateSub(children[0]->codegen(context, block),
		                         children[1]->codegen(context, block));
	}
};

struct MulNode : LambdaNode {
	MulNode(LambdaNode *a, LambdaNode *b) : LambdaNode({a, b}) {}
	Value *codegen(LLVMContext& context, BasicBlock *block) const override
	{
		IRBuilder<> builder(block);
		return builder.CreateMul(children[0]->codegen(context, block),
		                         children[1]->codegen(context, block));
	}
};

struct DivNode : LambdaNode {
	DivNode(LambdaNode *a, LambdaNode *b) : LambdaNode({a, b}) {}
	Value *codegen(LLVMContext& context, BasicBlock *block) const override
	{
		IRBuilder<> builder(block);
		return builder.CreateSDiv(children[0]->codegen(context, block),
		                          children[1]->codegen(context, block));
	}
};

LambdaContext::LambdaContext() : lambda(nullptr)
{
	root = arg = new IdentNode();
}

Function *LambdaContext::codegen(Module *module, LLVMContext &context)
{
	lambda = cast<Function>(
               module->getOrInsertFunction(
                 "lambda",
                 seqIntLLVM(context),
                 seqIntLLVM(context)));

	arg->v = lambda->arg_begin();
	BasicBlock *entry = BasicBlock::Create(context, "entry", lambda);
	Value *result = root->codegen(context, entry);
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

LambdaContext& seq::operator+(LambdaContext& lambda, seq_int_t n)
{
	LambdaNode& node = *new ConstNode(n);
	return lambda + node;
}

LambdaContext& seq::operator-(LambdaContext& lambda, seq_int_t n)
{
	LambdaNode& node = *new ConstNode(n);
	return lambda - node;
}

LambdaContext& seq::operator*(LambdaContext& lambda, seq_int_t n)
{
	LambdaNode& node = *new ConstNode(n);
	return lambda * node;
}

LambdaContext& seq::operator/(LambdaContext& lambda, seq_int_t n)
{
	LambdaNode& node = *new ConstNode(n);
	return lambda / node;
}

LambdaContext& seq::operator+(seq_int_t n, LambdaContext& lambda)
{
	LambdaNode& node = *new ConstNode(n);
	return node + lambda;
}

LambdaContext& seq::operator-(seq_int_t n, LambdaContext& lambda)
{
	LambdaNode& node = *new ConstNode(n);
	return node - lambda;
}

LambdaContext& seq::operator*(seq_int_t n, LambdaContext& lambda)
{
	LambdaNode& node = *new ConstNode(n);
	return node * lambda;
}

LambdaContext& seq::operator/(seq_int_t n, LambdaContext& lambda)
{
	LambdaNode& node = *new ConstNode(n);
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
    Stage("lambda", types::IntType::get(), types::IntType::get()), lambda(lambda)
{
}

void LambdaStage::codegen(Module *module, LLVMContext& context)
{
	ensurePrev();
	validate();

	auto iter = prev->outs->find(SeqData::INT);

	if (iter == prev->outs->end())
		throw exc::StageException("pipeline error", *this);

	Function *func = lambda.codegen(module, context);
	block = prev->block;
	IRBuilder<> builder(block);
	std::vector<Value *> args = {iter->second};
	Value *result = builder.CreateCall(func, args);

	outs->insert({SeqData::INT, result});

	codegenNext(module, context);
	prev->setAfter(getAfter());
}

LambdaStage& LambdaStage::make(LambdaContext& lambda)
{
	return *new LambdaStage(lambda);
}
