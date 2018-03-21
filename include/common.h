#ifndef SEQ_COMMON_H
#define SEQ_COMMON_H

#include <cstdint>
#include <memory>
#include "llvm.h"
#include "seqdata.h"

namespace seq {

	typedef int64_t seq_int_t;

	inline llvm::Type *seqIntLLVM(llvm::LLVMContext& context)
	{
		return llvm::IntegerType::getIntNTy(context, 8*sizeof(seq_int_t));
	}

	typedef std::shared_ptr<std::map<SeqData, llvm::Value *>> ValMap;

}

#define SEQ_FUNC extern "C"

#endif /* SEQ_COMMON_H */
