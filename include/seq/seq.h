#ifndef SEQ_SEQ_H
#define SEQ_SEQ_H

#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <initializer_list>

#include "llvm.h"
#include "func.h"
#include "lang.h"
#include "source.h"
#include "var.h"
#include "patterns.h"
#include "io.h"
#include "exc.h"
#include "common.h"
#include "seqgc.h"

#include "parser.h"
#include "expr.h"
#include "numexpr.h"
#include "strexpr.h"
#include "varexpr.h"
#include "arrayexpr.h"
#include "recordexpr.h"
#include "lookupexpr.h"
#include "getelemexpr.h"
#include "callexpr.h"

namespace seq {

	namespace types {
		static AnyType&      Any    = *AnyType::get();
		static BaseType&     Base   = *BaseType::get();
		static VoidType&     Void   = *VoidType::get();
		static SeqType&      Seq    = *SeqType::get();
		static IntType&      Int    = *IntType::get();
		static FloatType&    Float  = *FloatType::get();
		static BoolType&     Bool   = *BoolType::get();
		static StrType&      Str    = *StrType::get();
		static ArrayType&    Array  = *ArrayType::get();
		static RecordType&   Record = *RecordType::get({});
		static OptionalType& Opt    = *OptionalType::get();
	}

	class SeqModule : public BaseFunc {
	private:
		Block *scope;
		Var argsVar;
		llvm::Function *initFunc;
	public:
		SeqModule();
		Block *getBlock();
		Var *getArgVar();

		void codegen(llvm::Module *module) override;
		void codegenReturn(llvm::Value *val,
		                   types::Type *type,
		                   llvm::BasicBlock*& block) override;
		void codegenYield(llvm::Value *val,
		                  types::Type *type,
		                  llvm::BasicBlock*& block) override;
		void execute(const std::vector<std::string>& args={}, bool debug=false);
	};

	llvm::LLVMContext& getLLVMContext();

}

#endif /* SEQ_SEQ_H */
