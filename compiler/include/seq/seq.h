#ifndef SEQ_SEQ_H
#define SEQ_SEQ_H

#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>

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

#include "common.h"
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
		static ByteType   *Byte   = ByteType::get();
		static StrType    *Str    = StrType::get();
		static ArrayType  *Array  = ArrayType::get();
		static GenType    *Gen    = GenType::get();
	}

	class SeqModule : public BaseFunc {
	private:
		Block *scope;
		Var *argVar;
		llvm::Function *initFunc;
		llvm::Function *strlenFunc;
		llvm::Function *makeCanonicalMainFunc(llvm::Function *realMain);
	public:
		SeqModule();
		Block *getBlock();
		Var *getArgVar();
		void setFileName(std::string file);

		void resolveTypes() override;
		void codegen(llvm::Module *module) override;
		void verify();
		void optimize(bool debug=false);
		void compile(const std::string& out, bool debug=false);
		void execute(const std::vector<std::string>& args={},
		             const std::vector<std::string>& libs={},
		             bool debug=false);
	};

#if LLVM_VERSION_MAJOR >= 7
	class SeqJIT {
	private:
		llvm::orc::ExecutionSession es;
		std::map<llvm::orc::VModuleKey, std::shared_ptr<llvm::orc::SymbolResolver>> resolvers;
		std::unique_ptr<llvm::TargetMachine> target;
		const llvm::DataLayout layout;
		llvm::orc::RTDyldObjectLinkingLayer objLayer;
		llvm::orc::IRCompileLayer<decltype(objLayer), llvm::orc::SimpleCompiler> comLayer;

		using OptimizeFunction =
		    std::function<std::unique_ptr<llvm::Module>(std::unique_ptr<llvm::Module>)>;

		llvm::orc::IRTransformLayer<decltype(comLayer), OptimizeFunction> optLayer;
		std::unique_ptr<llvm::orc::JITCompileCallbackManager> callbackManager;
		llvm::orc::CompileOnDemandLayer<decltype(optLayer)> codLayer;
		std::vector<Var *> globals;
		int inputNum;

		std::unique_ptr<llvm::Module> makeModule();
		llvm::orc::VModuleKey addModule(std::unique_ptr<llvm::Module> module);
		llvm::JITSymbol findSymbol(std::string name);
		void removeModule(llvm::orc::VModuleKey key);
		Func makeFunc();
		void exec(Func *func, std::unique_ptr<llvm::Module> module);
	public:
		SeqJIT();
		static void init();
		void addFunc(Func *func);
		void addExpr(Expr *expr, bool print=true);
		Var *addVar(Expr *expr);
		void delVar(Var *var);
	};
#endif

	void compilationError(const std::string& msg, const std::string& file, int line, int col);

}

#endif /* SEQ_SEQ_H */
