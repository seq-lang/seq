#include <iostream>
#include <string>
#include <vector>
#include "util.h"
#include "exc.h"
#include "seq.h"

using namespace seq;
using namespace llvm;


/*
 * Stage pipeline
 */

Pipeline::Pipeline(Stage *head, Stage *tail) :
    head(head), tail(tail), linked(false), added(false)
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

Stage *Pipeline::getHead() const
{
	return head;
}

Stage *Pipeline::getTail() const
{
	return tail;
}

bool Pipeline::isLinked() const
{
	return linked;
}

bool Pipeline::isAdded() const
{
	return added;
}

void Pipeline::setAdded()
{
	added = true;
}

void Pipeline::validate()
{
	for (Stage *stage = head; stage; stage = stage->getNext()) {
		stage->validate();
	}
}


/*
 * Var -- intermediate variables
 */

class BaseStage : public Stage {
public:
	BaseStage(types::Type in, types::Type out) :
	    Stage("Base", in, out)
	{
	}

	void codegen(Module *module, LLVMContext& context) override
	{
		validate();
		if (next)
			next->codegen(module, context);
	}

	static BaseStage& make(types::Type in, types::Type out)
	{
		return *new BaseStage(in, out);
	}
};

Var::Var() : Var(types::Base())
{
}

Var::Var(types::Type type) :
    assigned(false), type(type), pipeline(nullptr)
{
}

Pipeline& Var::operator|(Pipeline& to)
{
	if (!assigned)
		throw exc::SeqException("variable used before assigned");

	if (to.isAdded())
		throw exc::SeqException("cannot use same pipeline twice");

	to.getHead()->setBase(base);
	BaseStage& begin = BaseStage::make(types::Void(), type);
	begin.setBase(base);
	begin.outs = pipeline->getTail()->outs;

	Pipeline& add = begin | to;
	add.setAdded();
	base->add(&add);

	return add;
}

Pipeline& Var::operator|(Stage& to)
{
	return (*this | to.asPipeline());
}

Var& Var::operator=(Pipeline& to)
{
	if (assigned)
		throw exc::SeqException("variable cannot be assigned twice");

	assigned = true;
	type = to.getTail()->getOutType();
	base = to.getHead()->getBase();
	pipeline = &to;

	return *this;
}

Var& Var::operator=(Stage& to)
{
	return *this = to.asPipeline();
}


/*
 * Seq -- interface between I/O and JIT
 */

Seq::Seq() : src(""), func(nullptr)
{
}

void Seq::source(std::string source)
{
	src = std::move(source);
}

void Seq::codegen(Module *module, LLVMContext& context)
{
	func = cast<Function>(
	         module->getOrInsertFunction(
	           "main",
	           Type::getVoidTy(context),
	           IntegerType::getInt8PtrTy(context),
	           IntegerType::getInt32Ty(context)));

	auto args = func->arg_begin();
	Value *seq = args++;
	Value *len = args;

	seq->setName("seq");
	len->setName("len");

	BasicBlock *block = BasicBlock::Create(context, "entry", func);
	IRBuilder<> builder(block);

	for(auto& pipeline : pipelines) {
		pipeline->validate();
		builder.SetInsertPoint(&func->getBasicBlockList().back());
		block = BasicBlock::Create(context, "pipeline", func);
		builder.CreateBr(block);

		BaseStage *begin;
		if ((begin = dynamic_cast<BaseStage *>(pipeline->getHead()))) {
			begin->setBase(pipeline->getHead()->getBase());
			begin->block = block;
			pipeline->getHead()->codegen(module, context);
		} else {
			begin = &BaseStage::make(types::Void(), types::Seq());
			begin->setBase(pipeline->getHead()->getBase());
			begin->block = block;
			begin->outs->insert({SeqData::SEQ, seq});
			begin->outs->insert({SeqData::LEN, len});
			pipeline = &(*begin | *pipeline);
			begin->codegen(module, context);
		}
	}

	builder.SetInsertPoint(&func->getBasicBlockList().back());
	builder.CreateRetVoid();
}

void Seq::execute(bool debug)
{
	try {
		if (src.empty())
			throw exc::SeqException("sequence source not specified");

		auto fmtIter = io::EXT_CONV.find(src.substr(src.find_last_of('.') + 1));

		if (fmtIter == io::EXT_CONV.end())
			throw exc::IOException("unknown file extension in '" + src + "'");

		io::Format fmt = fmtIter->second;

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

		for (auto& pipeline : pipelines) {
			pipeline->getHead()->finalize(eng);
		}

		auto op = (SeqOp)eng->getPointerToFunction(func);

		auto *data = new io::DataBlock();
		std::ifstream input(src);

		if (!input.good())
			throw exc::IOException("could not open '" + src + "' for reading");

		do {
			data->read(input, fmt);
			const size_t len = data->len;
			for (size_t i = 0; i < len; i++) {
				op(data->block[i].data[SeqData::SEQ], data->block[i].lens[SeqData::SEQ]);
			}
		} while (data->len > 0);

		delete data;
	} catch (std::exception& e) {
		errs() << e.what() << '\n';
		throw;
	}
}

void Seq::add(Pipeline *pipeline)
{
	pipelines.push_back(pipeline);
}

Function *Seq::getFunc() const
{
	return func;
}

Pipeline& Seq::operator|(Pipeline& to)
{
	to.getHead()->setBase(this);
	add(&to);
	return to;
}

Pipeline& Seq::operator|(Stage& to)
{
	return (*this | to.asPipeline());
}


/*
 * Stage-building helper functions
 */

Copy &stageutil::copy()
{
	return Copy::make();
}

Filter& stageutil::filter(std::string name, SeqPred op)
{
	return Filter::make(std::move(name), op);
}

Op &stageutil::op(std::string name, SeqOp op)
{
	return Op::make(std::move(name), op);
}

Print &stageutil::print()
{
	return Print::make();
}

RevComp &stageutil::revcomp()
{
	return RevComp::make();
}

Split &stageutil::split(uint32_t k, uint32_t step)
{
	return Split::make(k, step);
}

Substr &stageutil::substr(uint32_t start, uint32_t len)
{
	return Substr::make(start, len);
}
