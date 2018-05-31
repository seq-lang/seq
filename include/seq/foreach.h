#ifndef SEQ_FOREACH_H
#define SEQ_FOREACH_H

#include "stage.h"

namespace seq {
	class ForEach : public Stage {
	private:
	public:
		ForEach();
		void validate() override;
		void codegen(llvm::Module *module) override;
		static ForEach& make();
	};
}

#endif /* SEQ_FOREACH_H */
