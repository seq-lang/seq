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
#include "ptr.h"
#include "generic.h"
#include "source.h"

#include "common.h"
#include "parser.h"
#include "llvm.h"

namespace seq {

	namespace types {
		static AnyType    *Any    = AnyType::get();
		static BaseType   *Base   = BaseType::get();
		static VoidType   *Void   = VoidType::get();
		static SeqType    *Seq    = SeqType::get();
		static IntType    *Int    = IntType::get();
		static FloatType  *Float  = FloatType::get();
		static BoolType   *Bool   = BoolType::get();
		static StrType    *Str    = StrType::get();
		static ArrayType  *Array  = ArrayType::get();
		static GenType    *Gen    = GenType::get();
		static SourceType *Source = SourceType::get();
		static RawType    *Raw    = RawType::get();
	}

	class SeqModule : public BaseFunc {
	private:
		Block *scope;
		Var *argVar;
		llvm::Function *initFunc;
		llvm::Function *strlenFunc;
	public:
		explicit SeqModule(std::string source="unnamed.seq");
		Block *getBlock();
		Var *getArgVar();

		void resolveTypes() override;
		void codegen(llvm::Module *module) override;
		void verify();
		void optimize(bool debug=false);
		void compile(const std::string& out, bool debug=false);
		void execute(const std::vector<std::string>& args={},
		             const std::vector<std::string>& libs={},
		             bool debug=false);
	};

}

#endif /* SEQ_SEQ_H */
