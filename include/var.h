#ifndef SEQ_VAR_H
#define SEQ_VAR_H

#include <cstdlib>
#include <map>
#include "types.h"
#include "stage.h"
#include "pipeline.h"

namespace seq {
	class Seq;

	class Var {
	protected:
		bool assigned;
		Stage *stage;
		Seq *base;
	public:
		Var();
		Var(Pipeline pipeline);

		types::Type *getType() const;
		std::shared_ptr<std::map<SeqData, llvm::Value *>> outs() const;

		Pipeline operator|(Pipeline to);
		Var& operator=(Pipeline to);
	};
}

#endif /* SEQ_VAR_H */
