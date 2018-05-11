#ifndef SEQ_COMMON_H
#define SEQ_COMMON_H

#include <cstdint>
#include <memory>
#include "llvm.h"
#include "seqdata.h"
#include "exc.h"

#define _SEQ_STR(x) #x
#define SEQ_NATIVE(x) _SEQ_STR(x), (void *)x

namespace seq {

	typedef int64_t seq_int_t;

	struct seq_t {
		seq_int_t len;
		char *seq;
	};

	struct str_t {
		seq_int_t len;
		char *str;
	};

	template<typename T = void>
	struct arr_t {
		seq_int_t len;
		T *arr;
	};

	inline llvm::IntegerType *seqIntLLVM(llvm::LLVMContext& context)
	{
		return llvm::IntegerType::getIntNTy(context, 8*sizeof(seq_int_t));
	}

	inline llvm::Constant *nullPtrLLVM(llvm::LLVMContext& context)
	{
		return llvm::ConstantPointerNull::get(
		         llvm::PointerType::getInt8PtrTy(context));
	}

	inline llvm::Constant *zeroLLVM(llvm::LLVMContext& context)
	{
		return llvm::ConstantInt::get(seqIntLLVM(context), 0);
	}

	inline llvm::Constant *oneLLVM(llvm::LLVMContext& context)
	{
		return llvm::ConstantInt::get(seqIntLLVM(context), 1);
	}

	typedef std::shared_ptr<std::map<SeqData, llvm::Value *>> ValMap;

	inline llvm::Value *getSafe(ValMap outs, SeqData key)
	{
		auto iter = outs->find(key);

		if (iter == outs->end())
			throw exc::SeqException("pipeline error: could not find required value in outputs");

		return iter->second;
	}

	inline llvm::Value *makeAlloca(llvm::Type *type, llvm::BasicBlock *block)
	{
		llvm::IRBuilder<> builder(block);
		llvm::Value *ptr = builder.CreateAlloca(type);
		return ptr;
	}

	inline llvm::Value *makeAlloca(llvm::Value *value, llvm::BasicBlock *block)
	{
		llvm::IRBuilder<> builder(block);
		llvm::Value *ptr = makeAlloca(value->getType(), block);
		builder.CreateStore(value, ptr);
		return ptr;
	}

}

#define SEQ_FUNC extern "C"

#endif /* SEQ_COMMON_H */
