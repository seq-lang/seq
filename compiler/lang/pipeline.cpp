#include "lang/seq.h"
#include <queue>
#include <utility>

using namespace seq;
using namespace llvm;

PipeExpr::PipeExpr(std::vector<seq::Expr *> stages, std::vector<bool> parallel)
    : Expr(), stages(std::move(stages)), parallel(std::move(parallel)) {
  if (this->parallel.empty())
    this->parallel = std::vector<bool>(this->stages.size(), false);
}

void PipeExpr::setParallel(unsigned which) {
  assert(which < parallel.size());
  parallel[which] = true;
}

void PipeExpr::resolveTypes() {
  for (auto *stage : stages)
    stage->resolveTypes();
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

  DrainState()
      : states(nullptr), filled(nullptr), statesTemp(nullptr), pairs(nullptr),
        pairsTemp(nullptr), bufRef(nullptr), bufQer(nullptr), params(nullptr),
        hist(nullptr), type(nullptr), stages(), parallel() {}
};

// Details of a stage for optimization purposes.
struct UnpackedStage {
  FuncExpr *func;
  std::vector<Expr *> args;
  bool isCall;

  UnpackedStage(FuncExpr *func, std::vector<Expr *> args, bool isCall)
      : func(func), args(std::move(args)), isCall(isCall) {}

