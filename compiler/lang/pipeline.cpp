#include "lang/seq.h"
#include <queue>
#include <utility>

using namespace seq;
using namespace llvm;

PipeExpr::PipeExpr(std::vector<seq::Expr *> stages, std::vector<bool> parallel)
    : Expr(), stages(std::move(stages)), parallel(std::move(parallel)),
      intermediateTypes(), entry(nullptr), syncReg(nullptr) {
  if (this->parallel.empty())
    this->parallel = std::vector<bool>(this->stages.size(), false);
}

void PipeExpr::setParallel(unsigned which) {
  assert(which < parallel.size());
  parallel[which] = true;
}

void PipeExpr::setIntermediateTypes(std::vector<types::Type *> types) {
  assert(types.size() == stages.size());
  intermediateTypes = std::move(types);
}

// Some useful info for codegen'ing the "drain" step after prefetch transform.
struct DrainState {
  Value *states; // coroutine states buffer
  Value *filled; // how many coroutines have been added (alloca'd)

  // inter-align-specific fields
  Value *statesTemp;
  Value *pairs;
  Value *pairsTemp;
  Value *bufRef;
  Value *bufQer;
  Value *params;
  Value *hist;

  types::GenType *type;      // type of prefetch generator
  std::queue<Expr *> stages; // remaining pipeline stages
  std::queue<bool> parallel;
  std::queue<types::Type *> types;

  DrainState()
      : states(nullptr), filled(nullptr), statesTemp(nullptr), pairs(nullptr),
        pairsTemp(nullptr), bufRef(nullptr), bufQer(nullptr), params(nullptr),
        hist(nullptr), type(nullptr), stages(), parallel(), types() {}
};

struct seq::PipeExpr::PipelineCodegenState {
  types::Type *type;               // type of current pipeline output
  Value *val;                      // value of current pipeline output
  BasicBlock *block;               // current codegen block
  std::queue<Expr *> stages;       // stages left to codegen
  std::queue<bool> parallel;       // parallel ("||>") stages
  std::queue<types::Type *> types; // output type of each stage

  bool inParallel;     // whether current stage is in parallel section
  bool inLoop;         // whether we are in a loop (i.e. past some generator stage)
  bool nestedParallel; // whether this pipeline has multiple parallel stages

  DrainState drain; // drain state for prefetch and inter-align optimizations

  PipelineCodegenState(BasicBlock *block, std::queue<Expr *> stages,
                       std::queue<bool> parallel, std::queue<types::Type *> types)
      : type(nullptr), val(nullptr), block(block), stages(std::move(stages)),
        parallel(), types(types), inParallel(false), inLoop(false),
        nestedParallel(false), drain() {
    int numParallels = 0;
    while (!parallel.empty()) {
      bool p = parallel.front();
      parallel.pop();
      this->parallel.push(p);
      numParallels += p ? 1 : 0;
    }
    nestedParallel = numParallels > 1;
  }

  PipelineCodegenState getDrainState(Value *val, types::Type *type, BasicBlock *block) {
    PipelineCodegenState state(block, drain.stages, drain.parallel, drain.types);
    state.val = val;
    state.type = type;
    return state;
  }
};

// Details of a stage for optimization purposes.
struct UnpackedStage {
  FuncExpr *func;
  std::vector<Expr *> args;
  types::Type *type;
  bool isCall;

  UnpackedStage(FuncExpr *func, std::vector<Expr *> args, types::Type *type,
                bool isCall)
      : func(func), args(std::move(args)), type(type), isCall(isCall) {}

  UnpackedStage(Expr *stage) : UnpackedStage(nullptr, {}, nullptr, false) {
    if (auto *funcExpr = dynamic_cast<FuncExpr *>(stage)) {
      this->func = funcExpr;
      this->args = {};
      this->isCall = false;
    } else if (auto *callExpr = dynamic_cast<CallExpr *>(stage)) {
      if (auto *funcExpr = dynamic_cast<FuncExpr *>(callExpr->getFuncExpr())) {
        this->func = funcExpr;
        this->args = callExpr->getArgs();
        this->isCall = true;
      }
    } else if (auto *partialExpr = dynamic_cast<PartialCallExpr *>(stage)) {
      if (auto *funcExpr = dynamic_cast<FuncExpr *>(partialExpr->getFuncExpr())) {
        this->func = funcExpr;
        this->args = partialExpr->getArgs();
        this->isCall = true;
      }
    }
    this->type = stage->getType();
  }

