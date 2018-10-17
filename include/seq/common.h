#ifndef SEQ_COMMON_H
#define SEQ_COMMON_H

#include <cstdint>
#include <memory>
#include <stdexcept>
#include "lib.h"
#include "llvm.h"

namespace seq {

	struct SrcInfo {
		std::string file;
		int line;
		int col;
		SrcInfo(std::string file, int line, int col) :
		    file(std::move(file)), line(line), col(col) {};
		SrcInfo() : SrcInfo("", 0, 0) {};
	};

	struct SrcObject {
	private:
		SrcInfo info;
	public:
		SrcObject() : info() {}
		SrcObject(const SrcObject& s)
		{
			setSrcInfo(s.getSrcInfo());
		}

		virtual ~SrcObject()=default;

		SrcInfo getSrcInfo() const
		{
			return info;
		}

		void setSrcInfo(SrcInfo info)
		{
			this->info = std::move(info);
		}
	};

	inline llvm::IntegerType *seqIntLLVM(llvm::LLVMContext& context)
	{
		return llvm::IntegerType::getIntNTy(context, 8*sizeof(seq_int_t));
	}

	inline llvm::Constant *nullPtrLLVM(llvm::LLVMContext& context)
	{
		return llvm::ConstantPointerNull::get(llvm::PointerType::getInt8PtrTy(context));
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

	inline void makeMemCpy(llvm::Value *dst,
	                       llvm::Value *src,
	                       llvm::Value *size,
	                       llvm::BasicBlock *block,
	                       unsigned align=0)
	{
		llvm::IRBuilder<> builder(block);
#if LLVM_VERSION_MAJOR >= 7
		builder.CreateMemCpy(dst, align, src, align, size);
#else
		builder.CreateMemCpy(dst, src, size, align);
#endif
	}

	inline void makeMemMove(llvm::Value *dst,
	                        llvm::Value *src,
	                        llvm::Value *size,
	                        llvm::BasicBlock *block,
	                        unsigned align=0)
	{
		llvm::IRBuilder<> builder(block);
#if LLVM_VERSION_MAJOR >= 7
		builder.CreateMemMove(dst, align, src, align, size);
#else
		builder.CreateMemMove(dst, src, size, align);
#endif
	}

	inline llvm::Function *makeAllocFunc(llvm::Module *module, bool atomic)
	{
		llvm::LLVMContext& context = module->getContext();
		return llvm::cast<llvm::Function>(
		         module->getOrInsertFunction(
		           atomic ? "seq_alloc_atomic" : "seq_alloc",
		           llvm::IntegerType::getInt8PtrTy(context),
		           seqIntLLVM(context)));
	}

	namespace exc {
		class SeqException : public SrcObject, public std::runtime_error {
		public:
			SeqException(const std::string& msg, SrcInfo info) noexcept :
			    SrcObject(), std::runtime_error(msg)
			{
				setSrcInfo(std::move(info));
			}

			explicit SeqException(const std::string& msg) noexcept : SeqException(msg, {})
			{
			}

			SeqException(const SeqException& e) noexcept : SrcObject(e), std::runtime_error(e)  // NOLINT
			{
			}
		};
	}

}

#endif /* SEQ_COMMON_H */