  UnpackedStage(Expr *stage) : UnpackedStage(nullptr, {}, false) {
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
      if (auto *funcExpr =
              dynamic_cast<FuncExpr *>(partialExpr->getFuncExpr())) {
        this->func = funcExpr;
        this->args = partialExpr->getArgs();
        this->isCall = true;
      }
    }
  }

  bool matches(const std::string &name, int argCount = -1) {
    if (!func || (argCount >= 0 && args.size() != (unsigned)argCount))
      return false;
    Func *f = dynamic_cast<Func *>(func->getFunc());
    return f && f->genericName() == name && f->hasAttribute("builtin");
  }

  Expr *repack(Func *f) {
    assert(func);
    FuncExpr *newFunc = new FuncExpr(f, func->getTypes());
    if (!isCall)
      return newFunc;

    bool isPartial = false;
    for (Expr *arg : args) {
      if (!arg) {
        isPartial = true;
        break;
      }
    }

    if (isPartial)
      return new PartialCallExpr(newFunc, args);
    else
      return new CallExpr(newFunc, args);
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
        stagesNew.back()->resolveTypes();
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
static Value *validateAndCodegenInterAlignParamExpr(Expr *e,
                                                    const std::string &name,
                                                    BaseFunc *base,
                                                    BasicBlock *block) {
  if (!e)
    throw exc::SeqException("inter-sequence alignment parameter '" + name +
                            "' not specified");
  if (!e->getType()->is(types::Int))
    throw exc::SeqException("inter-sequence alignment parameter '" + name +
                            "' is not of type int");
  bool valid = false;
  if (auto *v = dynamic_cast<VarExpr *>(e)) {
    valid = v->getVar()->isGlobal();
  } else {
    valid = (dynamic_cast<IntExpr *>(e) != nullptr);
  }
  if (!valid)
    throw exc::SeqException("inter-sequence alignment parameters must be "
                            "constants or global variables");
  Value *val = e->codegen(base, block);
  IRBuilder<> builder(block);
  return builder.CreateTrunc(val, builder.getInt8Ty());
}

Value *PipeExpr::validateAndCodegenInterAlignParams(
    types::GenType::InterAlignParams &paramExprs, BaseFunc *base,
    BasicBlock *block) {
  types::RecordType *paramsType = PipeExpr::getInterAlignParamsType();
  Value *params = paramsType->defaultValue(block);
  Value *paramVal =
      validateAndCodegenInterAlignParamExpr(paramExprs.a, "a", base, block);
  params = paramsType->setMemb(params, "a", paramVal, block);
  paramVal =
      validateAndCodegenInterAlignParamExpr(paramExprs.b, "b", base, block);
  params = paramsType->setMemb(params, "b", paramVal, block);
  paramVal = validateAndCodegenInterAlignParamExpr(paramExprs.ambig, "ambig",
                                                   base, block);
  params = paramsType->setMemb(params, "ambig", paramVal, block);
  paramVal = validateAndCodegenInterAlignParamExpr(paramExprs.gapo, "gapo",
                                                   base, block);
  params = paramsType->setMemb(params, "gapo", paramVal, block);
  paramVal = validateAndCodegenInterAlignParamExpr(paramExprs.gape, "gape",
                                                   base, block);
  params = paramsType->setMemb(params, "gape", paramVal, block);
  paramVal = validateAndCodegenInterAlignParamExpr(paramExprs.bandwidth,
                                                   "bandwidth", base, block);
  params = paramsType->setMemb(params, "bandwidth", paramVal, block);
  paramVal = validateAndCodegenInterAlignParamExpr(paramExprs.zdrop, "zdrop",
                                                   base, block);
  params = paramsType->setMemb(params, "zdrop", paramVal, block);
  paramVal = validateAndCodegenInterAlignParamExpr(paramExprs.zdrop,
                                                   "end_bonus", base, block);
  params = paramsType->setMemb(params, "end_bonus", paramVal, block);
  return params;
}

static Value *codegenPipe(BaseFunc *base,
                          Value *val,        // value of current pipeline output
                          types::Type *type, // type of current pipeline output
                          BasicBlock *entry, // block before pipeline start
                          BasicBlock *&block, // current codegen block
                          std::queue<Expr *> &stages,
                          std::queue<bool> &parallel, TryCatch *tc,
                          DrainState *drain, bool inParallel) {
  assert(stages.size() == parallel.size());
  if (stages.empty())
    return val;

  LLVMContext &context = block->getContext();
  Module *module = block->getModule();
  Function *func = block->getParent();

  Expr *stage = stages.front();
  bool parallelize = parallel.front();
  stages.pop();
  parallel.pop();

  Value *val0 = val;
  types::Type *type0 = type;

  if (!val) {
    assert(!type);
    type = stage->getType();
    val = stage->codegen(base, block);
  } else {
    assert(val && type);
    ValueExpr arg(type, val);
    CallExpr call(
        stage, {&arg}); // do this through CallExpr for type-parameter deduction
    call.setTryCatch(tc);
    type = call.getType();
    types::GenType *genType = type->asGen();

    if (!(genType && (genType->fromPrefetch() || genType->fromInterAlign()))) {
      val = call.codegen(base, block);
    } else if (drain->states) {
      throw exc::SeqException("cannot have multiple prefetch or inter-seq "
                              "alignment functions in single pipeline");
    }
  }

  types::GenType *genType = type->asGen();
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
    if (parallelize || inParallel)
      throw exc::SeqException(
          "parallel prefetch transformation currently not supported");

    BasicBlock *preamble = base->getPreamble();
    IRBuilder<> builder(preamble);
    Value *states = makeAlloca(builder.getInt8PtrTy(), preamble,
                               PipeExpr::SCHED_WIDTH_PREFETCH);
    Value *next = makeAlloca(seqIntLLVM(context), preamble);
    Value *filled = makeAlloca(seqIntLLVM(context), preamble);

    builder.SetInsertPoint(entry);
    builder.CreateStore(zeroLLVM(context), next);
    builder.CreateStore(zeroLLVM(context), filled);

    BasicBlock *notFull = BasicBlock::Create(context, "not_full", func);
    BasicBlock *full = BasicBlock::Create(context, "full", func);
    BasicBlock *exit = BasicBlock::Create(context, "exit", func);

    builder.SetInsertPoint(block);
    Value *N = builder.CreateLoad(filled);
    Value *M =
        ConstantInt::get(seqIntLLVM(context), PipeExpr::SCHED_WIDTH_PREFETCH);
    Value *cond = builder.CreateICmpSLT(N, M);
    builder.CreateCondBr(cond, notFull, full);

    Value *task = nullptr;
    {
      ValueExpr arg(type0, val0);
      CallExpr call(stage, {&arg});
      call.setTryCatch(tc);
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

    type = genType->getBaseType(0);
    val = type->is(types::Void) ? nullptr : genType->promise(gen, genDone);

    // store the current state for the drain step:
    drain->states = states;
    drain->filled = filled;
    drain->type = genType;
    drain->stages = stages;
    drain->parallel = parallel;

    codegenPipe(base, val, type, entry, genDone, stages, parallel, tc, drain,
                inParallel);
    genType->destroy(gen, genDone);

    {
      ValueExpr arg(type0, val0);
      CallExpr call(stage, {&arg});
      call.setTryCatch(tc);
      task = call.codegen(base, genDone);
    }

    builder.SetInsertPoint(genDone);
    builder.CreateStore(task, slot);
    builder.CreateBr(exit);

    builder.SetInsertPoint(genNotDone);
    nextVal = builder.CreateAdd(nextVal, oneLLVM(context));
    nextVal = builder.CreateAnd(
        nextVal, ConstantInt::get(seqIntLLVM(context),
                                  PipeExpr::SCHED_WIDTH_PREFETCH - 1));
    builder.CreateStore(nextVal, next);
    builder.CreateBr(full0);

    block = exit;
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
    assert(!inParallel);

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
      task = call.codegen(base, notFull);
    }

    // following defs are from bio/align.seq
    const int MAX_SEQ_LEN_REF = 256;
    const int MAX_SEQ_LEN_QER = 128;
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
    Value *bufRefSize = builder.getInt64(MAX_SEQ_LEN_REF * W);
    Value *bufQerSize = builder.getInt64(MAX_SEQ_LEN_QER * W);
    Value *pairsSize = builder.getInt64(pairType->size(module) * W);
    Value *histSize = builder.getInt64((MAX_SEQ_LEN8 + MAX_SEQ_LEN16 + 32) * 4);
    Value *states = builder.CreateCall(alloc, statesSize);
    states = builder.CreateBitCast(
        states, genType->getLLVMType(context)->getPointerTo());
    Value *statesTemp = builder.CreateCall(alloc, statesSize);
    statesTemp = builder.CreateBitCast(
        statesTemp, genType->getLLVMType(context)->getPointerTo());
    Value *bufRef = builder.CreateCall(allocAtomic, bufRefSize);
    Value *bufQer = builder.CreateCall(allocAtomic, bufQerSize);
    Value *pairs = builder.CreateCall(allocAtomic, pairsSize);
    pairs = builder.CreateBitCast(
        pairs, pairType->getLLVMType(context)->getPointerTo());
    Value *pairsTemp = builder.CreateCall(allocAtomic, pairsSize);
    pairsTemp = builder.CreateBitCast(
        pairsTemp, pairType->getLLVMType(context)->getPointerTo());
    Value *hist = builder.CreateCall(allocAtomic, histSize);
    hist = builder.CreateBitCast(hist, builder.getInt32Ty()->getPointerTo());
    Value *filled = makeAlloca(seqIntLLVM(context), preamble);

    builder.SetInsertPoint(entry);
    builder.CreateStore(zeroLLVM(context), filled);

    builder.SetInsertPoint(block);
    Value *N = builder.CreateLoad(filled);
    Value *M = ConstantInt::get(seqIntLLVM(context), W);
    Value *cond = builder.CreateICmpSLT(N, M);
    builder.CreateCondBr(cond, notFull0, full);

    builder.SetInsertPoint(full);
    N = builder.CreateCall(flush, {pairs, bufRef, bufQer, states, N, params,
                                   hist, pairsTemp, statesTemp});
    builder.CreateStore(N, filled);
    cond = builder.CreateICmpSLT(N, M);
    builder.CreateCondBr(cond, notFull0, full); // keep flushing while full

    // store the current state for the drain step:
    drain->states = states;
    drain->filled = filled;
    drain->statesTemp = statesTemp;
    drain->pairs = pairs;
    drain->pairsTemp = pairsTemp;
    drain->bufRef = bufRef;
    drain->bufQer = bufQer;
    drain->params = params;
    drain->hist = hist;
    drain->type = genType;
    drain->stages = stages;
    drain->parallel = parallel;

    builder.SetInsertPoint(notFull);
    N = builder.CreateLoad(filled);
    N = builder.CreateCall(queue, {task, pairs, bufRef, bufQer, states, N});
    builder.CreateStore(N, filled);
    builder.CreateBr(exit);
    block = exit;
    return nullptr;
  } else if (genType && stage != stages.back()) {
    /*
     * Plain generator -- create implicit for-loop
     */
    Value *gen = val;
    IRBuilder<> builder(block);

    BasicBlock *loop = BasicBlock::Create(context, "pipe", func);
    BasicBlock *loop0 = loop;
    builder.CreateBr(loop);

#if SEQ_HAS_TAPIR
    Value *syncReg = nullptr;
    if (parallelize) {
      builder.SetInsertPoint(loop);
      Function *syncStart =
          Intrinsic::getDeclaration(module, Intrinsic::syncregion_start);
      syncReg = builder.CreateCall(syncStart);
    }
#endif

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

    block = body;
    type = genType->getBaseType(0);
    val = type->is(types::Void) ? nullptr : genType->promise(gen, block);

#if SEQ_HAS_TAPIR
    if (parallelize) {
      BasicBlock *unwind = tc ? tc->getExceptionBlock() : nullptr;
      BasicBlock *detach = BasicBlock::Create(context, "detach", func);
      builder.SetInsertPoint(block);
      if (unwind)
        builder.CreateDetach(detach, loop0, unwind, syncReg);
      else
        builder.CreateDetach(detach, loop0, syncReg);
      block = detach;
    }
#endif

    if (parallelize)
      inParallel = true;

    codegenPipe(base, val, type, entry, block, stages, parallel, tc, drain,
                inParallel);

    builder.SetInsertPoint(block);

#if SEQ_HAS_TAPIR
    if (parallelize) {
      builder.CreateReattach(loop0, syncReg);
    } else {
#endif
      builder.CreateBr(loop0);
#if SEQ_HAS_TAPIR
    }
#endif

    BasicBlock *cleanup = BasicBlock::Create(context, "cleanup", func);
    branch->setSuccessor(0, cleanup);

#if SEQ_HAS_TAPIR
    if (parallelize) {
      BasicBlock *cleanupReal =
          BasicBlock::Create(context, "cleanup_real", func);
      builder.SetInsertPoint(cleanup);
      builder.CreateSync(cleanupReal, syncReg);
      cleanup = cleanupReal;
    }
#endif

    genType->destroy(gen, cleanup);

    BasicBlock *exit = BasicBlock::Create(context, "exit", func);
    builder.SetInsertPoint(cleanup);
    builder.CreateBr(exit);
    block = exit;
    return nullptr;
  } else {
    /*
     * Simple function -- just a plain call
     */
    if (parallelize)
      throw exc::SeqException(
          "function pipeline stage cannot be marked parallel");

    return codegenPipe(base, val, type, entry, block, stages, parallel, tc,
                       drain, inParallel);
  }
}