  bool matches(const std::string &name, int argCount = -1) {
    if (!func || (argCount >= 0 && args.size() != (unsigned)argCount))
      return false;
    Func *f = dynamic_cast<Func *>(func->getFunc());
    return f && f->genericName() == name && f->hasAttribute("builtin");
  }

  Expr *repack(Func *f) {
    assert(func);
    FuncExpr *newFunc = new FuncExpr(f);
    if (!isCall)
      return newFunc;

    bool isPartial = false;
    for (Expr *arg : args) {
      if (!arg) {
        isPartial = true;
        break;
      }
    }

    Expr *repacked = isPartial ? (Expr *)new PartialCallExpr(newFunc, args)
                               : new CallExpr(newFunc, args);
    repacked->setType(type);
    return repacked;
  }
};

/*
 * RevComp optimization swaps k-merization loop with revcomp loop so that
 * the latter is only done once.
 */
static void applyRevCompOptimization(std::vector<Expr *> &stages,
                                     std::vector<bool> &parallel) {
  std::vector<Expr *> stagesNew;
  std::vector<bool> parallelNew;
  unsigned i = 0;
  while (i < stages.size()) {
    if (i < stages.size() - 1) {
      UnpackedStage f1(stages[i]);
      UnpackedStage f2(stages[i + 1]);

      std::string replacement = "";
      if (f1.matches("kmers", 1) && f2.matches("revcomp"))
        replacement = "_kmers_revcomp";
      if (f1.matches("kmers_with_pos", 1) && f2.matches("revcomp_with_pos"))
        replacement = "_kmers_revcomp_with_pos";

      if (!replacement.empty()) {
        stagesNew.push_back(f1.repack(Func::getBuiltin(replacement)));
        parallelNew.push_back(parallel[i] || parallel[i + 1]);
        i += 2;
        continue;
      }
    }

    stagesNew.push_back(stages[i]);
    parallelNew.push_back(parallel[i]);
    ++i;
  }
  stages = stagesNew;
  parallel = parallelNew;
}

/*
 * Canonical k-mer optimization optimizes kmers |> canonical by using two
 * sliding windows to iterate over both forward and reverse k-mers
 * simultaneously. Currently only works with step=1 but probably possible to
 * support arbitrary step size.
 */
static void applyCanonicalKmerOptimization(std::vector<Expr *> &stages,
                                           std::vector<bool> &parallel) {
  std::vector<Expr *> stagesNew;
  std::vector<bool> parallelNew;
  unsigned i = 0;
  while (i < stages.size()) {
    if (i < stages.size() - 1) {
      UnpackedStage f1(stages[i]);
      UnpackedStage f2(stages[i + 1]);

      seq_int_t step = -1;
      if (!f1.args.empty()) {
        if (auto *e = dynamic_cast<IntExpr *>(f1.args[0]))
          step = e->value();
      }

      std::string replacement = "";
      if (step == 1 && f1.matches("kmers", 1) && f2.matches("canonical"))
        replacement = "_kmers_canonical";
      if (step == 1 && f1.matches("kmers_with_pos", 1) &&
          f2.matches("canonical_with_pos"))
        replacement = "_kmers_canonical_with_pos";

      if (!replacement.empty()) {
        stagesNew.push_back(new FuncExpr(Func::getBuiltin(replacement)));
        parallelNew.push_back(parallel[i] || parallel[i + 1]);
        i += 2;
        continue;
      }
    }

    stagesNew.push_back(stages[i]);
    parallelNew.push_back(parallel[i]);
    ++i;
  }
  stages = stagesNew;
  parallel = parallelNew;
}

