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
			static NumberType *get() noexcept;
		};

		class IntType : public Type {
		private:
			IntType();
		public:
			IntType(IntType const&)=delete;
			void operator=(IntType const&)=delete;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			void initOps() override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			size_t size(llvm::Module *module) const override;
			static IntType *get() noexcept;
		};

		class IntNType : public Type {
		private:
			unsigned len;
			bool sign;
			IntNType(unsigned len, bool sign);
		public:
			static const unsigned MAX_LEN = 256;

			IntNType(IntNType const&)=delete;
			void operator=(IntNType const&)=delete;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			bool is(Type *type) const override;
			void initOps() override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			size_t size(llvm::Module *module) const override;
			static IntNType *get(unsigned len=32, bool sign=true);
		};

		class FloatType : public Type {
		private:
			FloatType();
		public:
			FloatType(FloatType const&)=delete;
			void operator=(FloatType const&)=delete;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			void initOps() override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			size_t size(llvm::Module *module) const override;
			static FloatType *get() noexcept;
		};

		class BoolType : public Type {
		private:
			BoolType();
		public:
			BoolType(BoolType const&)=delete;
			void operator=(BoolType const&)=delete;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			void initOps() override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			size_t size(llvm::Module *module) const override;
			static BoolType *get() noexcept;
		};

		class ByteType : public Type {
		private:
			ByteType();
		public:
			ByteType(ByteType const&)=delete;
			void operator=(BoolType const&)=delete;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			void initOps() override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			size_t size(llvm::Module *module) const override;
			static ByteType *get() noexcept;
		};

	}
}

#endif /* SEQ_NUM_H */
