#ifndef SEQ_SEQ_H
#define SEQ_SEQ_H

#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "expr.h"
#include "func.h"
#include "lang.h"
#include "ops.h"
#include "patterns.h"
#include "pipeline.h"
#include "var.h"

#include "any.h"
#include "array.h"
#include "base.h"
#include "funct.h"
#include "generic.h"
#include "num.h"
#include "optional.h"
#include "ptr.h"
#include "record.h"
#include "ref.h"
#include "seqt.h"
#include "types.h"
#include "void.h"

#include "common.h"
#include "llvm.h"

#define SEQ_VERSION_MAJOR 0
#define SEQ_VERSION_MINOR 9
#define SEQ_VERSION_PATCH 1

namespace seq {
namespace types {
static AnyType *Any = AnyType::get();
static BaseType *Base = BaseType::get();
static VoidType *Void = VoidType::get();
static SeqType *Seq = SeqType::get();
static IntType *Int = IntType::get();
static FloatType *Float = FloatType::get();
static BoolType *Bool = BoolType::get();
static ByteType *Byte = ByteType::get();
static StrType *Str = StrType::get();
static ArrayType *Array = ArrayType::get();
static GenType *Gen = GenType::get();
} // namespace types

/**
 * Top-level module representation for programs. All parsing, type checking
 * and code generation is initiated from this class.
 */
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
  void optimize(bool debug = false);
  void compile(const std::string &out, bool debug = false);
  void execute(const std::vector<std::string> &args = {},
               const std::vector<std::string> &libs = {}, bool debug = false);
};

// following is largely from LLVM docs
#if LLVM_VERSION_MAJOR == 6
class SeqJIT {
private:
  std::unique_ptr<llvm::TargetMachine> target;
  const llvm::DataLayout layout;
  llvm::orc::RTDyldObjectLinkingLayer objLayer;
  llvm::orc::IRCompileLayer<decltype(objLayer), llvm::orc::SimpleCompiler>
      comLayer;

  using OptimizeFunction = std::function<std::shared_ptr<llvm::Module>(
      std::shared_ptr<llvm::Module>)>;

  llvm::orc::IRTransformLayer<decltype(comLayer), OptimizeFunction> optLayer;
  std::unique_ptr<llvm::orc::JITCompileCallbackManager> callbackManager;
  llvm::orc::CompileOnDemandLayer<decltype(optLayer)> codLayer;
  std::vector<Var *> globals;
  int inputNum;

  using ModuleHandle = decltype(codLayer)::ModuleHandleT;
  std::unique_ptr<llvm::Module> makeModule();
  ModuleHandle addModule(std::unique_ptr<llvm::Module> module);
  llvm::JITSymbol findSymbol(std::string name);
  void removeModule(ModuleHandle key);
  Func makeFunc();
  void exec(Func *func, std::unique_ptr<llvm::Module> module);

public:
  SeqJIT();
  static void init();
  void addFunc(Func *func);
  void addExpr(Expr *expr, bool print = true);
  Var *addVar(Expr *expr);
  void delVar(Var *var);
};
#endif

void compilationError(const std::string &msg, const std::string &file = "",
                      int line = 0, int col = 0);

void compilationWarning(const std::string &msg, const std::string &file = "",
                        int line = 0, int col = 0);

} // namespace seq

#endif /* SEQ_SEQ_H */