// make sure params are globals or literals, since codegen'ing in function entry
// block
template <typename E = IntExpr>
static Value *
validateAndCodegenInterAlignParamExpr(Expr *e, const std::string &name, BaseFunc *base,
                                      BasicBlock *block, bool i32 = false,
                                      types::Type *expectedType = types::Int) {
  if (!e)
    throw exc::SeqException("inter-sequence alignment parameter '" + name +
                            "' not specified");
  if (!e->getType()->is(expectedType))
    throw exc::SeqException("inter-sequence alignment parameter '" + name +
                            "' is not of type " + expectedType->getName());
  bool valid = false;
  if (auto *v = dynamic_cast<VarExpr *>(e)) {
    valid = v->getVar()->isGlobal();
  } else {
    valid = (dynamic_cast<E *>(e) != nullptr);
  }
  if (!valid)
    throw exc::SeqException("inter-sequence alignment parameters must be "
                            "constants or global variables");
  Value *val = e->codegen(base, block);
  IRBuilder<> builder(block);
  return builder.CreateZExtOrTrunc(val,
                                   i32 ? builder.getInt32Ty() : builder.getInt8Ty());
}

Value *PipeExpr::validateAndCodegenInterAlignParams(
    types::GenType::InterAlignParams &paramExprs, BaseFunc *base, BasicBlock *block) {
  types::RecordType *paramsType = PipeExpr::getInterAlignParamsType();
  Value *params = paramsType->defaultValue(block);
  Value *paramVal =
      validateAndCodegenInterAlignParamExpr(paramExprs.a, "a", base, block);
  params = paramsType->setMemb(params, "a", paramVal, block);
  paramVal = validateAndCodegenInterAlignParamExpr(paramExprs.b, "b", base, block);
  params = paramsType->setMemb(params, "b", paramVal, block);
  paramVal =
      validateAndCodegenInterAlignParamExpr(paramExprs.ambig, "ambig", base, block);
  params = paramsType->setMemb(params, "ambig", paramVal, block);
  paramVal =
      validateAndCodegenInterAlignParamExpr(paramExprs.gapo, "gapo", base, block);
  params = paramsType->setMemb(params, "gapo", paramVal, block);
  paramVal =
      validateAndCodegenInterAlignParamExpr(paramExprs.gape, "gape", base, block);
  params = paramsType->setMemb(params, "gape", paramVal, block);
  paramVal = validateAndCodegenInterAlignParamExpr<BoolExpr>(
      paramExprs.score_only, "score_only", base, block, /*i32=*/false,
      /*expectedType=*/types::Bool);
  params = paramsType->setMemb(params, "score_only", paramVal, block);
  paramVal = validateAndCodegenInterAlignParamExpr(paramExprs.bandwidth, "bandwidth",
                                                   base, block, /*i32=*/true);
  params = paramsType->setMemb(params, "bandwidth", paramVal, block);
  paramVal = validateAndCodegenInterAlignParamExpr(paramExprs.zdrop, "zdrop", base,
                                                   block, /*i32=*/true);
  params = paramsType->setMemb(params, "zdrop", paramVal, block);
  paramVal = validateAndCodegenInterAlignParamExpr(paramExprs.end_bonus, "end_bonus",
                                                   base, block, /*i32=*/true);
  params = paramsType->setMemb(params, "end_bonus", paramVal, block);
  return params;
}

