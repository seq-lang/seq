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
		SrcInfo(const std::string file, int line, int col) :
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

		SrcInfo getSrcInfo() const
		{
			return info;
		}

		void setSrcInfo(SrcInfo info)
		{
			this->info = std::move(info);
		}

		virtual ~SrcObject()=default;
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

	namespace exc {
		class SeqException : public SrcObject, public std::runtime_error {
		public:
			SeqException(const std::string& msg, SrcInfo info) noexcept :
			    SrcObject(), std::runtime_error(msg)
			{
				setSrcInfo(info);
			}

			explicit SeqException(const std::string& msg) noexcept : SeqException(msg, {})
			{
			}

			SeqException(const SeqException& e) noexcept : SrcObject(e), std::runtime_error(e)
			{
			}
		};
	}

}

#define SEQ_TRY(block) \
    do { \
        try { \
            block \
        } catch (seq::exc::SeqException& e) { \
            if (e.getSrcInfo().line <= 0) \
                e.setSrcInfo(getSrcInfo()); \
            throw e; \
        } \
    } while (0) \

#endif /* SEQ_COMMON_H */
