#include "seq/makerec.h"
#include "seq/stage.h"
#include "seq/var.h"
#include "seq/exc.h"
#include "seq/pipeline.h"

using namespace seq;

Pipeline::Pipeline(Stage *head, Stage *tail) :
    head(head), tail(tail)
{
}

Pipeline::Pipeline()=default;

Stage *Pipeline::getHead() const
{
	return head;
}

Stage *Pipeline::getTail() const
{
	return tail;
}

bool Pipeline::isAdded() const
{
	return head->isAdded();
}

void Pipeline::setAdded()
{
	head->setAdded();
}

static void validateStageRecursive(Stage *stage)
{
	stage->validate();
	for (auto& next : stage->getNext()) {
		next->validate();
	}
}

void Pipeline::validate()
{
	validateStageRecursive(head);
}

std::ostream& operator<<(std::ostream& os, Pipeline& pipeline)
{
	for (Stage *s = pipeline.getHead(); s;) {
		os << *s << " ";

		if (!s->getNext().empty())
			s = s->getNext()[0];
	}
	return os;
}

Pipeline Pipeline::operator|(Pipeline to)
{
	to.getHead()->setBase(getHead()->getBase());
	getTail()->addNext(to.getHead());
	to.getHead()->setPrev(getTail());

	return {getHead(), to.getTail()};
}

Pipeline Pipeline::operator|(Var& to)
{
	if (!to.isAssigned())
		throw exc::SeqException("variable used before assigned");

	to.ensureConsistentBase(getHead()->getBase());
	Stage *stage = to.getStage();
	BaseStage& base = BaseStage::make(types::AnyType::get(), to.getType(getTail()), stage);
	base.outs = to.outs(getTail());
	return *this | base;
}

Pipeline Pipeline::operator|(PipelineList& to)
{
	return *this | MakeRec::make(to);
}

Pipeline Pipeline::operator<<(PipelineList& to)
{
	Pipeline last;

	for (auto *n = to.head; n; n = n->next) {
		if (n->isVar)
			last = *this | *n->v;
		else
			last = *this | n->p;
	}

	return {getHead(), last.getTail()};
}

PipelineList::Node::Node(Pipeline p) :
    isVar(false), p(p), v(nullptr), next(nullptr)
{
}

PipelineList::Node::Node(Var *v) :
    isVar(true), p({nullptr, nullptr}), v(v), next(nullptr)
{
}

PipelineList::PipelineList(Pipeline p)
{
	head = tail = new Node(p);
}

PipelineList::PipelineList(Var *v)
{
	head = tail = new Node(v);
}

void PipelineList::addNode(Node *n)
{
	tail->next = n;
	tail = n;
}

PipelineList& PipelineList::operator,(Pipeline p)
{
	addNode(new Node(p));
	return *this;
}

PipelineList& PipelineList::operator,(Var& v)
{
	addNode(new Node(&v));
	return *this;
}

PipelineList& seq::operator,(Pipeline from, Pipeline to)
{
	auto& l = *new PipelineList(from);
	l , to;
	return l;
}

PipelineList& seq::operator,(Stage& from, Pipeline to)
{
	auto& l = *new PipelineList(from);
	l , to;
	return l;
}

PipelineList& seq::operator,(Var& from, Pipeline to)
{
	auto& l = *new PipelineList(&from);
	l , to;
	return l;
}

PipelineList& seq::operator,(Pipeline from, Var& to)
{
	auto& l = *new PipelineList(from);
	l , to;
	return l;
}

PipelineList& seq::operator,(Stage& from, Var& to)
{
	auto& l = *new PipelineList(from);
	l , to;
	return l;
}

PipelineList& seq::operator,(Var& from, Var& to)
{
	auto& l = *new PipelineList(&from);
	l , to;
	return l;
}
