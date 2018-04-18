#ifndef SEQ_PIPELINE_H
#define SEQ_PIPELINE_H

#include <iostream>

namespace seq {
	class Stage;
	class Var;
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
		Pipeline operator|(Var& to);
		Pipeline operator|(PipelineList& to);
		Pipeline operator<<(PipelineList& to);
	};

	class PipelineList {
		struct Node {
			bool isVar;
			Pipeline p;
			Var *v;
			Node *next;

			explicit Node(Pipeline p);
			explicit Node(Var *v);
		};
	private:
		void addNode(Node *n);
	public:
		Node *head;
		Node *tail;

		explicit PipelineList(Pipeline p);
		explicit PipelineList(Var *v);
		PipelineList& operator,(Pipeline p);
		PipelineList& operator,(Var& v);
	};

	PipelineList& operator,(Pipeline from, Pipeline to);
	PipelineList& operator,(Stage& from, Pipeline to);
	PipelineList& operator,(Var& from, Pipeline to);
	PipelineList& operator,(Pipeline from, Var& to);
	PipelineList& operator,(Stage& from, Var &to);
	PipelineList& operator,(Var& from, Var &to);
}

std::ostream& operator<<(std::ostream& os, seq::Pipeline& pipeline);

#endif /* SEQ_PIPELINE_H */
