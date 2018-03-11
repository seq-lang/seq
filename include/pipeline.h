#ifndef SEQ_PIPELINE_H
#define SEQ_PIPELINE_H

#include <iostream>
#include "var.h"

namespace seq {
	class Stage;

	class PipelineList;

	class Pipeline {
	private:
		Stage *head;
		Stage *tail;
	public:
		Pipeline(Stage *head, Stage *tail);
		Pipeline();

		Stage *getHead() const;
		Stage *getTail() const;
		bool isAdded() const;
		void setAdded();
		void validate();

		Pipeline operator|(Pipeline to);
		Pipeline operator|(PipelineList& to);
		PipelineList& operator,(Pipeline p);
	};

	class PipelineList {
		struct Node {
			Pipeline p;
			Node *next;

			explicit Node(Pipeline p);
		};

	public:
		Node *head;
		Node *tail;

		explicit PipelineList(Pipeline p);
		PipelineList& operator,(Pipeline p);
	};
}

std::ostream& operator<<(std::ostream& os, seq::Pipeline& pipeline);

#endif /* SEQ_PIPELINE_H */