Value *PipeExpr::codegenPipe(BaseFunc *base, PipeExpr::PipelineCodegenState &state) {
  assert(state.stages.size() == state.parallel.size());
  if (state.stages.empty())
    return state.val;

  LLVMContext &context = state.block->getContext();
  Module *module = state.block->getModule();
  Function *func = state.block->getParent();
  TryCatch *tc = getTryCatch();

  Value *val0 = state.val;
  types::Type *type0 = state.type;

  Expr *stage = state.stages.front();
  bool parallelize = state.parallel.front();
  state.type = state.types.front();
  state.stages.pop();
  state.parallel.pop();
  state.types.pop();

  if (!state.val) {
    state.val = stage->codegen(base, state.block);
  } else {
    assert(state.val && state.type);
    ValueExpr arg(state.type, state.val);
    CallExpr call(stage, {&arg});
    call.setTryCatch(tc);
    call.setType(state.type);

    types::GenType *genType = state.type->asGen();

    if (!(genType && (genType->fromPrefetch() || genType->fromInterAlign()))) {
      state.val = call.codegen(base, state.block);
    } else if (state.drain.states) {
      throw exc::SeqException("cannot have multiple prefetch or inter-seq "
                              "alignment functions in single pipeline");
    }
  }

  types::GenType *genType = state.type->asGen();
  if (genType && genType->fromPrefetch()) {
    /*
     * Function has a prefetch statement
     *
     * We need to batch calls so that we can yield after prefetch,
     * execute a step of each call, then repeat until all calls are
     * done. This entails codegen'ing a simple dynamic scheduler at
     * this point in the pipeline, as well as a "drain" loop after
     * the pipeline to complete any remaining calls.
     */
    if (parallelize || state.inParallel)
      throw exc::SeqException(
          "parallel prefetch transformation currently not supported");

    BasicBlock *preamble = base->getPreamble();
    IRBuilder<> builder(preamble);
    Value *states =
        makeAlloca(builder.getInt8PtrTy(), preamble, PipeExpr::SCHED_WIDTH_PREFETCH);
    Value *next = makeAlloca(seqIntLLVM(context), preamble);
    Value *filled = makeAlloca(seqIntLLVM(context), preamble);

    builder.SetInsertPoint(entry);
    builder.CreateStore(zeroLLVM(context), next);
    builder.CreateStore(zeroLLVM(context), filled);

    BasicBlock *notFull = BasicBlock::Create(context, "not_full", func);
    BasicBlock *full = BasicBlock::Create(context, "full", func);
    BasicBlock *exit = BasicBlock::Create(context, "exit", func);

    builder.SetInsertPoint(state.block);
    Value *N = builder.CreateLoad(filled);
    Value *M = ConstantInt::get(seqIntLLVM(context), PipeExpr::SCHED_WIDTH_PREFETCH);
    Value *cond = builder.CreateICmpSLT(N, M);
    builder.CreateCondBr(cond, notFull, full);

    Value *task = nullptr;
    {
      ValueExpr arg(type0, val0);
      CallExpr call(stage, {&arg});
      call.setTryCatch(tc);
      call.setType(state.type);
      task = call.codegen(base, notFull);
    }

    builder.SetInsertPoint(notFull);
    Value *slot = builder.CreateGEP(states, N);
    builder.CreateStore(task, slot);
    N = builder.CreateAdd(N, oneLLVM(context));
    builder.CreateStore(N, filled);
    builder.CreateBr(exit);

    BasicBlock *full0 = full;
    builder.SetInsertPoint(full);
    Value *nextVal = builder.CreateLoad(next);
    slot = builder.CreateGEP(states, nextVal);
    Value *gen = builder.CreateLoad(slot);

    if (tc) {
      BasicBlock *normal = BasicBlock::Create(context, "normal", func);
      BasicBlock *unwind = tc->getExceptionBlock();
      genType->resume(gen, full, normal, unwind);
      full = normal;
    } else {
      genType->resume(gen, full, nullptr, nullptr);
    }

    Value *done = genType->done(gen, full);
    BasicBlock *genDone = BasicBlock::Create(context, "done", func);
    BasicBlock *genNotDone = BasicBlock::Create(context, "not_done", func);
    builder.SetInsertPoint(full);
    builder.CreateCondBr(done, genDone, genNotDone);

    state.type = genType->getBaseType(0);
    state.val = state.type->is(types::Void) ? nullptr : genType->promise(gen, genDone);

    // store the current state for the drain step:
    state.drain.states = states;
    state.drain.filled = filled;
    state.drain.type = genType;
    state.drain.stages = state.stages;
    state.drain.parallel = state.parallel;
    state.drain.types = state.types;

    state.block = genDone;
    codegenPipe(base, state);
    genDone = state.block;
    genType->destroy(gen, genDone);

    {
      ValueExpr arg(type0, val0);
      CallExpr call(stage, {&arg});
      call.setTryCatch(tc);
      call.setType(state.type);
      task = call.codegen(base, genDone);
    }

    builder.SetInsertPoint(genDone);
    builder.CreateStore(task, slot);
    builder.CreateBr(exit);

    builder.SetInsertPoint(genNotDone);
    nextVal = builder.CreateAdd(nextVal, oneLLVM(context));
    nextVal = builder.CreateAnd(
        nextVal,
        ConstantInt::get(seqIntLLVM(context), PipeExpr::SCHED_WIDTH_PREFETCH - 1));
    builder.CreateStore(nextVal, next);
    builder.CreateBr(full0);

    state.block = exit;
    return nullptr;
  } else if (genType && genType->fromInterAlign()) {
    /*
     * Function performs inter-sequence alignment
     *
     * Ensuing transformation is similar to prefetch, except we batch
     * sequences to be aligned with inter-sequence alignment, and send the
     * scores back to the coroutines when finished. Dynamic scheduling
     * of coroutines is very similar to prefetch's.
     */
    // we unparallelize inter-seq alignment pipelines (see below)
    assert(!parallelize);
    assert(!state.inParallel);

    BasicBlock *notFull = BasicBlock::Create(context, "not_full", func);
    BasicBlock *notFull0 = notFull;
    BasicBlock *full = BasicBlock::Create(context, "full", func);
    BasicBlock *exit = BasicBlock::Create(context, "exit", func);

    // codegen task early as it is needed before reading align params
    Value *task = nullptr;
    {
      ValueExpr arg(type0, val0);
      CallExpr call(stage, {&arg});
      call.setTryCatch(tc);
      call.setType(state.type);
      task = call.codegen(base, notFull);
    }

    // following defs are from bio/align.seq
    const int LEN_LIMIT = 512;
    const int MAX_SEQ_LEN8 = 128;
    const int MAX_SEQ_LEN16 = 32768;
    types::RecordType *pairType = PipeExpr::getInterAlignSeqPairType();
    Func *queueFunc = Func::getBuiltin("_interaln_queue");
    Func *flushFunc = Func::getBuiltin("_interaln_flush");

    Function *queue = queueFunc->getFunc(module);
    Function *flush = flushFunc->getFunc(module);
    Function *alloc = makeAllocFunc(module, /*atomic=*/false);
    Function *allocAtomic = makeAllocFunc(module, /*atomic=*/true);

    BasicBlock *preamble = base->getPreamble();
    // construct parameters
    types::GenType::InterAlignParams paramExprs = genType->getAlignParams();
    Value *params =
        PipeExpr::validateAndCodegenInterAlignParams(paramExprs, base, entry);

    IRBuilder<> builder(preamble);
    const unsigned W = PipeExpr::SCHED_WIDTH_INTERALIGN;
    Value *statesSize = builder.getInt64(genType->size(module) * W);
    Value *bufRefSize = builder.getInt64(LEN_LIMIT * W);
    Value *bufQerSize = builder.getInt64(LEN_LIMIT * W);
    Value *pairsSize = builder.getInt64(pairType->size(module) * W);
    Value *histSize = builder.getInt64((MAX_SEQ_LEN8 + MAX_SEQ_LEN16 + 32) * 4);
    Value *states = builder.CreateCall(alloc, statesSize);
    states =
        builder.CreateBitCast(states, genType->getLLVMType(context)->getPointerTo());
    Value *statesTemp = builder.CreateCall(alloc, statesSize);
    statesTemp = builder.CreateBitCast(statesTemp,
                                       genType->getLLVMType(context)->getPointerTo());
    Value *bufRef = builder.CreateCall(allocAtomic, bufRefSize);
    Value *bufQer = builder.CreateCall(allocAtomic, bufQerSize);
    Value *pairs = builder.CreateCall(alloc, pairsSize);
    pairs =
        builder.CreateBitCast(pairs, pairType->getLLVMType(context)->getPointerTo());
    Value *pairsTemp = builder.CreateCall(alloc, pairsSize);
    pairsTemp = builder.CreateBitCast(pairsTemp,
                                      pairType->getLLVMType(context)->getPointerTo());
    Value *hist = builder.CreateCall(allocAtomic, histSize);
    hist = builder.CreateBitCast(hist, builder.getInt32Ty()->getPointerTo());
    Value *filled = makeAlloca(seqIntLLVM(context), preamble);

    builder.SetInsertPoint(entry);
    builder.CreateStore(zeroLLVM(context), filled);

    builder.SetInsertPoint(state.block);
    Value *N = builder.CreateLoad(filled);
    Value *M = ConstantInt::get(seqIntLLVM(context), W);
    Value *cond = builder.CreateICmpSLT(N, M);
    builder.CreateCondBr(cond, notFull0, full);

    builder.SetInsertPoint(full);
    N = builder.CreateCall(
        flush, {pairs, bufRef, bufQer, states, N, params, hist, pairsTemp, statesTemp});
    builder.CreateStore(N, filled);
    cond = builder.CreateICmpSLT(N, M);
    builder.CreateCondBr(cond, notFull0, full); // keep flushing while full

    // store the current state for the drain step:
    state.drain.states = states;
    state.drain.filled = filled;
    state.drain.statesTemp = statesTemp;
    state.drain.pairs = pairs;
    state.drain.pairsTemp = pairsTemp;
    state.drain.bufRef = bufRef;
    state.drain.bufQer = bufQer;
    state.drain.params = params;
    state.drain.hist = hist;
    state.drain.type = genType;
    state.drain.stages = state.stages;
    state.drain.parallel = state.parallel;
    state.drain.types = state.types;

    builder.SetInsertPoint(notFull);
    N = builder.CreateLoad(filled);
    N = builder.CreateCall(queue, {task, pairs, bufRef, bufQer, states, N, params});
    builder.CreateStore(N, filled);
    builder.CreateBr(exit);
    state.block = exit;
    return nullptr;
  } else if (genType && stage != state.stages.back()) {
    /*
     * Plain generator -- create implicit for-loop
     */
    Value *gen = state.val;
    IRBuilder<> builder(state.block);

    BasicBlock *loop = BasicBlock::Create(context, "pipe", func);
    BasicBlock *loop0 = loop;
    builder.CreateBr(loop);

    if (tc) {
      BasicBlock *normal = BasicBlock::Create(context, "normal", func);
      BasicBlock *unwind = tc->getExceptionBlock();
      genType->resume(gen, loop, normal, unwind);
      loop = normal;
    } else {
      genType->resume(gen, loop, nullptr, nullptr);
    }

    Value *cond = genType->done(gen, loop);
    BasicBlock *body = BasicBlock::Create(context, "body", func);
    builder.SetInsertPoint(loop);
    BranchInst *branch =
        builder.CreateCondBr(cond, body, body); // we set true-branch below

    state.block = body;
    state.val =
        state.type->is(types::Void) ? nullptr : genType->promise(gen, state.block);

#if SEQ_HAS_TAPIR
    if (parallelize) {
      BasicBlock *unwind = tc ? tc->getExceptionBlock() : nullptr;
      BasicBlock *detach = BasicBlock::Create(context, "detach", func);
      builder.SetInsertPoint(state.block);
      if (unwind)
        builder.CreateDetach(detach, loop0, unwind, syncReg);
      else
        builder.CreateDetach(detach, loop0, syncReg);
      state.block = detach;
    }
#endif

    BasicBlock *cleanup = BasicBlock::Create(context, "cleanup", func);
    branch->setSuccessor(0, cleanup);

    // save and restore state to codegen next stage
    bool oldInLoop = state.inLoop;
    bool oldInParallel = state.inParallel;
    state.inLoop = true;
    state.inParallel = parallelize;
    codegenPipe(base, state);
    state.inLoop = oldInLoop;
    state.inParallel = oldInParallel;

    builder.SetInsertPoint(state.block);

#if SEQ_HAS_TAPIR
    if (parallelize) {
      builder.CreateReattach(loop0, syncReg);
    } else {
#endif
      builder.CreateBr(loop0);
#if SEQ_HAS_TAPIR
    }
#endif

    genType->destroy(gen, cleanup);
    BasicBlock *exit = BasicBlock::Create(context, "exit", func);
    builder.SetInsertPoint(cleanup);
    builder.CreateBr(exit);
    state.block = exit;
    return nullptr;
  } else {
    /*
     * Simple function -- just a plain call
     */
#if SEQ_HAS_TAPIR
    if (parallelize) {
      if (!state.inLoop)
        throw exc::SeqException(
            "parallel pipeline stage is not preceded by generator stage");

      BasicBlock *unwind = tc ? tc->getExceptionBlock() : nullptr;
      BasicBlock *detach = BasicBlock::Create(context, "detach", func);
      BasicBlock *cont = BasicBlock::Create(context, "continue", func);

      IRBuilder<> builder(state.block);
      if (unwind)
        builder.CreateDetach(detach, cont, unwind, syncReg);
      else
        builder.CreateDetach(detach, cont, syncReg);

      bool oldInParallel = state.inParallel;
      state.inParallel = true;
      state.block = detach;
      codegenPipe(base, state);
      state.inParallel = oldInParallel;

      builder.SetInsertPoint(state.block);
      builder.CreateReattach(cont, syncReg);

      state.block = cont;
      return nullptr;
    }
#endif /* SEQ_HAS_TAPIR */

    return codegenPipe(base, state);
  }
}

