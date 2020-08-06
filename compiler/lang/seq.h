#pragma once

#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "lang/expr.h"
#include "lang/func.h"
#include "lang/lang.h"
#include "lang/ops.h"
#include "lang/patterns.h"
#include "lang/pipeline.h"
#include "lang/var.h"

#include "types/any.h"
#include "types/array.h"
#include "types/base.h"
#include "types/func.h"
#include "types/num.h"
#include "types/optional.h"
#include "types/ptr.h"
#include "types/record.h"
#include "types/ref.h"
#include "types/seq.h"
#include "types/types.h"
#include "types/void.h"

#include "util/common.h"
#include "util/llvm.h"
#include "util/tapir.h"

#define SEQ_VERSION_MAJOR 0
#define SEQ_VERSION_MINOR 9
#define SEQ_VERSION_PATCH 7

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

// Level 7
extern int __dbg_level__;
extern int __level__;
#define DBG(l, c, ...)                                                                 \
  {                                                                                    \
    if (__dbg_level__ >= l)                                                            \
      fmt::print("{}" c "\n", std::string(2 * __level__, ' '), ##__VA_ARGS__);         \
  }
#define LOG(c, ...) DBG(0, c, ##__VA_ARGS__)
#define LOG1(c, ...) DBG(1, c, ##__VA_ARGS__)
#define LOG2(c, ...) DBG(2, c, ##__VA_ARGS__)
#define LOG3(c, ...) DBG(3, c, ##__VA_ARGS__)
#define LOG4(c, ...) DBG(4, c, ##__VA_ARGS__)
#define LOG7(c, ...) DBG(7, c, ##__VA_ARGS__)
#define LOG8(c, ...) DBG(8, c, ##__VA_ARGS__)
#define LOG9(c, ...) DBG(9, c, ##__VA_ARGS__)
#define CAST(s, T) dynamic_cast<T *>(s.get())
#pragma clang diagnostic pop

namespace seq {
namespace config {
struct Config {
  llvm::LLVMContext context;
  bool debug;
  bool profile;

  Config();
};

Config &config();
} // namespace config

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
  void runCodegenPipeline();

public:
  SeqModule();
  Block *getBlock();
  Var *getArgVar();
  void setFileName(std::string file);

  void codegen(llvm::Module *module) override;
  void verify();
  void optimize();
  void compile(const std::string &out);
  void execute(const std::vector<std::string> &args = {},
               const std::vector<std::string> &libs = {});
};

// following is largely from LLVM docs
#if LLVM_VERSION_MAJOR == 6
class SeqJIT {
private:
  std::unique_ptr<llvm::TargetMachine> target;
  const llvm::DataLayout layout;
  llvm::orc::RTDyldObjectLinkingLayer objLayer;
  llvm::orc::IRCompileLayer<decltype(objLayer), llvm::orc::SimpleCompiler> comLayer;

  using OptimizeFunction =
      std::function<std::shared_ptr<llvm::Module>(std::shared_ptr<llvm::Module>)>;

  llvm::orc::IRTransformLayer<decltype(comLayer), OptimizeFunction> optLayer;

  std::vector<Var *> globals;
  int inputNum;

  using ModuleHandle = decltype(optLayer)::ModuleHandleT;
  std::unique_ptr<llvm::Module> makeModule();
  ModuleHandle addModule(std::unique_ptr<llvm::Module> module);
  llvm::JITSymbol findSymbol(std::string name);
  void removeModule(ModuleHandle key);
  Func *makeFunc();
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

void compilationMessage(const std::string &header, const std::string &msg,
                        const std::string &file, int line, int col);

void compilationError(const std::string &msg, const std::string &file = "",
                      int line = 0, int col = 0);

void compilationWarning(const std::string &msg, const std::string &file = "",
                        int line = 0, int col = 0);

seq_int_t sliceAdjustIndices(seq_int_t length, seq_int_t *start, seq_int_t *stop,
                             seq_int_t step);

seq_int_t translateIndex(seq_int_t idx, seq_int_t len, bool clamp = false);
} // namespace seq
