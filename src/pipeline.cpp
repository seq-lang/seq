#include "seq/makerec.h"
#include "seq/basestage.h"
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
		validateStageRecursive(next);
	}
}

void Pipeline::validate()
{
	validateStageRecursive(head);
}

void printHelper(std::ostream& os, Stage *stage)
{
	os << *stage << " [ ";
	for (auto next : stage->getNext())
		printHelper(os, next);
	os << " ] ";
}

std::ostream& operator<<(std::ostream& os, Pipeline& pipeline)
{
	printHelper(os, pipeline.getHead());
	return os;
}

Pipeline Pipeline::operator|(Pipeline to)
{
	if (to.isAdded())
		throw exc::MultiLinkException(*to.getHead());

	to.getHead()->setBase(getHead()->getBase());
	getTail()->addNext(to.getHead());
	to.getHead()->setPrev(getTail());

	return {getHead(), to.getTail()};
}

Pipeline Pipeline::operator|(PipelineList& to)
{
	return *this | MakeRec::make(to);
}

Pipeline Pipeline::clone(types::RefType *ref)
{
	return {head->clone(ref), tail->clone(ref)};
}

PipelineList::Node::Node(Pipeline p) :
    isVar(false), p(p), v(nullptr), next(nullptr)
{
}

PipelineList::Node::Node(Var *v) :
    isVar(true), p({nullptr, nullptr}), v(v), next(nullptr)
{
}

PipelineList::Node *PipelineList::Node::clone(types::RefType *ref)
{
	if (ref->seenClone(this))
		return (Node *)ref->getClone(this);

	Node *x = isVar ? new Node(v->clone(ref)) : new Node(p.clone(ref));
	ref->addClone(this, x);
	x->next = next ? next->clone(ref) : nullptr;
	return x;
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

PipelineList *PipelineList::clone(types::RefType *ref)
{
	auto *x = new PipelineList(nullptr);
	x->head = head->clone(ref);
	x->tail = tail->clone(ref);
	return x;
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