Value *PipeExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  Module *module = block->getModule();
  Function *func = block->getParent();

  // unparallelize inter-seq alignment pipelines
  // multithreading will be handled by the alignment kernel
  bool unparallelize = false;
  for (types::Type *type : intermediateTypes) {
    if (types::GenType *genType = type->asGen()) {
      if (genType->fromInterAlign()) {
        unparallelize = true;
        break;
      }
    }
  }

  std::vector<Expr *> stages(this->stages);
  std::vector<bool> parallel(this->parallel);
  applyRevCompOptimization(stages, parallel);
  applyCanonicalKmerOptimization(stages, parallel);

  std::queue<Expr *> queue;
  std::queue<bool> parallelQueue;
  std::queue<types::Type *> typesQueue;

  for (auto *stage : stages)
    queue.push(stage);

  for (bool parallelize : parallel)
    parallelQueue.push(parallelize && !unparallelize);

  for (types::Type *type : intermediateTypes)
    typesQueue.push(type);

  entry = block;
  IRBuilder<> builder(entry);

#if SEQ_HAS_TAPIR
  Function *syncStart = Intrinsic::getDeclaration(module, Intrinsic::syncregion_start);
  syncReg = builder.CreateCall(syncStart);
#endif

  BasicBlock *start = BasicBlock::Create(context, "pipe_start", func);
  block = start;

  TryCatch *tc = getTryCatch();
  PipeExpr::PipelineCodegenState state(block, queue, parallelQueue, typesQueue);

