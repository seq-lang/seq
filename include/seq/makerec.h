#ifndef SEQ_MAKEREC_H
#define SEQ_MAKEREC_H

#include "stage.h"
#include "pipeline.h"

namespace seq {
	class MakeRec : public Stage {
	private:
		bool validated;
		PipelineList& pl;
	public:
		explicit MakeRec(PipelineList& pl);
		void validate() override;
		void codegen(llvm::Module *module) override;
		void finalize(llvm::Module *module, llvm::ExecutionEngine *eng) override;
		static MakeRec& make(PipelineList& pl);

		MakeRec *clone(types::RefType *ref) override;
	};
}

#endif /* SEQ_MAKEREC_H */
