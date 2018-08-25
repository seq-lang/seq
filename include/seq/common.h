#ifndef SEQ_COMMON_H
#define SEQ_COMMON_H

#include <cstdint>
#include <memory>
#include "util/llvm.h"
#include "util/seqdata.h"
#include "exc.h"

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

	inline llvm::Value *makeAlloca(llvm::Type *type, llvm::BasicBlock *block, uint64_t n=1)
	{
		llvm::LLVMContext& context = block->getContext();
		llvm::IRBuilder<> builder(block);
		llvm::Value *ptr = builder.CreateAlloca(type, llvm::ConstantInt::get(seqIntLLVM(context), n));
		return ptr;
	}

	inline llvm::Value *makeAlloca(llvm::Value *value, llvm::BasicBlock *block, uint64_t n=1)
	{
		llvm::IRBuilder<> builder(block);
		llvm::Value *ptr = makeAlloca(value->getType(), block, n);
		builder.CreateStore(value, ptr);
		return ptr;
	}

}

#define SEQ_FUNC extern "C"

#endif /* SEQ_COMMON_H */
