#include <iostream>
#include <string>
#include <vector>
#include "util.h"
#include "exc.h"
#include "seq.h"

using namespace seq;
using namespace llvm;

Pipeline::Pipeline(Stage *head, Stage *tail) : head(head), tail(tail), linked(false)
{
}

Pipeline& Stage::operator|(Stage& to)
{
	if (linked)
		throw exc::MultiLinkException(*this);

	if (to.linked)
		throw exc::MultiLinkException(to);

	next = &to;
	to.prev = this;
	to.linked = linked = true;
	return *new Pipeline(this, &to);
}

Pipeline& Stage::operator|(Pipeline& to)
{
	if (linked)
		throw exc::MultiLinkException(*this);

	if (to.linked)
		throw exc::MultiLinkException(*to.getHead());

	next = to.head;
	to.head->prev = this;
	to.head = this;
	linked = true;
	return to;
}

Pipeline& Pipeline::operator|(Stage& to)
{
	if (linked)
		throw exc::MultiLinkException(*getHead());

	if (to.linked)
		throw exc::MultiLinkException(to);

	tail->next = &to;
	to.prev = tail;
	tail = &to;
	to.linked = true;
	return *this;
}

Pipeline& Pipeline::operator|(Pipeline& to)
{
	if (linked)
		throw exc::MultiLinkException(*getHead());

	if (to.linked)
		throw exc::MultiLinkException(*to.getHead());

	tail->next = to.head;
	to.head->prev = tail;
	tail = to.tail;
	to.linked = true;
	return *this;
}

std::ostream& operator<<(std::ostream& os, Stage& stage)
{
	return os << stage.getName();
}

std::ostream& operator<<(std::ostream& os, Pipeline& stage)
{
	for (Stage *s = stage.getHead(); s; s = s->getNext()) {
		os << *s << " ";
	}
	return os;
}

Stage *Pipeline::getHead()
{
	return head;
}

void Pipeline::validate()
{
	for (Stage *stage = head; stage; stage = stage->getNext())
		stage->validate();
}


/*
 * Seq methods
 */

Seq::Seq() : Stage("Seq", types::Void(), types::Seq()), src("")
{
}

void Seq::source(std::string source)
{
	src = std::move(source);
}

void Seq::codegen(Module *module, LLVMContext& context)
{
	validate();
	func = cast<Function>(module->getOrInsertFunction("seq",
	                                                  Type::getVoidTy(context),
	                                                  IntegerType::getInt8PtrTy(context),
	                                                  IntegerType::getInt32Ty(context)));

	block = BasicBlock::Create(context, "entry", func);
	IRBuilder<> builder(block);

	auto args = func->arg_begin();
	Value *seq = args++;
	Value *len = args;

	seq->setName("seq");
	len->setName("len");

	outs.insert({SeqData::RESULT, seq});
	outs.insert({SeqData::LEN, len});

	if (next)
		next->codegen(module, context);

	builder.SetInsertPoint(&func->getBasicBlockList().back());
	builder.CreateRetVoid();
}

void Seq::execute(bool debug)
{
	try {
		if (src.empty())
			throw exc::StageException("sequence source not specified", *this);

		InitializeNativeTarget();
		InitializeNativeTargetAsmPrinter();
		LLVMContext context;

		std::unique_ptr<Module> owner(new Module("seq", context));
		Module *M = owner.get();

		codegen(M, context);

		if (debug)
			errs() << *M;

		EngineBuilder EB(std::move(owner));
		EB.setMCJITMemoryManager(make_unique<SectionMemoryManager>());
		EB.setUseOrcMCJITReplacement(true);
		ExecutionEngine *eng = EB.create();
		finalize(eng);

		auto op = (SeqOp)eng->getPointerToFunction(func);

		auto *data = new io::DataBlock();
		std::ifstream input(src);

		if (!input.good())
			throw exc::IOException("could not open '" + src + "' for reading");

		do {
			data->read(input);
			const size_t len = data->len;
			for (size_t i = 0; i < len; i++) {
				op(data->block[i].data, data->block[i].len);
			}
		} while (data->len > 0);
	} catch (std::exception& e) {
		errs() << e.what() << '\n';
		throw;
	}
}
