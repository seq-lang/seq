#include "pipeline.h"
#include "sir/util/matching.h"

namespace seq {
namespace ir {
namespace transform {
namespace pipeline {

bool hasAttribute(const Func *func, const std::string &attribute) {
  if (auto *attr = func->getAttribute<KeyValueAttribute>()) {
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

VarValue *makeVar(Value *x, SeriesFlow *flow, BodiedFunc *parent) {
  auto *M = x->getModule();
  auto *v = M->Nr<Var>(x->getType());
  flow->push_back(M->Nr<AssignInstr>(v, x));
  parent->push_back(v);
  return M->Nr<VarValue>(v);
}

Value *alloc(types::Type *type, Value *count) {
  auto *M = type->getModule();
  auto *ptrType = M->getPointerType(type);
  return (*ptrType)(*count);
}

Value *alloc(types::Type *type, int64_t count) {
  auto *M = type->getModule();
  return alloc(type, M->getIntConstant(count));
}

template <typename... Args> SeriesFlow *series(Args... args) {
  std::vector<Value *> vals = {args...};
  if (vals.empty())
    return nullptr;
  auto *series = vals[0]->getModule()->Nr<SeriesFlow>();
  for (auto *val : vals) {
    series->push_back(val);
  }
  return series;
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

/*
 * Substitution optimizations
 */

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

/*
 * Prefetch optimization
 */

struct PrefetchFunctionTransformer : public util::LambdaValueVisitor {
  void handle(ReturnInstr *x) override {
    auto *M = x->getModule();
    x->replaceAll(M->Nr<YieldInstr>(x->getValue(), /*final=*/true));
  }

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
      if (!it->isGenerator() && hasAttribute(func, "prefetch")) {
        // transform prefetch'ing function
        auto *clone = cast<BodiedFunc>(func->clone());
        setReturnType(clone, M->getGeneratorType(getReturnType(clone)));
        clone->setGenerator();
        clone->getBody()->accept(pft);

        // make sure the arguments are in the correct order
        auto *inputType = prev->getOutputElementType();
        clone = makeStageWrapperFunc(&*it, clone, inputType);
        auto *coroType = cast<types::FuncType>(clone->getType());

        // vars
        auto *statesType = M->getArrayType(coroType->getReturnType());
        auto *width = M->getIntConstant(SCHED_WIDTH_PREFETCH);

        auto *init = M->Nr<SeriesFlow>();
        auto *parent = cast<BodiedFunc>(getParentFunc());
        assert(parent);
        auto *filled = makeVar(M->getIntConstant(0), init, parent);
        auto *next = makeVar(M->getIntConstant(0), init, parent);
        auto *states = makeVar(M->Nr<StackAllocInstr>(statesType, SCHED_WIDTH_PREFETCH),
                               init, parent);
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
                                  {nullptr, M->Nr<VarValue>(clone), states,
                                   M->Nr<PointerValue>(next->getVar()),
                                   M->Nr<PointerValue>(filled->getVar()), width,
                                   extraArgs},
                                  /*generator=*/true, /*parallel=*/false);

        // drain
        Func *drainFunc = M->getOrRealizeFunc("_dynamic_coroutine_scheduler_drain",
                                              {statesType, intType});
        std::vector<Value *> args = {states, filled};

        std::vector<PipelineFlow::Stage> drainStages = {
            {call(drainFunc, args), {}, /*generator=*/true, /*parallel=*/false}};
        *it = stage;
        for (++it; it != p->end(); ++it) {
          drainStages.push_back(it->clone());
        }

        auto *drain = M->Nr<SeriesFlow>();
        drain->push_back(M->Nr<AssignInstr>(next->getVar(), M->getIntConstant(0)));
        drain->push_back(M->Nr<PipelineFlow>(drainStages));
        insertAfter(drain);

        break; // at most one prefetch transformation per pipeline
      }
    }
    prev = &*it;
  }
}

/*
 * Inter-sequence alignment optimization
 */

struct InterAlignTypes {
  types::Type *seq;    // plain sequence type ('seq')
  types::Type *cigar;  // CIGAR string type ('CIGAR')
  types::Type *align;  // alignment result type ('Alignment')
  types::Type *params; // alignment parameters type ('InterAlignParams')
  types::Type *pair;   // sequence pair type ('SeqPair')
  types::Type *yield;  // inter-align yield type ('InterAlignYield')

