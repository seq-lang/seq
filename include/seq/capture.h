#ifndef SEQ_CAPTURE_H
#define SEQ_CAPTURE_H

#include "stage.h"

namespace seq {
	class Capture : public Stage {
	private:
		void *addr;
	public:
		explicit Capture(void *addr);
		void validate() override;
		void codegen(llvm::Module *module) override;
		static Capture& make(void *addr);

		Capture *clone(types::RefType *ref) override;
	};
}

#endif /* SEQ_CAPTURE_H */
