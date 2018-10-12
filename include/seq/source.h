#ifndef SEQ_SOURCE_H
#define SEQ_SOURCE_H

#include "types.h"

namespace seq {
	namespace types {
		class SourceType : public Type {
		private:
			SourceType();
		public:
			SourceType(SourceType const&)=delete;
			void operator=(SourceType const&)=delete;

			bool isAtomic() const override;
			void initOps() override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			seq_int_t size(llvm::Module *module) const override;

			static SourceType *get() noexcept;
		};

		class RawType : public Type {
		private:
			RawType();
		public:
			RawType(RawType const&)=delete;
			void operator=(RawType const&)=delete;

			bool isAtomic() const override;
			void initOps() override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			seq_int_t size(llvm::Module *module) const override;

			static RawType *get() noexcept;
		};
	}
}

#endif /* SEQ_SOURCE_H */