  operator bool() const { return seq && cigar && align && params && pair && yield; }
};

InterAlignTypes gatherInterAlignTypes(IRModule *M) {
  return {M->getOrRealizeType("seq"),       M->getOrRealizeType("CIGAR"),
          M->getOrRealizeType("Alignment"), M->getOrRealizeType("InterAlignParams"),
          M->getOrRealizeType("SeqPair"),   M->getOrRealizeType("InterAlignYield")};
}

bool isConstOrGlobal(const Value *x) {
  if (!x) {
    return false;
  } else if (auto *v = cast<VarValue>(x)) {
    return v->getVar()->isGlobal();
  } else {
    return isConst<int64_t>(x) || isConst<bool>(x);
  }
}

bool isGlobalVar(Value *x) {
  if (auto *v = cast<VarValue>(x)) {
    return v->getVar()->isGlobal();
  }
  return false;
}

template <typename T> bool verifyAlignParams(T begin, T end) {
  enum ParamKind {
    SI, // supported int
    SB, // supported bool
    UI, // unsupported int
    UB, // unsupported bool
  };

  /*
    a: int = 2,
    b: int = 4,
    ambig: int = 0,
    gapo: int = 4,
    gape: int = 2,
    gapo2: int = -1,
    gape2: int = -1,
    bandwidth: int = -1,
    zdrop: int = -1,
    end_bonus: int = 0,
    score_only: bool = False,
    right: bool = False,
    generic_sc: bool = False,
    approx_max: bool = False,
    approx_drop: bool = False,
    ext_only: bool = False,
    rev_cigar: bool = False,
    splice: bool = False,
    splice_fwd: bool = False,
    splice_rev: bool = False,
    splice_flank: bool = False
  */

  ParamKind kinds[] = {
      SI, SI, SI, SI, SI, UI, UI, SI, SI, SI, SB,
      UB, UB, UB, UB, SB, SB, UB, UB, UB, UB,
  };

  int i = 0;
  for (auto it = begin; it != end; ++it) {
    Value *v = *it;
    switch (kinds[i]) {
    case SI:
      if (!(isGlobalVar(v) || isConst<int64_t>(v)))
        return false;
      break;
    case SB:
      if (!(isGlobalVar(v) || isConst<bool>(v)))
        return false;
      break;
    case UI:
      if (!isConst<int64_t>(v, -1))
        return false;
      break;
    case UB:
      if (!isConst<bool>(v, false))
        return false;
      break;
    default:
      assert(0);
    }
    i += 1;
  }

  return true;
}

struct InterAlignFunctionTransformer : public util::LambdaValueVisitor {
  InterAlignTypes *types;
  std::vector<Value *> params;

  void handle(ReturnInstr *x) override {
    assert(!x->getValue());
    auto *M = x->getModule();
    x->replaceAll(M->Nr<YieldInstr>(nullptr, /*final=*/true));
  }

  void handle(CallInstr *x) override {
    if (!params.empty())
      return;

    auto *M = x->getModule();
    auto *I = M->getIntType();
    auto *B = M->getBoolType();
    auto *alignFunc = M->getOrRealizeMethod(
        types->seq, "align", {types->seq, types->seq, I, I, I, I, I, I, I, I, I, I,
                              B,          B,          B, B, B, B, B, B, B, B, B});

    auto *func = cast<BodiedFunc>(getFunc(x->getFunc()));
    if (!(func && alignFunc && util::match(func, alignFunc) &&
          verifyAlignParams(x->begin() + 2, x->end())))
      return;

    params = std::vector<Value *>(x->begin(), x->end());
    Value *self = x->front();
    Value *other = *(x->begin() + 1);
    Value *extzOnly = params[17];
    Value *revCigar = params[18];
    Value *yieldValue = (*types->yield)(*self, *other, *extzOnly, *revCigar);

    auto *yieldOut = M->Nr<YieldInstr>(yieldValue);
    auto *yieldIn = M->Nr<YieldInInstr>(types->yield, /*suspend=*/false);
    auto *alnResult = M->Nr<ExtractInstr>(yieldIn, "aln");
    x->replaceAll(M->Nr<FlowInstr>(series(yieldOut), alnResult));
  }

  InterAlignFunctionTransformer(InterAlignTypes *types)
      : util::LambdaValueVisitor(), types(types), params() {}