#if SEQ_HAS_TAPIR
  // If we have nested parallelism, make sure we use a task group
  // TODO: move this to Tapir? OpenMP backend should detect nested parallelism
  // and use a task group automatically
  const bool nestedParallel = state.nestedParallel;
  getOrCreateIdentTy(module);
  auto *threadNumFunc = cast<Function>(module->getOrInsertFunction(
      "__kmpc_global_thread_num", builder.getInt32Ty(), getIdentTyPointerTy()));
  threadNumFunc->setDoesNotThrow();

  auto *taskGroupFunc = cast<Function>(
      module->getOrInsertFunction("__kmpc_taskgroup", builder.getVoidTy(),
                                  getIdentTyPointerTy(), builder.getInt32Ty()));
  taskGroupFunc->setDoesNotThrow();

  auto *endTaskGroupFunc = cast<Function>(
      module->getOrInsertFunction("__kmpc_end_taskgroup", builder.getVoidTy(),
                                  getIdentTyPointerTy(), builder.getInt32Ty()));
  endTaskGroupFunc->setDoesNotThrow();

  Value *gtid = nullptr;
  Value *ompLoc = nullptr;
  if (nestedParallel) {
    ompLoc = getOrCreateDefaultLocation(module);
    BasicBlock *preamble = base->getPreamble();
    builder.SetInsertPoint(preamble);
    gtid = builder.CreateCall(threadNumFunc, ompLoc);

    builder.SetInsertPoint(block);
    builder.CreateCall(taskGroupFunc, {ompLoc, gtid});
  }
