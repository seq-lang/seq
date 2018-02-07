#include "exc.h"
#include "pipeline.h"

using namespace seq;

Pipeline::Pipeline(Stage *head, Stage *tail) :
    head(head), tail(tail), linked(false), added(false)
{
	if (head->isLinked())
		throw exc::MultiLinkException(*head);

	if (tail->isLinked())
		throw exc::MultiLinkException(*tail);

	if (head != tail) {
		head->addNext(tail);
		tail->setPrev(head);
	}

	head->setLinked();
	tail->setLinked();
}

Pipeline::Pipeline()=default;

Stage *Pipeline::getHead() const
{
	return head;
}

void Pipeline::setHead(Stage *newHead)
{
	if (isLinked())
		throw exc::MultiLinkException(*getHead());

	if (newHead->isLinked())
		throw exc::MultiLinkException(*newHead);

	newHead->addNext(head);
	head->setPrev(newHead);
	head = newHead;
	newHead->setLinked();
}

Stage *Pipeline::getTail() const
{
	return tail;
}

void Pipeline::setTail(Stage *newTail)
{
	if (isLinked())
		throw exc::MultiLinkException(*getHead());

	if (newTail->isLinked())
		throw exc::MultiLinkException(*newTail);

	tail->addNext(newTail);
	newTail->setPrev(tail);
	tail = newTail;
	newTail->setLinked();
}

bool Pipeline::isLinked() const
{
	return linked;
}

void Pipeline::setLinked()
{
	linked = true;
}

bool Pipeline::isAdded() const
{
	return added;
}

void Pipeline::setAdded()
{
	added = true;
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

Pipeline& Pipeline::operator|(Stage& to)
{
	setTail(&to);
	return *this;
}

Pipeline& Pipeline::operator|(Pipeline& to)
{
	if (linked)
		throw exc::MultiLinkException(*getHead());

	if (to.linked)
		throw exc::MultiLinkException(*to.getHead());

	tail->addNext(to.getHead());
	to.getHead()->setPrev(tail);
	tail = to.getTail();
	to.setLinked();
	return *this;
}
