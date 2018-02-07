#ifndef SEQ_VAR_H
#define SEQ_VAR_H

#include "types.h"
#include "pipeline.h"
#include "seq.h"

namespace seq {
	class Var {
	protected:
		bool assigned;
		types::Type type;
		Pipeline *pipeline;
		Seq *base;
	public:
		Var();
		explicit Var(types::Type type);

		Pipeline& operator|(Pipeline& to);
		Pipeline& operator|(Stage& to);

		Var& operator=(Pipeline& to);
		Var& operator=(Stage& to);
	};
}

#endif /* SEQ_VAR_H */
