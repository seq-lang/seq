#ifndef SEQ_ARRAY_H
#define SEQ_ARRAY_H

#include "types.h"

namespace seq {
	namespace types {

		class ArrayType : public Type {
		private:
			Type *baseType;
			explicit ArrayType(Type *baseType);
		public:
			ArrayType(ArrayType const&)=delete;
			void operator=(ArrayType const&)=delete;

			llvm::Value *copy(BaseFunc *base,
			                  llvm::Value *self,
			                  llvm::BasicBlock *block) override;

			void serialize(BaseFunc *base,
			               llvm::Value *self,
			               llvm::Value *fp,
			               llvm::BasicBlock *block) override;

			llvm::Value *deserialize(BaseFunc *base,
			                         llvm::Value *fp,
			                         llvm::BasicBlock *block) override;

			llvm::Value *indexLoad(BaseFunc *base,
			                       llvm::Value *self,
			                       llvm::Value *idx,
			                       llvm::BasicBlock *block) override;

			void indexStore(BaseFunc *base,
			                llvm::Value *self,
			                llvm::Value *idx,
			                llvm::Value *val,
			                llvm::BasicBlock *block) override;

			llvm::Value *indexSlice(BaseFunc *base,
			                        llvm::Value *self,
			                        llvm::Value *from,
			                        llvm::Value *to,
			                        llvm::BasicBlock *block) override;

			llvm::Value *indexSliceNoFrom(BaseFunc *base,
			                              llvm::Value *self,
			                              llvm::Value *to,
			                              llvm::BasicBlock *block) override;

			llvm::Value *indexSliceNoTo(BaseFunc *base,
			                            llvm::Value *self,
			                            llvm::Value *from,
			                            llvm::BasicBlock *block) override;

			Type *indexType() const override;

			llvm::Value *defaultValue(llvm::BasicBlock *block) override;

			llvm::Value *construct(BaseFunc *base,
			                       const std::vector<llvm::Value *>& args,
			                       llvm::BasicBlock *block) override;

			void initFields() override;

			bool isAtomic() const override;
			bool is(Type *type) const override;
			unsigned numBaseTypes() const override;
			Type *getBaseType(unsigned idx) const override;
			Type *getConstructType(const std::vector<Type *>& inTypes) override;
			llvm::Type *getLLVMType(llvm::LLVMContext& context) const override;
			seq_int_t size(llvm::Module *module) const override;
			llvm::Value *make(llvm::Value *ptr, llvm::Value *len, llvm::BasicBlock *block);
			static ArrayType *get(Type *baseType) noexcept;
			static ArrayType *get() noexcept;

			ArrayType *clone(Generic *ref) override;
		};

	}
}

#endif /* SEQ_ARRAY_H */
