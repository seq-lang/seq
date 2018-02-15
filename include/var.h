#ifndef SEQ_VAR_H
#define SEQ_VAR_H

#include <cstdlib>
#include <map>
#include "types.h"
#include "pipeline.h"

namespace seq {
	class Seq;

	class Var {
	protected:
		bool assigned;
		Pipeline *pipeline;
		Seq *base;
	public:
		Var();

		types::Type *getType() const;
		std::shared_ptr<std::map<SeqData, llvm::Value *>> outs() const;

		Pipeline& operator|(Pipeline& to);
		Pipeline& operator|(Stage& to);

		Var& operator=(Pipeline& to);
		Var& operator=(Stage& to);
	};
}

#endif /* SEQ_VAR_H */