Value *PipeExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  Module *module = block->getModule();
  Function *func = block->getParent();

  // unparallelize inter-seq alignment pipelines
  // multithreading will be handled by the alignment kernel
  bool unparallelize = false;
  {
    types::Type *type = nullptr;
    for (auto *stage : stages) {
      if (!type) {
        type = stage->getType();
      } else {
        ValueExpr arg(type, nullptr);
        CallExpr call(
            stage,
            {&arg}); // do this through CallExpr for type-parameter deduction
        type = call.getType();
      }

      types::GenType *genType = type->asGen();
      if (genType) {
        if (genType->fromInterAlign()) {
          unparallelize = true;
          break;
        }
        type = genType->getBaseType(0);
      }
    }
  }

  std::vector<Expr *> stages(this->stages);
  std::vector<bool> parallel(this->parallel);
  applyRevCompOptimization(stages, parallel);

  std::queue<Expr *> queue;
  std::queue<bool> parallelQueue;

  for (auto *stage : stages)
    queue.push(stage);

  for (bool parallelize : parallel)
    parallelQueue.push(parallelize && !unparallelize);

  BasicBlock *entry = block;
  BasicBlock *start = BasicBlock::Create(context, "pipe_start", func);
  block = start;

  TryCatch *tc = getTryCatch();
  DrainState drain;
  Value *result = codegenPipe(base, nullptr, nullptr, entry, block, queue,
                              parallelQueue, tc, &drain, false);
  IRBuilder<> builder(block);

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

      BasicBlock *notDoneLoop =
          BasicBlock::Create(context, "not_done_loop", func);
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
      codegenPipe(base, val, genType->getBaseType(0), entry, finalize,
                  drain.stages, drain.parallel, tc, &drain, false);
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
      N = builder.CreateCall(flush, {drain.pairs, drain.bufRef, drain.bufQer,
                                     states, N, drain.params, drain.hist,
                                     drain.pairsTemp, drain.statesTemp});
      builder.CreateStore(N, filled);
      cond = builder.CreateICmpSGT(N, builder.getInt64(0));
      builder.CreateCondBr(cond, loop, exit); // keep flushing while not empty

      block = exit;
    } else {
      assert(0);
    }
  }

  // connect entry block:
  builder.SetInsertPoint(entry);
  builder.CreateBr(start);

  return result;
}

