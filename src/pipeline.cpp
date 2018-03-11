#include "stage.h"
#include "exc.h"
#include "pipeline.h"

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
	tail->addNext(to.getHead());
	to.getHead()->setPrev(tail);

	return {getHead(), to.getTail()};
}

Pipeline Pipeline::operator|(PipelineList& to)
{
	for (auto *node = to.head; node; node = node->next) {
		*this | node->p;
	}

	return {getHead(), to.tail->p.getTail()};
}

PipelineList& Pipeline::operator,(Pipeline p)
{
	auto& l = *new PipelineList(*this);
	l , p;
	return l;
}

PipelineList::Node::Node(Pipeline p) :
    p(p), next(nullptr)
{
}

PipelineList::PipelineList(Pipeline p)
{
	head = tail = new Node(p);
}

PipelineList& PipelineList::operator,(Pipeline p)
{
	auto *n = new Node(p);
	tail->next = n;
	tail = n;
	return *this;
};