#endif

  Value *result = codegenPipe(base, state);
  block = state.block;
  builder.SetInsertPoint(block);

  DrainState &drain = state.drain;
  if (drain.states) {
    // drain step:
    types::GenType *genType = drain.type;
    Value *states = drain.states;
    Value *filled = drain.filled;
    Value *N = builder.CreateLoad(filled);
    BasicBlock *loop = BasicBlock::Create(context, "drain", func);

    if (genType->fromPrefetch()) {
      BasicBlock *loop0 = loop;
      builder.CreateBr(loop);

      builder.SetInsertPoint(loop);
      PHINode *control = builder.CreatePHI(seqIntLLVM(context), 3);
      control->addIncoming(zeroLLVM(context), block);
      Value *cond = builder.CreateICmpSLT(control, N);
      BasicBlock *body = BasicBlock::Create(context, "body", func);
      BasicBlock *exit = BasicBlock::Create(context, "exit", func);
      builder.CreateCondBr(cond, body, exit);

      builder.SetInsertPoint(body);
      Value *genSlot = builder.CreateGEP(states, control);
      Value *gen = builder.CreateLoad(genSlot);
      Value *done = genType->done(gen, body);
      Value *next = builder.CreateAdd(control, oneLLVM(context));

      BasicBlock *notDone = BasicBlock::Create(context, "not_done", func);
      builder.CreateCondBr(done, loop0, notDone);
      control->addIncoming(next, body);

      BasicBlock *notDoneLoop = BasicBlock::Create(context, "not_done_loop", func);
      BasicBlock *notDoneLoop0 = notDoneLoop;

      builder.SetInsertPoint(notDone);
      builder.CreateBr(notDoneLoop);

      if (tc) {
        BasicBlock *normal = BasicBlock::Create(context, "normal", func);
        BasicBlock *unwind = tc->getExceptionBlock();
        genType->resume(gen, notDoneLoop, normal, unwind);
        notDoneLoop = normal;
      } else {
        genType->resume(gen, notDoneLoop, nullptr, nullptr);
      }

      BasicBlock *finalize = BasicBlock::Create(context, "finalize_gen", func);
      done = genType->done(gen, notDoneLoop);
      builder.SetInsertPoint(notDoneLoop);
      builder.CreateCondBr(done, finalize, notDoneLoop0);

      Value *val = genType->promise(gen, finalize);
      PipeExpr::PipelineCodegenState drainState =
          state.getDrainState(val, genType->getBaseType(0), finalize);
      codegenPipe(base, drainState);
      finalize = drainState.block;
      genType->destroy(gen, finalize);
      builder.SetInsertPoint(finalize);
      builder.CreateBr(loop0);
      control->addIncoming(next, finalize);

      block = exit;
    } else if (genType->fromInterAlign()) {
      Func *flushFunc = Func::getBuiltin("_interaln_flush");
      Function *flush = flushFunc->getFunc(module);

      Value *cond = builder.CreateICmpSGT(N, builder.getInt64(0));
      BasicBlock *exit = BasicBlock::Create(context, "exit", func);
      builder.CreateCondBr(cond, loop, exit);

      builder.SetInsertPoint(loop);
      N = builder.CreateCall(flush, {drain.pairs, drain.bufRef, drain.bufQer, states, N,
                                     drain.params, drain.hist, drain.pairsTemp,
                                     drain.statesTemp});
      builder.CreateStore(N, filled);
      cond = builder.CreateICmpSGT(N, builder.getInt64(0));
      builder.CreateCondBr(cond, loop, exit); // keep flushing while not empty

      block = exit;
    } else {
      assert(0);
    }
  }