types::Type *PipeExpr::getType0() const {
  types::Type *type = nullptr;
  for (auto *stage : stages) {
    if (!type) {
      type = stage->getType();
    } else {
      ValueExpr arg(type, nullptr);
      CallExpr call(
          stage,
          {&arg}); // do this through CallExpr for type-parameter deduction
      type = call.getType();
    }

    types::GenType *genType = type->asGen();
    if (genType && (genType->fromPrefetch() || genType->fromInterAlign() ||
                    stage != stages.back()))
      return types::Void;
  }
  assert(type);
  return type;
}

PipeExpr *PipeExpr::clone(Generic *ref) {
  std::vector<Expr *> stagesCloned;
  for (auto *stage : stages)
    stagesCloned.push_back(stage->clone(ref));
  SEQ_RETURN_CLONE(new PipeExpr(stagesCloned, parallel));
}

types::RecordType *PipeExpr::getInterAlignYieldType() {
  return types::RecordType::get({types::Seq, types::Seq, types::Int},
                                {"query", "reference", "score"},
                                "InterAlignYield");
}

types::RecordType *PipeExpr::getInterAlignParamsType() {
  auto *i8 = types::IntNType::get(8, true);
  return types::RecordType::get(
      {i8, i8, i8, i8, i8, i8, i8, i8},
      {"a", "b", "ambig", "gapo", "gape", "bandwidth", "zdrop", "end_bonus"},
      "InterAlignParams");
}

types::RecordType *PipeExpr::getInterAlignSeqPairType() {
  auto *i32 = types::IntNType::get(32, true);
  return types::RecordType::get(
      {i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32},
      {"idr", "idq", "id", "len1", "len2", "h0", "seqid", "regid", "score",
       "tle", "gtle", "qle", "gscore", "max_off"},
      "SeqPair");
}
