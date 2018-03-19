#ifndef SEQ_COMMON_H
#define SEQ_COMMON_H

#include <cstdint>
#include "llvm.h"

namespace seq {

	typedef int64_t seq_int_t;

	inline llvm::Type *seqIntLLVM(llvm::LLVMContext& context)
	{
		return llvm::IntegerType::getIntNTy(context, 8*sizeof(seq_int_t));
	}

}

#endif /* SEQ_COMMON_H */
