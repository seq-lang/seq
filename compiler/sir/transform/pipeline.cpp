#include "pipeline.h"

namespace seq {
namespace ir {
namespace transform {
namespace pipeline {

bool hasAttribute(const Func *func, const std::string &attribute) {
  if (auto *attr = func->getAttribute<FuncAttribute>()) {
    return attr->has(attribute);
  }
  return false;
}

bool isStdlibFunc(const Func *func) { return hasAttribute(func, ".stdlib"); }

CallInstr *call(Func *func, const std::vector<Value *> &args) {
  auto *M = func->getModule();
  return M->Nr<CallInstr>(M->Nr<VarValue>(func), args);
}

Value *makeTuple(const std::vector<Value *> &args, IRModule *M) {
  std::vector<types::Type *> types;
  for (auto *arg : args) {
    types.push_back(arg->getType());
  }
  auto *tupleType = M->getTupleType(types);
  auto *newFunc = M->getOrRealizeMethod(tupleType, "__new__", types);
  assert(newFunc);
  return M->Nr<CallInstr>(M->Nr<VarValue>(newFunc), args);
}

template <typename T> bool isConst(const Value *x) {
  return isA<TemplatedConstant<T>>(x);
}

template <typename T> bool isConst(const Value *x, const T &value) {
  if (auto *c = cast<TemplatedConstant<T>>(x)) {
    return c->getVal() == value;
  }
  return false;
}

template <typename T> T getConst(const Value *x) {
  auto *c = cast<TemplatedConstant<T>>(x);
  assert(c);
  return c->getVal();
}

Var *getVar(Value *x) {
  if (auto *v = cast<VarValue>(x)) {
    if (auto *var = cast<Var>(v->getVar())) {
      if (!isA<Func>(var)) {
        return var;
      }
    }
  }
  return nullptr;
}

const Var *getVar(const Value *x) {
  if (auto *v = cast<VarValue>(x)) {
    if (auto *var = cast<Var>(v->getVar())) {
      if (!isA<Func>(var)) {
        return var;
      }
    }
  }
  return nullptr;
}

Func *getFunc(Value *x) {
  if (auto *v = cast<VarValue>(x)) {
    if (auto *func = cast<Func>(v->getVar())) {
      return func;
    }
  }
  return nullptr;
}

const Func *getFunc(const Value *x) {
  if (auto *v = cast<VarValue>(x)) {
    if (auto *func = cast<Func>(v->getVar())) {
      return func;
    }
  }
  return nullptr;
}

BodiedFunc *getStdlibFunc(Value *x, const std::string &name) {
  if (auto *f = getFunc(x)) {
    if (auto *g = cast<BodiedFunc>(f)) {
      if (/*isStdlibFunc(g) &&*/ g->getUnmangledName() == name) {
        return g;
      }
    }
  }
  return nullptr;
}

const BodiedFunc *getStdlibFunc(const Value *x, const std::string &name) {
  if (auto *f = getFunc(x)) {
    if (auto *g = cast<BodiedFunc>(f)) {
      if (/*isStdlibFunc(g) &&*/ g->getUnmangledName() == name) {
        return g;
      }
    }
  }
  return nullptr;
}

types::Type *getReturnType(Func *func) {
  return cast<types::FuncType>(func->getType())->getReturnType();
}

const types::Type *getReturnType(const Func *func) {
  return cast<types::FuncType>(func->getType())->getReturnType();
}

void setReturnType(Func *func, types::Type *rType) {
  auto *M = func->getModule();
  auto *t = cast<types::FuncType>(func->getType());
  assert(t);
  std::vector<types::Type *> argTypes(t->begin(), t->end());
  func->setType(M->getFuncType(rType, argTypes));
}

void PipelineOptimizations::applySubstitutionOptimizations(PipelineFlow *p) {
  auto *M = p->getModule();

  PipelineFlow::Stage *prev = nullptr;
  auto it = p->begin();
  while (it != p->end()) {
    if (prev) {
      {
        auto *f1 = getStdlibFunc(prev->getFunc(), "kmers");
        auto *f2 = getStdlibFunc(it->getFunc(), "revcomp");
        if (f1 && f2) {
          auto *funcType = cast<types::FuncType>(f1->getType());
          auto *genType = cast<types::GeneratorType>(funcType->getReturnType());
          auto *seqType = funcType->front();
          auto *kmerType = genType->getBase();
          auto *kmersRevcompFunc = M->getOrRealizeFunc(
              "_kmers_revcomp", {seqType, M->getIntType()}, {kmerType});
          assert(kmersRevcompFunc && getReturnType(kmersRevcompFunc)->is(genType));
          cast<VarValue>(prev->getFunc())->setVar(kmersRevcompFunc);
          it = p->erase(it);
          continue;
        }
      }

      {
        auto *f1 = getStdlibFunc(prev->getFunc(), "kmers_with_pos");
        auto *f2 = getStdlibFunc(it->getFunc(), "revcomp_with_pos");
        if (f1 && f2) {
          auto *funcType = cast<types::FuncType>(f1->getType());
          auto *genType = cast<types::GeneratorType>(funcType->getReturnType());
          auto *seqType = funcType->front();
          auto *kmerType =
              cast<types::MemberedType>(genType->getBase())->back().getType();
          auto *kmersRevcompWithPosFunc = M->getOrRealizeFunc(
              "_kmers_revcomp_with_pos", {seqType, M->getIntType()}, {kmerType});
          assert(kmersRevcompWithPosFunc &&
                 getReturnType(kmersRevcompWithPosFunc)->is(genType));
          cast<VarValue>(prev->getFunc())->setVar(kmersRevcompWithPosFunc);
          it = p->erase(it);
          continue;
        }
      }

      {
        auto *f1 = getStdlibFunc(prev->getFunc(), "kmers");
        auto *f2 = getStdlibFunc(it->getFunc(), "canonical");
        if (f1 && f2 && isConst<int64_t>(prev->back(), 1)) {
          auto *funcType = cast<types::FuncType>(f1->getType());
          auto *genType = cast<types::GeneratorType>(funcType->getReturnType());
          auto *seqType = funcType->front();
          auto *kmerType = genType->getBase();
          auto *kmersCanonicalFunc =
              M->getOrRealizeFunc("_kmers_canonical", {seqType}, {kmerType});
          assert(kmersCanonicalFunc && getReturnType(kmersCanonicalFunc)->is(genType));
          cast<VarValue>(prev->getFunc())->setVar(kmersCanonicalFunc);
          prev->erase(prev->end() - 1); // remove step argument
          it = p->erase(it);
          continue;
        }
      }

      {
        auto *f1 = getStdlibFunc(prev->getFunc(), "kmers_with_pos");
        auto *f2 = getStdlibFunc(it->getFunc(), "canonical_with_pos");
        if (f1 && f2 && isConst<int64_t>(prev->back(), 1)) {
          auto *funcType = cast<types::FuncType>(f1->getType());
          auto *genType = cast<types::GeneratorType>(funcType->getReturnType());
          auto *seqType = funcType->front();
          auto *kmerType =
              cast<types::MemberedType>(genType->getBase())->back().getType();
          auto *kmersCanonicalWithPosFunc =
              M->getOrRealizeFunc("_kmers_canonical_with_pos", {seqType}, {kmerType});
          assert(kmersCanonicalWithPosFunc &&
                 getReturnType(kmersCanonicalWithPosFunc)->is(genType));
          cast<VarValue>(prev->getFunc())->setVar(kmersCanonicalWithPosFunc);
          prev->erase(prev->end() - 1); // remove step argument
          it = p->erase(it);
          continue;
        }
      }
    }
    prev = &*it;
    ++it;
  }
}

class PrefetchFunctionTransformer : public util::LambdaValueVisitor {
  void handle(CallInstr *x) override {
    auto *func = cast<BodiedFunc>(getFunc(x->getFunc()));
    if (!func || func->getUnmangledName() != "__getitem__" || x->numArgs() != 2)
      return;

    auto *M = x->getModule();
    Value *self = x->front();
    Value *key = x->back();
    types::Type *selfType = self->getType();
    types::Type *keyType = key->getType();
    Func *prefetchFunc =
        M->getOrRealizeMethod(selfType, "__prefetch__", {selfType, keyType});
    if (!prefetchFunc)
      return;

    Value *prefetch = call(prefetchFunc, {self, key});
    auto *yield = M->Nr<YieldInstr>();

    auto *series = M->Nr<SeriesFlow>();
    series->push_back(prefetch);
    series->push_back(yield);

    auto *clone = x->clone();
    see(clone); // avoid infinite loop on clone
    x->replaceAll(M->Nr<FlowInstr>(series, clone));
  }
};

BodiedFunc *makeStageWrapperFunc(PipelineFlow::Stage *stage, Func *callee,
                                 types::Type *inputType) {
  auto *M = callee->getModule();
  std::vector<types::Type *> argTypes = {inputType};
  std::vector<std::string> argNames = {"0"};
  int i = 1;
  for (auto *arg : *stage) {
    if (arg) {
      argTypes.push_back(arg->getType());
      argNames.push_back(std::to_string(i++));
    }
  }
  auto *funcType = M->getFuncType(getReturnType(callee), argTypes);
  auto *wrapperFunc = M->Nr<BodiedFunc>("__stage_wrapper");
  wrapperFunc->realize(funcType, argNames);

  // reorder arguments
  std::vector<Value *> args;
  auto it = wrapperFunc->arg_begin();
  ++it;
  for (auto *arg : *stage) {
    if (arg) {
      args.push_back(M->Nr<VarValue>(*it++));
    } else {
      args.push_back(M->Nr<VarValue>(wrapperFunc->arg_front()));
    }
  }

  auto *body = M->Nr<SeriesFlow>();
  body->push_back(M->Nr<ReturnInstr>(call(callee, args)));
  wrapperFunc->setBody(body);
  return wrapperFunc;
}

void PipelineOptimizations::applyPrefetchOptimizations(PipelineFlow *p) {
  auto *M = p->getModule();
  PrefetchFunctionTransformer pft;
  PipelineFlow::Stage *prev = nullptr;
  for (auto it = p->begin(); it != p->end(); ++it) {
    if (auto *func = cast<BodiedFunc>(getFunc(it->getFunc()))) {
      if (hasAttribute(func, "prefetch")) {
        // transform prefetch'ing function
        auto *clone = cast<BodiedFunc>(func->clone());
        setReturnType(clone, M->getGeneratorType(getReturnType(clone)));
        clone->setGenerator();
        clone->getBody()->accept(pft);
        std::cout << "CLONE:" << std::endl << *clone << std::endl;

        // make sure the arguments are in the correct order
        auto *inputType = prev->getOutputElementType();
        clone = makeStageWrapperFunc(&*it, clone, inputType);
        auto *coroType = cast<types::FuncType>(clone->getType());

        std::cout << "CLONE:" << std::endl << *clone << std::endl;

        // vars
        auto *statesType = M->getArrayType(coroType->getReturnType());
        auto *width = M->getIntConstant(SCHED_WIDTH_PREFETCH);
        auto *filled = M->Nr<Var>(M->getIntType());
        auto *next = M->Nr<Var>(M->getIntType());
        auto *states = M->Nr<Var>(statesType);

        auto *parent = cast<BodiedFunc>(getParentFunc());
        assert(parent);
        parent->push_back(filled);
        parent->push_back(next);
        parent->push_back(states);

        // state initialization
        auto *init = M->Nr<SeriesFlow>();
        init->push_back(M->Nr<AssignInstr>(filled, M->getIntConstant(0)));
        init->push_back(M->Nr<AssignInstr>(next, M->getIntConstant(0)));
        init->push_back(M->Nr<AssignInstr>(
            states, M->Nr<StackAllocInstr>(statesType, SCHED_WIDTH_PREFETCH)));
        insertBefore(init);

        // scheduler
        auto *intType = M->getIntType();
        auto *intPtrType = M->getPointerType(intType);

        std::vector<types::Type *> stageArgTypes;
        std::vector<Value *> stageArgs;
        for (auto *arg : *it) {
          if (arg) {
            stageArgs.push_back(arg);
            stageArgTypes.push_back(arg->getType());
          }
        }
        auto *extraArgs = makeTuple(stageArgs, M);
        std::vector<types::Type *> argTypes = {
            inputType,  coroType, statesType,          intPtrType,
            intPtrType, intType,  extraArgs->getType()};

        Func *schedFunc = M->getOrRealizeFunc("_dynamic_coroutine_scheduler", argTypes);
        assert(schedFunc);
        PipelineFlow::Stage stage(M->Nr<VarValue>(schedFunc),
                                  {nullptr, M->Nr<VarValue>(clone),
                                   M->Nr<VarValue>(states), M->Nr<PointerValue>(next),
                                   M->Nr<PointerValue>(filled), width, extraArgs},
                                  /*generator=*/true, /*parallel=*/false);

        // drain
        Func *drainFunc = M->getOrRealizeFunc("_dynamic_coroutine_scheduler_drain",
                                              {statesType, intType});
        std::vector<Value *> args = {M->Nr<VarValue>(states), M->Nr<VarValue>(filled)};

        std::vector<PipelineFlow::Stage> drainStages = {
            {call(drainFunc, args), {}, /*generator=*/true, /*parallel=*/false}};
        *it = stage;
        for (++it; it != p->end(); ++it) {
          drainStages.push_back(it->clone());
        }

        auto *drain = M->Nr<SeriesFlow>();
        drain->push_back(M->Nr<AssignInstr>(next, M->getIntConstant(0)));
        drain->push_back(M->Nr<PipelineFlow>(drainStages));
        insertAfter(drain);

        break; // at most one prefetch transformation per pipeline
      }
    }
    prev = &*it;
  }
}

void PipelineOptimizations::handle(PipelineFlow *x) {
  std::cout << "BEFORE: " << *x << std::endl;
  applySubstitutionOptimizations(x);
  applyPrefetchOptimizations(x);
  std::cout << "AFTER:  " << *x << std::endl << std::endl;
}

} // namespace pipeline
} // namespace transform
} // namespace ir
} // namespace seq
