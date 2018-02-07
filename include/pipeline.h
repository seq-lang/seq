#ifndef SEQ_PIPELINE_H
#define SEQ_PIPELINE_H

#include <iostream>

namespace seq {
	class Stage;

	class Pipeline {
	private:
		Stage *head;
		Stage *tail;
		bool linked;
		bool added;
	public:
		Pipeline(Stage *head, Stage *tail);
		Pipeline();

		Stage *getHead() const;
		void setHead(Stage *head);
		Stage *getTail() const;
		void setTail(Stage *tail);
		bool isLinked() const;
		void setLinked();
		bool isAdded() const;
		void setAdded();
		void validate();

		Pipeline& operator|(Stage& to);
		Pipeline& operator|(Pipeline& to);
	};
}

std::ostream& operator<<(std::ostream& os, seq::Pipeline& pipeline);

#endif /* SEQ_PIPELINE_H */
