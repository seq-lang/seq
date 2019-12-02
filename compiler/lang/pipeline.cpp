#include "seq/seq.h"
#include <queue>

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

struct DrainState {
  Value *states;             // coroutine states buffer
  Value *filled;             // how many coroutines have been added (alloca'd)
  types::GenType *type;      // type of prefetch generator
  std::queue<Expr *> stages; // remaining pipeline stages
  std::queue<bool> parallel;

  DrainState()
      : states(nullptr), filled(nullptr), type(nullptr), stages(), parallel() {}
};

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

    if (!(genType && genType->fromPrefetch())) {
      val = call.codegen(base, block);
    } else if (drain->states) {
      throw exc::SeqException(
          "cannot have multiple prefetch functions in single pipeline");
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
    Value *states =
        makeAlloca(builder.getInt8PtrTy(), preamble, PipeExpr::SCHED_WIDTH);
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
    Value *M = ConstantInt::get(seqIntLLVM(context), PipeExpr::SCHED_WIDTH);
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
    nextVal =
        builder.CreateAnd(nextVal, ConstantInt::get(seqIntLLVM(context),
                                                    PipeExpr::SCHED_WIDTH - 1));
    builder.CreateStore(nextVal, next);
    builder.CreateBr(full0);

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
    Module *module = block->getModule();
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
  Function *func = block->getParent();

  std::queue<Expr *> queue;
  std::queue<bool> parallelQueue;

  for (auto *stage : stages)
    queue.push(stage);

  for (bool parallelize : parallel)
    parallelQueue.push(parallelize);

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
    if (genType && (genType->fromPrefetch() || stage != stages.back()))
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
