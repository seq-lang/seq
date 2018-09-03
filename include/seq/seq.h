#ifndef SEQ_SEQ_H
#define SEQ_SEQ_H

#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>
#include <map>

#include "expr.h"
#include "func.h"
#include "lang.h"
#include "ops.h"
#include "patterns.h"
#include "source.h"
#include "var.h"

#include "types.h"
#include "any.h"
#include "base.h"
#include "void.h"
#include "seqt.h"
#include "num.h"
#include "array.h"
#include "record.h"
#include "funct.h"
#include "ref.h"
#include "optional.h"
#include "generic.h"

#include "common.h"
#include "io.h"
#include "seqgc.h"

#include "parser.h"

#include "util/llvm.h"
#include "util/seqdata.h"

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
		llvm::Function *strlenFunc;
	public:
		SeqModule();
		Block *getBlock();
		Var *getArgVar();

		void resolveTypes() override;
		void codegen(llvm::Module *module) override;
		void execute(const std::vector<std::string>& args={}, bool debug=false);
	};

	llvm::LLVMContext& getLLVMContext();

}

#endif /* SEQ_SEQ_H */
