#include "openmp.h"

#include <iterator>

#include "sir/util/cloning.h"
#include "sir/util/irtools.h"
#include "sir/util/outlining.h"

namespace seq {
namespace ir {
namespace transform {
namespace parallel {
namespace {
const std::string ompModule = "std.openmp";

struct OMPTypes {
  types::Type *i32 = nullptr;
  types::Type *i8ptr = nullptr;
  types::Type *i32ptr = nullptr;
  types::Type *micro = nullptr;
  types::Type *routine = nullptr;
  types::Type *ident = nullptr;
  types::Type *task = nullptr;

  explicit OMPTypes(Module *M) {
    i32 = M->getIntNType(32, /*sign=*/true);
    i8ptr = M->getPointerType(M->getByteType());
    i32ptr = M->getPointerType(i32);
    micro = M->getFuncType(M->getVoidType(), {i32ptr, i32ptr});
    routine = M->getFuncType(i32, {i32ptr, i8ptr});
    ident = M->getOrRealizeType("Ident", {}, ompModule);
    task = M->getOrRealizeType("Task", {}, ompModule);
    seqassert(ident, "openmp.Ident type not found");
    seqassert(task, "openmp.Task type not found");
  }
};

Var *getVarFromOutlinedArg(Value *arg) {
  if (auto *val = cast<VarValue>(arg)) {
    return val->getVar();
  } else if (auto *val = cast<PointerValue>(arg)) {
    return val->getVar();
  } else {
    seqassert(false, "unknown outline var");
  }
  return nullptr;
}

struct StaticLoopTemplateReplacer : public util::Operator {
  Func *parent;
  CallInstr *replacement;
  Var *loopVar;

  explicit StaticLoopTemplateReplacer(Func *parent, CallInstr *replacement,
                                      Var *loopVar)
      : util::Operator(), parent(parent), replacement(replacement), loopVar(loopVar) {}

  void handle(CallInstr *v) override {
    auto *M = v->getModule();
    auto *func = util::getFunc(v->getCallee());
    if (func && func->getUnmangledName() == "_static_loop_body_stub") {
      seqassert(replacement, "unexpected double replacement");
      seqassert(v->numArgs() == 1 && isA<VarValue>(*v->begin()),
                "unexpected loop body stub");

      // the template passes the new loop var to the body stub for convenience
      auto *newLoopVar = cast<VarValue>(*v->begin())->getVar();
      auto *extras = parent->arg_back();

      std::vector<Value *> newArgs;
      unsigned i = 1;
      for (auto *arg : *replacement) {
        if (getVarFromOutlinedArg(arg)->getId() != loopVar->getId()) {
          auto *val = M->Nr<VarValue>(extras);
          newArgs.push_back(M->Nr<ExtractInstr>(val, "item" + std::to_string(i++)));
        } else {
          if (isA<VarValue>(arg)) {
            newArgs.push_back(M->Nr<VarValue>(newLoopVar));
          } else if (isA<PointerValue>(arg)) {
            newArgs.push_back(M->Nr<PointerValue>(newLoopVar));
          } else {
            seqassert(false, "unknown outline var");
          }
        }
      }

      v->replaceAll(util::call(util::getFunc(replacement->getCallee()), newArgs));
      replacement = nullptr;
    }
  }
};
} // namespace

void OpenMPPass::handle(ForFlow *v) {
  // TODO
}

void OpenMPPass::handle(ImperativeForFlow *v) {
  if (!v->isAsync())
    return;
  auto *M = v->getModule();
  auto *parent = cast<BodiedFunc>(getParentFunc());
  auto *body = cast<SeriesFlow>(v->getBody());
  if (!parent || !body)
    return;
  auto outline = util::outlineRegion(parent, body, /*allowOutflows=*/false);
  if (!outline)
    return;

  // set up args to pass fork_call
  Var *loopVar = v->getVar();
  OMPTypes types(M);

  // gather extra arguments
  std::vector<Value *> extraArgs;
  std::vector<types::Type *> extraArgTypes;
  for (auto *arg : *outline.call) {
    if (getVarFromOutlinedArg(arg)->getId() != loopVar->getId()) {
      extraArgs.push_back(arg);
      extraArgTypes.push_back(arg->getType());
    }
  }

  // template call
  auto *intType = M->getIntType();
  std::vector<types::Type *> templateFuncArgs = {
      types.i32ptr, types.i32ptr, intType,
      intType,      intType,      M->getTupleType(extraArgTypes)};
  auto *templateFunc = M->getOrRealizeFunc("_static_loop_outline_template",
                                           templateFuncArgs, {}, ompModule);
  seqassert(templateFunc, "static loop outline template not found");

  util::CloneVisitor cv(M);
  templateFunc = cast<BodiedFunc>(cv.clone(templateFunc));
  StaticLoopTemplateReplacer rep(templateFunc, outline.call, loopVar);
  templateFunc->accept(rep);

  // raw template func
  auto *templateFuncType = templateFunc->getType();
  auto *rawMethod =
      M->getOrRealizeMethod(templateFuncType, "__raw__", {templateFuncType});
  auto *rawTemplateFunc = util::call(rawMethod, {M->Nr<VarValue>(templateFunc)});

  // fork call
  std::vector<Value *> forkExtraArgs = {v->getStart(), v->getEnd(),
                                        M->getInt(v->getStep())};
  for (auto *arg : extraArgs) {
    forkExtraArgs.push_back(arg);
  }
  auto *forkExtra = util::makeTuple(forkExtraArgs, M);
  std::vector<types::Type *> forkArgTypes = {types.i8ptr,
                                             forkExtra->getType()}; // function pointer
  auto *forkFunc = M->getOrRealizeFunc("fork_call", forkArgTypes, {}, ompModule);
  seqassert(forkFunc, "fork call function not found");

  auto *fork = util::call(forkFunc, {rawTemplateFunc, forkExtra});
  v->replaceAll(fork);
}

} // namespace parallel
} // namespace transform
} // namespace ir
} // namespace seq
