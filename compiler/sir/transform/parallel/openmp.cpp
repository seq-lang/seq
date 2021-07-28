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

Value *tget(Value *tuple, unsigned idx) {
  auto *M = tuple->getModule();
  return M->Nr<ExtractInstr>(tuple, "item" + std::to_string(idx));
}

struct StaticLoopTemplateReplacer : public util::Operator {
  CallInstr *replacement;
  Var *loopVar;

  StaticLoopTemplateReplacer(CallInstr *replacement, Var *loopVar)
      : util::Operator(), replacement(replacement), loopVar(loopVar) {}

  void handle(CallInstr *v) override {
    auto *M = v->getModule();
    auto *func = util::getFunc(v->getCallee());
    if (func && func->getUnmangledName() == "_static_loop_body_stub") {
      seqassert(replacement, "unexpected double replacement");
      seqassert(v->numArgs() == 2 && isA<VarValue>(v->front()) &&
                    isA<VarValue>(v->back()),
                "unexpected loop body stub");

      // the template passes the new loop var and extra args
      // to the body stub for convenience
      auto *newLoopVar = util::getVar(v->front());
      auto *extras = util::getVar(v->back());

      std::vector<Value *> newArgs;
      unsigned next = 1;
      for (auto *arg : *replacement) {
        if (getVarFromOutlinedArg(arg)->getId() != loopVar->getId()) {
          auto *val = M->Nr<VarValue>(extras);
          newArgs.push_back(tget(val, next++));
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

struct TaskLoopBodyStubReplacer : public util::Operator {
  CallInstr *replacement;

  explicit TaskLoopBodyStubReplacer(CallInstr *replacement)
      : util::Operator(), replacement(replacement) {}

  void handle(CallInstr *v) override {
    auto *func = util::getFunc(v->getCallee());
    if (func && func->getUnmangledName() == "_task_loop_body_stub") {
      seqassert(replacement, "unexpected double replacement");
      seqassert(v->numArgs() == 2 && isA<VarValue>(v->front()) &&
                    isA<VarValue>(v->back()),
                "unexpected loop body stub");

      // the template passes the privs and shareds to the body stub for convenience
      auto *privatesTuple = v->front();
      auto *sharedsTuple = v->back();
      unsigned privatesNext = 1;
      unsigned sharedsNext = 1;
      std::vector<Value *> newArgs;

      for (auto *arg : *replacement) {
        if (isA<VarValue>(arg)) {
          newArgs.push_back(tget(privatesTuple, privatesNext++));
        } else if (isA<PointerValue>(arg)) {
          newArgs.push_back(tget(sharedsTuple, sharedsNext++));
        } else {
          seqassert(false, "unknown outline var");
        }
      }

      v->replaceAll(util::call(util::getFunc(replacement->getCallee()), newArgs));
      replacement = nullptr;
    }
  }
};

struct TaskLoopRoutineStubReplacer : public util::Operator {
  std::vector<Value *> &privates;
  std::vector<Value *> &shareds;
  CallInstr *replacement;
  Var *loopVar;

  TaskLoopRoutineStubReplacer(std::vector<Value *> &privates,
                              std::vector<Value *> &shareds, CallInstr *replacement,
                              Var *loopVar)
      : util::Operator(), privates(privates), shareds(shareds),
        replacement(replacement), loopVar(loopVar) {}

  void handle(VarValue *v) override {
    auto *M = v->getModule();
    auto *func = util::getFunc(v);
    if (func && func->getUnmangledName() == "_routine_stub") {
      util::CloneVisitor cv(M);
      auto *newRoutine = cv.clone(func);
      TaskLoopBodyStubReplacer rep(replacement);
      newRoutine->accept(rep);
      v->setVar(newRoutine);
    }
  }

  void handle(CallInstr *v) override {
    auto *M = v->getModule();
    auto *func = util::getFunc(v->getCallee());
    if (func && func->getUnmangledName() == "_insert_new_loop_var") {
      std::vector<Value *> args(v->begin(), v->end());
      seqassert(args.size() == 3, "invalid _insert_new_loop_var call found");
      auto *newLoopVar = args[0];
      auto *privatesTuple = args[1];
      auto *sharedsTuple = args[2];

      unsigned privatesNext = 1;
      unsigned sharedsNext = 1;

      bool needNewPrivates = false;
      bool needNewShareds = false;

      std::vector<Value *> newPrivates;
      std::vector<Value *> newShareds;

      for (auto *val : privates) {
        if (getVarFromOutlinedArg(val)->getId() != loopVar->getId()) {
          newPrivates.push_back(tget(privatesTuple, privatesNext));
        } else {
          newPrivates.push_back(newLoopVar);
          needNewPrivates = true;
        }
        ++privatesNext;
      }

      for (auto *val : shareds) {
        if (getVarFromOutlinedArg(val)->getId() != loopVar->getId()) {
          newShareds.push_back(tget(sharedsTuple, sharedsNext));
        } else {
          newShareds.push_back(M->Nr<PointerValue>(util::getVar(newLoopVar)));
          needNewShareds = true;
        }
        ++sharedsNext;
      }

      privatesTuple = needNewPrivates ? util::makeTuple(newPrivates, M) : privatesTuple;
      sharedsTuple = needNewShareds ? util::makeTuple(newShareds, M) : sharedsTuple;

      Value *result = util::makeTuple({privatesTuple, sharedsTuple}, M);
      v->replaceAll(result);
    }
  }
};
} // namespace

void OpenMPPass::handle(ForFlow *v) {
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

  // separate arguments into 'private' and 'shared'
  std::vector<Value *> privates, shareds;
  for (auto *arg : *outline.call) {
    const bool isPrivate = isA<VarValue>(arg);
    auto &vec = (isPrivate ? privates : shareds);
    vec.push_back(arg);
  }

  auto *privatesTuple = util::makeTuple(privates, M);
  auto *sharedsTuple = util::makeTuple(shareds, M);

  // template call
  std::vector<types::Type *> templateFuncArgs = {
      types.i32ptr, types.i32ptr,
      M->getPointerType(
          M->getTupleType({v->getIter()->getType(), privatesTuple->getType(),
                           sharedsTuple->getType()}))};
  auto *templateFunc = M->getOrRealizeFunc("_task_loop_outline_template",
                                           templateFuncArgs, {}, ompModule);
  seqassert(templateFunc, "task loop outline template not found");

  util::CloneVisitor cv(M);
  templateFunc = cast<BodiedFunc>(cv.clone(templateFunc));
  TaskLoopRoutineStubReplacer rep(privates, shareds, outline.call, loopVar);
  templateFunc->accept(rep);

  // raw template func
  auto *templateFuncType = templateFunc->getType();
  auto *rawMethod =
      M->getOrRealizeMethod(templateFuncType, "__raw__", {templateFuncType});
  auto *rawTemplateFunc = util::call(rawMethod, {M->Nr<VarValue>(templateFunc)});

  // fork call
  std::vector<Value *> forkExtraArgs = {v->getIter(), privatesTuple, sharedsTuple};
  auto *forkExtra = util::makeTuple(forkExtraArgs, M);
  std::vector<types::Type *> forkArgTypes = {types.i8ptr, forkExtra->getType()};
  auto *forkFunc = M->getOrRealizeFunc("fork_call", forkArgTypes, {}, ompModule);
  seqassert(forkFunc, "fork call function not found");

  auto *fork = util::call(forkFunc, {rawTemplateFunc, forkExtra});
  v->replaceAll(fork);
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
      types.i32ptr, types.i32ptr,
      M->getPointerType(M->getTupleType(
          {intType, intType, intType, M->getTupleType(extraArgTypes)}))};
  auto *templateFunc = M->getOrRealizeFunc("_static_loop_outline_template",
                                           templateFuncArgs, {}, ompModule);
  seqassert(templateFunc, "static loop outline template not found");

  util::CloneVisitor cv(M);
  templateFunc = cast<BodiedFunc>(cv.clone(templateFunc));
  StaticLoopTemplateReplacer rep(outline.call, loopVar);
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
  std::vector<types::Type *> forkArgTypes = {types.i8ptr, forkExtra->getType()};
  auto *forkFunc = M->getOrRealizeFunc("fork_call", forkArgTypes, {}, ompModule);
  seqassert(forkFunc, "fork call function not found");

  auto *fork = util::call(forkFunc, {rawTemplateFunc, forkExtra});
  v->replaceAll(fork);
}

} // namespace parallel
} // namespace transform
} // namespace ir
} // namespace seq
