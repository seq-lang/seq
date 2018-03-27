#ifndef SEQ_COMMON_H
#define SEQ_COMMON_H

#include <cstdint>
#include <memory>
#include "llvm.h"
#include "seqdata.h"
#include "exc.h"

namespace seq {

	typedef int64_t seq_int_t;

	typedef struct {
		seq_int_t len;
		char *seq;
	} seq_t;

	typedef struct {
		seq_int_t len;
		void *arr;
	} arr_t;

	inline llvm::Type *seqIntLLVM(llvm::LLVMContext& context)
	{
		return llvm::IntegerType::getIntNTy(context, 8*sizeof(seq_int_t));
	}

	typedef std::shared_ptr<std::map<SeqData, llvm::Value *>> ValMap;

	inline llvm::Value *getSafe(ValMap outs, SeqData key)
	{
		auto iter = outs->find(key);

		if (iter == outs->end())
			throw exc::SeqException("pipeline error: could not find required value in outputs");

		return iter->second;
	}

}

#define SEQ_FUNC extern "C"

#endif /* SEQ_COMMON_H */