  Value *getParams() {
    // order of 'args': a, b, ambig, gapo, gape, score_only, bandwidth, zdrop, end_bonus
    std::vector<Value *> args = {params[2], params[3],  params[4],
                                 params[5], params[6],  params[12],
                                 params[9], params[10], params[11]};
    return types->params->construct(args);
  }
};

void PipelineOptimizations::applyInterAlignOptimizations(PipelineFlow *p) {
  auto *M = p->getModule();
  auto types = gatherInterAlignTypes(M);
  if (!types) // bio module not loaded; nothing to do
    return;
  PipelineFlow::Stage *prev = nullptr;
  for (auto it = p->begin(); it != p->end(); ++it) {
    if (auto *func = cast<BodiedFunc>(getFunc(it->getFunc()))) {
      if (!it->isGenerator() && hasAttribute(func, "inter_align") &&
          getReturnType(func)->is(M->getVoidType())) {
        // transform aligning function
        InterAlignFunctionTransformer aft(&types);
        auto *clone = cast<BodiedFunc>(func->clone());
        setReturnType(clone, M->getGeneratorType(types.yield));
        clone->setGenerator();
        clone->getBody()->accept(aft);

        // make sure the arguments are in the correct order
        auto *inputType = prev->getOutputElementType();
        clone = makeStageWrapperFunc(&*it, clone, inputType);
        auto *coroType = cast<types::FuncType>(clone->getType());

        // vars
        // following defs are from bio/align.seq
        const int LEN_LIMIT = 512;
        const int MAX_SEQ_LEN8 = 128;
        const int MAX_SEQ_LEN16 = 32768;
        const unsigned W = SCHED_WIDTH_INTERALIGN;

        auto *intType = M->getIntType();
        auto *intPtrType = M->getPointerType(intType);
        auto *i32 = M->getIntNType(32, true);

        auto *parent = cast<BodiedFunc>(getParentFunc());
        assert(parent);

        auto *init = M->Nr<SeriesFlow>();
        auto *states = makeVar(alloc(coroType->getReturnType(), W), init, parent);
        auto *statesTemp = makeVar(alloc(coroType->getReturnType(), W), init, parent);
        auto *pairs = makeVar(alloc(types.pair, W), init, parent);
        auto *pairsTemp = makeVar(alloc(types.pair, W), init, parent);
        auto *bufRef = makeVar(alloc(M->getByteType(), LEN_LIMIT * W), init, parent);
        auto *bufQer = makeVar(alloc(M->getByteType(), LEN_LIMIT * W), init, parent);
        auto *hist =
            makeVar(alloc(i32, MAX_SEQ_LEN8 + MAX_SEQ_LEN16 + 32), init, parent);
        auto *filled = makeVar(M->getIntConstant(0), init, parent);
        insertBefore(init);

        auto *width = M->getIntConstant(W);
        auto *params = aft.getParams();

        std::vector<types::Type *> stageArgTypes;
        std::vector<Value *> stageArgs;
        for (auto *arg : *it) {
          if (arg) {
            stageArgs.push_back(arg);
            stageArgTypes.push_back(arg->getType());
          }
        }
        auto *extraArgs = makeTuple(stageArgs, M);

        auto *schedFunc = M->getOrRealizeFunc(
            "_interaln_scheduler",
            {inputType, coroType, pairs->getType(), bufRef->getType(),
             bufQer->getType(), states->getType(), types.params, hist->getType(),
             pairsTemp->getType(), statesTemp->getType(), intPtrType, intType,
             extraArgs->getType()});
        auto *flushFunc = M->getOrRealizeFunc(
            "_interaln_flush",
            {pairs->getType(), bufRef->getType(), bufQer->getType(), states->getType(),
             M->getIntType(), types.params, hist->getType(), pairsTemp->getType(),
             statesTemp->getType()});
        assert(schedFunc);
        assert(flushFunc);

        PipelineFlow::Stage stage(M->Nr<VarValue>(schedFunc),
                                  {nullptr, M->Nr<VarValue>(clone), pairs, bufRef,
                                   bufRef, states, params, hist, pairsTemp, statesTemp,
                                   M->Nr<PointerValue>(filled->getVar()), width,
                                   extraArgs},
                                  /*generator=*/false, /*parallel=*/false);
        *it = stage;

        auto *drain = call(flushFunc, {pairs, bufRef, bufQer, states, filled, params,
                                       hist, pairsTemp, statesTemp});
        insertAfter(drain);

        break; // at most one inter-sequence alignment transformation per pipeline
      }
    }
    prev = &*it;
  }
}

void PipelineOptimizations::handle(PipelineFlow *x) {
  applySubstitutionOptimizations(x);
  applyPrefetchOptimizations(x);
  // applyInterAlignOptimizations(x);
  // std::cout << *x << std::endl;
}

} // namespace pipeline
} // namespace transform
} // namespace ir
} // namespace seq
