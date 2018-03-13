#ifndef SEQ_TYPES_H
#define SEQ_TYPES_H

#include <string>
#include <map>
#include "llvm.h"
#include "seqdata.h"
#include "exc.h"
#include "util.h"

namespace seq {
	namespace types {
		class Type {
		private:
			std::string name;
			Type *parent;
			SeqData key;
		protected:
			llvm::Function *printFunc;
			std::string printName;
			void *print;
		public:
			Type(std::string name,
			     Type *parent,
			     SeqData key,
			     std::string printName,
			     void *print);
			Type(std::string name, Type *parent, SeqData key);
			Type(std::string name, Type *parent);

			virtual llvm::Type *getLLVMType(llvm::LLVMContext& context)=0;

			virtual void callPrint(std::shared_ptr<std::map<SeqData, llvm::Value *>> outs, llvm::BasicBlock *block);

			virtual void finalizePrint(llvm::ExecutionEngine *eng);

			bool isChildOf(Type *type)
			{
				return (this == type) || (parent && parent->isChildOf(type));
			}

			std::string getName() const
			{
				return name;
			}

			SeqData getKey() const
			{
				return key;
			}

			virtual uint32_t size() const
			{
				return 0;
			}

			virtual llvm::Value *codegenLoad(llvm::Module *module,
			                                 llvm::LLVMContext& context,
			                                 llvm::BasicBlock *block,
			                                 llvm::Value *ptr,
			                                 llvm::Value *idx);

			virtual void codegenStore(llvm::Module *module,
			                          llvm::LLVMContext& context,
			                          llvm::BasicBlock *block,
			                          llvm::Value *ptr,
			                          llvm::Value *idx,
			                          llvm::Value *val);
		};

		class Any : public Type {
			Any() : Type("Any", nullptr) {};
		public:
			Any(Any const&)=delete;
			void operator=(Any const&)=delete;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override
			{
				throw exc::SeqException("cannot instantiate Any class");
			}

			static Any *get()
			{
				static Any instance;
				return &instance;
			}
		};

		class Base : public Type {
		private:
			Base() : Type("Base", Any::get()) {};
		public:
			Base(Base const&)=delete;
			void operator=(Base const&)=delete;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override
			{
				throw exc::SeqException("cannot instantiate Base class");
			}

			static Base *get()
			{
				static Base instance;
				return &instance;
			}
		};

		class Void : public Type {
		private:
			Void() : Type("Void", Any::get()) {};
		public:
			Void(Void const&)=delete;
			void operator=(Void const&)=delete;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override
			{
				return llvm::Type::getVoidTy(context);
			}

			static Void *get()
			{
				static Void instance;
				return &instance;
			}
		};

		class Seq : public Type {
		private:
			Seq() : Type("Seq", Base::get(), SeqData::SEQ, "print", (void *)&util::print) {};
		public:
			Seq(Seq const&)=delete;
			void operator=(Seq const&)=delete;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override
			{
				return llvm::Type::getInt8Ty(context);
			}

			void callPrint(std::shared_ptr<std::map<SeqData, llvm::Value *>> outs, llvm::BasicBlock *block) override;

			static Seq *get()
			{
				static Seq instance;
				return &instance;
			}

			uint32_t size() const override
			{
				return sizeof(char *);
			}
		};

		template<unsigned K>
		class Mer : public Type {
		private:
			Mer() : Type("Mer",
			             Seq::get(),
			             SeqData::SEQ,
			             "print",
			             (void *)&util::print) {};
		public:
			Mer(Mer const&)=delete;
			void operator=(Mer const&)=delete;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override
			{
				return llvm::IntegerType::getIntNTy(context, 2*K);
			}

			static Mer *get()
			{
				static Mer<K> instance;
				return &instance;
			}

			uint32_t size() const override
			{
				return K * sizeof(char);
			}
		};

		class Number : public Type {
		private:
			Number() : Type("Num", Base::get()) {};
		public:
			Number(Number const&)=delete;
			void operator=(Number const&)=delete;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override
			{
				throw exc::SeqException("cannot instantiate Number class");
			}

			static Number *get()
			{
				static Number instance;
				return &instance;
			}
		};

		class Int : public Type {
		private:
			Int() : Type("Int",
			             Number::get(),
			             SeqData::INT,
			             "print_int",
			             (void *)&util::print_int) {};
		public:
			Int(Int const&)=delete;
			void operator=(Int const&)=delete;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override
			{
				return llvm::IntegerType::getInt32Ty(context);
			}

			static Int *get()
			{
				static Int instance;
				return &instance;
			}

			uint32_t size() const override
			{
				return sizeof(int32_t);
			}
		};

		class Float : public Type {
		private:
			Float() : Type("Float",
			               Number::get(),
			               SeqData::DOUBLE,
			               "print_double",
			               (void *)&util::print_double) {};
		public:
			Float(Float const&)=delete;
			void operator=(Float const&)=delete;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override
			{
				return llvm::IntegerType::getInt32Ty(context);
			}

			static Float *get()
			{
				static Float instance;
				return &instance;
			}

			uint32_t size() const override
			{
				return sizeof(double);
			}
		};

		template<typename BASE, unsigned COUNT>
		class Array : public Type {
		private:
			Array() : Type("Array", Base::get(), SeqData::ARRAY) {}
		public:
			Array(Array const&)=delete;
			void operator=(Array const&)=delete;

			llvm::Type *getLLVMType(llvm::LLVMContext& context) override
			{
				return llvm::ArrayType::get(BASE().getLLVMType(context), COUNT);
			}

			static Array *get()
			{
				static Array<BASE,COUNT> instance;
				return &instance;
			}

			uint32_t size() const override
			{
				return COUNT * BASE::get().size();
			}
		};
	}
}

#endif /* SEQ_TYPES_H */