#if SEQ_HAS_TAPIR
  builder.SetInsertPoint(block);
  // create sync
  if (nestedParallel) {
    builder.CreateCall(endTaskGroupFunc, {ompLoc, gtid});
  } else {
    BasicBlock *exit = BasicBlock::Create(context, "exit", func);
    builder.CreateSync(exit, syncReg);
    block = exit;
  }
#endif

  // connect entry block:
  builder.SetInsertPoint(entry);
  builder.CreateBr(start);

  return result;
}

types::RecordType *PipeExpr::getInterAlignYieldType() {
  auto *i32 = types::IntNType::get(32, true);
  static types::RecordType *cigarType = types::RecordType::get(
      {types::PtrType::get(i32), types::Int}, {"_data", "_len"}, "CIGAR");
  static types::RecordType *resultType = types::RecordType::get(
      {cigarType, types::Int}, {"_cigar", "_score"}, "Alignment");
  return types::RecordType::get({types::Seq, types::Seq, resultType},
                                {"query", "target", "alignment"}, "InterAlignYield");
}

types::RecordType *PipeExpr::getInterAlignParamsType() {
  auto *i8 = types::IntNType::get(8, true);
  auto *i32 = types::IntNType::get(32, true);
  return types::RecordType::get({i8, i8, i8, i8, i8, i8, i32, i32, i32},
                                {"a", "b", "ambig", "gapo", "gape", "score_only",
                                 "bandwidth", "zdrop", "end_bonus"},
                                "InterAlignParams");
}

types::RecordType *PipeExpr::getInterAlignSeqPairType() {
  auto *i32 = types::IntNType::get(32, true);
  return types::RecordType::get(
      {i32, i32, i32, i32, types::PtrType::get(i32), i32, i32},
      {"id", "len1", "len2", "score", "cigar", "n_cigar", "flags"}, "SeqPair");
}
