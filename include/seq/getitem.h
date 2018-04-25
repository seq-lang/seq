#ifndef SEQ_GETITEM_H
#define SEQ_GETITEM_H

#include "stage.h"

namespace seq {
	class GetItem : public Stage {
	private:
		seq_int_t idx;
	public:
		explicit GetItem(seq_int_t idx);
		void codegen(llvm::Module *module) override;
		void validate() override;
		static GetItem& make(seq_int_t idx);
	};
}

#endif /* SEQ_GETITEM_H */
