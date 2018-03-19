#ifndef SEQ_NUM_H
#define SEQ_NUM_H

#include "types.h"

namespace seq {
	namespace types {

		class NumberType : public Type {
		private:
			NumberType();
		public:
			NumberType(NumberType const&)=delete;
			void operator=(NumberType const&)=delete;
			static NumberType *get();
		};

		class IntType : public Type {
		private:
			IntType();
		public:
			IntType(IntType const&)=delete;
			void operator=(IntType const&)=delete;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override;
			seq_int_t size() const override;
			static IntType *get();
		};

		class FloatType : public Type {
		private:
			FloatType();
		public:
			FloatType(FloatType const&)=delete;
			void operator=(FloatType const&)=delete;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override;
			seq_int_t size() const override;
			static FloatType *get();
		};

	}
}

#endif /* SEQ_NUM_H */
