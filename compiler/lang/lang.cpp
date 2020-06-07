#include "lang/seq.h"

using namespace seq;
using namespace llvm;

Print::Print(Expr *expr, bool nopOnVoid)
    : Stmt("print"), expr(expr), nopOnVoid(nopOnVoid) {}

void Print::resolveTypes() { expr->resolveTypes(); }

void Print::codegen0(BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  Module *module = block->getModule();
  types::Type *type = expr->getType();
  Value *val = expr->codegen(getBase(), block);

  if (nopOnVoid && type->is(types::Void))
    return;

  assert(type->is(types::Str));
  auto *printFunc = cast<Function>(module->getOrInsertFunction(
      "seq_print", Type::getVoidTy(context), types::Str->getLLVMType(context)));
  printFunc->setDoesNotThrow();
  IRBuilder<> builder(block);
  builder.CreateCall(printFunc, val);
}

Print *Print::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (Print *)ref->getClone(this);

  auto *x = new Print(expr->clone(ref), nopOnVoid);
  ref->addClone(this, x);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

ExprStmt::ExprStmt(Expr *expr) : Stmt("expr"), expr(expr) {}

void ExprStmt::resolveTypes() { expr->resolveTypes(); }

void ExprStmt::codegen0(BasicBlock *&block) {
  if (dynamic_cast<types::PartialFuncType *>(expr->getType())) {
    SrcInfo src = getSrcInfo();
    compilationWarning(
        "function call is partial; are there arguments missing on this call?",
        src.file, src.line, src.col);
  }
  expr->codegen(getBase(), block);
}

ExprStmt *ExprStmt::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (ExprStmt *)ref->getClone(this);

  auto *x = new ExprStmt(expr->clone(ref));
  ref->addClone(this, x);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

VarStmt::VarStmt(Expr *init, types::Type *type)
    : Stmt("var"), init(init), type(type), var(new Var()) {
  assert(init || type);
}

Var *VarStmt::getVar() { return var; }

void VarStmt::resolveTypes() {
  if (init)
    init->resolveTypes();
  var->setType(type ? type : init->getType());
}

void VarStmt::codegen0(BasicBlock *&block) {
  if (type && init) {
    types::Type *got = init->getType();
    if (!types::is(type, got))
      throw exc::SeqException("expected var type '" + type->getName() +
                              "', but got '" + got->getName() + "'");
  }

  Value *val =
      init ? init->codegen(getBase(), block) : type->defaultValue(block);
  var->store(getBase(), val, block);
}

VarStmt *VarStmt::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (VarStmt *)ref->getClone(this);

  auto *x = new VarStmt(init ? init->clone(ref) : nullptr);
  ref->addClone(this, x);
  x->var = var->clone(ref);
  x->type = type ? type->clone(ref) : nullptr;
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

FuncStmt::FuncStmt(Func *func) : Stmt("func"), func(func) {}

void FuncStmt::resolveTypes() { func->resolveTypes(); }

void FuncStmt::codegen0(BasicBlock *&block) {
  // make sure we codegen exported functions
  if (func->hasAttribute("export")) {
    FuncExpr f(func);
    ExprStmt e(&f);
    e.setBase(getBase());
    e.codegen(block);
  }
}

FuncStmt *FuncStmt::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (FuncStmt *)ref->getClone(this);

  auto *x = new FuncStmt(nullptr);
  ref->addClone(this, x);
  x->func = func->clone(ref);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

Assign::Assign(Var *var, Expr *value, bool atomic)
    : Stmt("(=)"), var(var), value(value), atomic(atomic) {}

void Assign::setAtomic() { atomic = true; }

void Assign::resolveTypes() { value->resolveTypes(); }

void Assign::codegen0(BasicBlock *&block) {
  value->ensure(var->getType());
  Value *val = value->codegen(getBase(), block);
  var->store(getBase(), val, block, atomic);
}

Assign *Assign::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (Assign *)ref->getClone(this);

  auto *x = new Assign(var->clone(ref), value->clone(ref), atomic);
  ref->addClone(this, x);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

AssignIndex::AssignIndex(Expr *array, Expr *idx, Expr *value)
    : Stmt("([]=)"), array(array), idx(idx), value(value) {}

void AssignIndex::resolveTypes() {
  array->resolveTypes();
  idx->resolveTypes();
  value->resolveTypes();
}

void AssignIndex::codegen0(BasicBlock *&block) {
  Value *val = value->codegen(getBase(), block);
  Value *arr = array->codegen(getBase(), block);
  Value *idx = this->idx->codegen(getBase(), block);
  array->getType()->callMagic("__setitem__",
                              {this->idx->getType(), value->getType()}, arr,
                              {idx, val}, block, getTryCatch());
}

AssignIndex *AssignIndex::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (AssignIndex *)ref->getClone(this);

  auto *x =
      new AssignIndex(array->clone(ref), idx->clone(ref), value->clone(ref));
  ref->addClone(this, x);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

Del::Del(Var *var) : Stmt("del"), var(var) {}

void Del::codegen0(BasicBlock *&block) {
  Value *empty = var->getType()->defaultValue(block);
  var->store(getBase(), empty, block);
}

Del *Del::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (Del *)ref->getClone(this);

  auto *x = new Del(var->clone(ref));
  ref->addClone(this, x);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

DelIndex::DelIndex(Expr *array, Expr *idx)
    : Stmt("del []"), array(array), idx(idx) {}

void DelIndex::resolveTypes() {
  array->resolveTypes();
  idx->resolveTypes();
}

void DelIndex::codegen0(BasicBlock *&block) {
  Value *arr = array->codegen(getBase(), block);
  Value *idx = this->idx->codegen(getBase(), block);
  array->getType()->callMagic("__delitem__", {this->idx->getType()}, arr, {idx},
                              block, getTryCatch());
}

DelIndex *DelIndex::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (DelIndex *)ref->getClone(this);

  auto *x = new DelIndex(array->clone(ref), idx->clone(ref));
  ref->addClone(this, x);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

AssignMember::AssignMember(Expr *expr, std::string memb, Expr *value)
    : Stmt("(.=)"), expr(expr), memb(std::move(memb)), value(value) {}

AssignMember::AssignMember(Expr *expr, seq_int_t idx, Expr *value)
    : AssignMember(expr, std::to_string(idx), value) {}

void AssignMember::resolveTypes() {
  expr->resolveTypes();
  value->resolveTypes();

  // auto-deduce class member types:
  try {
    if (types::RefType *ref = expr->getType()->asRef())
      ref->addMember(memb, value);
  } catch (exc::SeqException &) {
    // if we fail, no big deal, just carry on...
  }
}

void AssignMember::codegen0(BasicBlock *&block) {
  types::Type *type = expr->getType();
  if (!type->asRef())
    throw exc::SeqException("cannot assign member of non-reference type '" +
                            type->getName() + "'");
  value->ensure(expr->getType()->membType(memb));
  Value *x = expr->codegen(getBase(), block);
  Value *v = value->codegen(getBase(), block);
  expr->getType()->setMemb(x, memb, v, block);
}

AssignMember *AssignMember::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (AssignMember *)ref->getClone(this);

  auto *x = new AssignMember(expr->clone(ref), memb, value->clone(ref));
  ref->addClone(this, x);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

If::If() : Stmt("if"), conds(), branches(), elseAdded(false) {}

void If::resolveTypes() {
  for (auto *cond : conds)
    cond->resolveTypes();

  for (auto *branch : branches)
    branch->resolveTypes();
}

void If::codegen0(BasicBlock *&block) {
  assert(!conds.empty() && conds.size() == branches.size());

  LLVMContext &context = block->getContext();
  Function *func = block->getParent();
  IRBuilder<> builder(block);
  std::vector<BranchInst *> binsts;
  TryCatch *tc = getTryCatch();

  for (unsigned i = 0; i < conds.size(); i++) {
    Value *cond = conds[i]->codegen(getBase(), block);
    cond = conds[i]->getType()->boolValue(cond, block, tc);
    Block *branch = branches[i];

    builder.SetInsertPoint(block); // recall: expr codegen can change the block
    cond = builder.CreateTrunc(cond, IntegerType::getInt1Ty(context));

    BasicBlock *b1 = BasicBlock::Create(context, "", func);
    BranchInst *binst1 =
        builder.CreateCondBr(cond, b1, b1); // we set false-branch below

    block = b1;
    branch->codegen(block);
    builder.SetInsertPoint(block);
    BranchInst *binst2 = builder.CreateBr(b1); // we reset this below
    binsts.push_back(binst2);

    BasicBlock *b2 = BasicBlock::Create(context, "", func);
    binst1->setSuccessor(1, b2);
    block = b2;
  }

  BasicBlock *after = BasicBlock::Create(context, "", func);
  builder.SetInsertPoint(block);
  builder.CreateBr(after);

  for (auto *binst : binsts)
    binst->setSuccessor(0, after);

  block = after;
}

Block *If::addCond(Expr *cond) {
  assert(!elseAdded);
  auto *branch = new Block(this);
  conds.push_back(cond);
  branches.push_back(branch);
  return branch;
}

Block *If::addElse() {
  assert(!elseAdded);
  Block *branch = addCond(new BoolExpr(true));
  elseAdded = true;
  return branch;
}

Block *If::getBlock(unsigned int idx) {
  assert(idx < branches.size());
  return branches[idx];
}

If *If::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (If *)ref->getClone(this);

  auto *x = new If();
  ref->addClone(this, x);

  std::vector<Expr *> condsCloned;
  std::vector<Block *> branchesCloned;

  for (auto *cond : conds)
    condsCloned.push_back(cond->clone(ref));

  for (auto *branch : branches)
    branchesCloned.push_back(branch->clone(ref));

  x->conds = condsCloned;
  x->branches = branchesCloned;
  x->elseAdded = elseAdded;

  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

TryCatch::TryCatch()
    : Stmt("try"), scope(new Block(this)), catchTypes(), catchBlocks(),
      catchVars(), finally(new Block(this)), exceptionBlock(nullptr),
      exceptionRouteBlock(nullptr), finallyStart(nullptr), handlers(),
      excFlag(nullptr), catchStore(nullptr), delegateDepth(nullptr),
      retStore(nullptr) {}

ConstantInt *TryCatch::state(LLVMContext &context, State s) {
  return ConstantInt::get(Type::getInt8Ty(context), s);
}

Block *TryCatch::getBlock() { return scope; }

Var *TryCatch::getVar(unsigned idx) {
  assert(idx < catchVars.size());
  return catchVars[idx];
}

Block *TryCatch::addCatch(types::Type *type) {
  auto *block = new Block(this);
  catchTypes.push_back(type);
  catchBlocks.push_back(block);
  catchVars.push_back(type ? new Var(type) : nullptr);
  return block;
}

Block *TryCatch::getFinally() { return finally; }

BasicBlock *TryCatch::getExceptionBlock() {
  assert(exceptionBlock);
  return exceptionBlock;
}

void TryCatch::codegenReturn(Expr *expr, BasicBlock *&block) {
  assert(excFlag && finallyStart);
  auto *func = dynamic_cast<Func *>(getBase());
  if (!func)
    throw exc::SeqException("misplaced return");

  LLVMContext &context = block->getContext();
  types::Type *type = expr ? expr->getType() : types::Void;
  Value *val = expr ? expr->codegen(func, block) : nullptr;

  // dryrun=true -- just do type checking
  func->codegenReturn(val, type, block, true);
  IRBuilder<> builder(block);

  if (retStore) {
    assert(val);
    builder.CreateStore(val, retStore);
  }

  builder.CreateStore(state(context, RETURN), excFlag);
  builder.CreateBr(finallyStart);
  block = BasicBlock::Create(context, "", block->getParent());
}

void TryCatch::codegenBreak(BasicBlock *&block) {
  findEnclosingLoop(); // error check
  LLVMContext &context = block->getContext();
  IRBuilder<> builder(block);
  builder.CreateStore(state(context, BREAK), excFlag);
  builder.CreateBr(finallyStart);
  block = BasicBlock::Create(context, "", block->getParent());
}

void TryCatch::codegenContinue(BasicBlock *&block) {
  findEnclosingLoop(); // error check
  LLVMContext &context = block->getContext();
  IRBuilder<> builder(block);
  builder.CreateStore(state(context, CONTINUE), excFlag);
  builder.CreateBr(finallyStart);
  block = BasicBlock::Create(context, "", block->getParent());
}

static StructType *getTypeInfoType(LLVMContext &context) {
  return StructType::get(IntegerType::getInt32Ty(context));
}

StructType *TryCatch::getPadType(LLVMContext &context) {
  return StructType::get(IntegerType::getInt8PtrTy(context),
                         IntegerType::getInt32Ty(context));
}

StructType *TryCatch::getExcType(LLVMContext &context) {
  return StructType::get(getTypeInfoType(context),
                         IntegerType::getInt8PtrTy(context));
}

GlobalVariable *TryCatch::getTypeIdxVar(Module *module,
                                        const std::string &name) {
  LLVMContext &context = module->getContext();
  auto *typeInfoType = getTypeInfoType(context);
  const std::string typeVarName =
      "seq.typeidx." + (name.empty() ? "<all>" : name);
  GlobalVariable *tidx = module->getGlobalVariable(typeVarName);
  int idx = types::Type::getID(name);
  if (!tidx)
    tidx = new GlobalVariable(
        *module, typeInfoType, true, GlobalValue::PrivateLinkage,
        ConstantStruct::get(typeInfoType,
                            ConstantInt::get(IntegerType::getInt32Ty(context),
                                             (uint64_t)idx, false)),
        typeVarName);
  return tidx;
}

GlobalVariable *TryCatch::getTypeIdxVar(Module *module,
                                        types::Type *catchType) {
  return getTypeIdxVar(module, catchType ? catchType->getName() : "");
}

void TryCatch::resolveTypes() {
  scope->resolveTypes();
  for (auto *block : catchBlocks)
    block->resolveTypes();
  finally->resolveTypes();
}

static bool anyMatch(types::Type *type, std::vector<types::Type *> types) {
  if (type) {
    for (auto *t : types) {
      if (t && seq::types::is(type, t))
        return true;
    }
  } else {
    for (auto *t : types) {
      if (!t)
        return true;
    }
  }
  return false;
}

static bool inLoop(Stmt *stmt) {
  while (stmt) {
    if (stmt->isLoop())
      return true;
    stmt = stmt->getPrev();
  }
  return false;
}

static TryCatch *getInnermostTryCatchBeforeLoop(Stmt *x) {
  Stmt *stmt = x->getPrev();
  Stmt *last = x;

  while (stmt) {
    if (auto *s = dynamic_cast<TryCatch *>(stmt)) {
      // make sure we're not enclosed by except or finally
      if (last->getParent() == s->getBlock())
        return s;
    }

    if (stmt->isLoop())
      return nullptr;

    last = stmt;
    stmt = stmt->getPrev();
  }

  return nullptr;
}

void TryCatch::codegen0(BasicBlock *&block) {
  assert(catchTypes.size() == catchBlocks.size());

  for (unsigned i = 0; i < catchTypes.size(); i++) {
    for (unsigned j = 0; j < i; j++) {
      if (catchTypes[i] && catchTypes[j] &&
          types::is(catchTypes[i], catchTypes[j]))
        throw exc::SeqException("duplicate except type '" +
                                catchTypes[i]->getName() + "'");
    }
  }

  std::vector<TryCatch *> parents;
  TryCatch *parent = getTryCatch();
  while (parent) {
    parents.push_back(parent);
    parent = parent->getTryCatch();
  }
  TryCatch *root = parents.empty() ? this : parents.back();

  LLVMContext &context = block->getContext();
  Module *module = block->getModule();
  Function *func = block->getParent();
  BaseFunc *base = getBase();
  BasicBlock *preambleBlock = base->getPreamble();
  types::Type *retType = base->getFuncType()->getBaseType(0);

  if (types::GenType *gen = retType->asGen()) {
    if (gen->fromPrefetch() || gen->fromInterAlign())
      retType = gen->getBaseType(0);
  }

  // entry block:
  BasicBlock *entryBlock = BasicBlock::Create(context, "entry", func);
  BasicBlock *entryBlock0 = entryBlock;

  // unwind block for invoke
  exceptionBlock = BasicBlock::Create(context, "exception", func);

  // block which routes exception to correct catch handler block
  exceptionRouteBlock = BasicBlock::Create(context, "exception_route", func);

  // foreign exception handler
  BasicBlock *externalExceptionBlock =
      BasicBlock::Create(context, "external_exception", func);

  // block which calls _Unwind_Resume
  BasicBlock *unwindResumeBlock =
      BasicBlock::Create(context, "unwind_resume", func);

  // clean up block which delete exception if needed
  BasicBlock *endBlock = BasicBlock::Create(context, "end", func);

  StructType *padType = getPadType(context);
  StructType *unwindType =
      StructType::get(IntegerType::getInt64Ty(context)); // header only
  StructType *excType = getExcType(context);

  // exception storage/state:
  ConstantInt *excStateNotThrown = state(context, NOT_THROWN);
  ConstantInt *excStateThrown = state(context, THROWN);
  ConstantInt *excStateCaught = state(context, CAUGHT);
  ConstantInt *excStateReturn = state(context, RETURN);
  ConstantInt *excStateBreak = state(context, BREAK);
  ConstantInt *excStateContinue = state(context, CONTINUE);

  IRBuilder<> builder(entryBlock);
  if (this == root) {
    excFlag = makeAlloca(excStateNotThrown, preambleBlock);
    catchStore = makeAlloca(ConstantAggregateZero::get(padType), preambleBlock);
    delegateDepth = makeAlloca(zeroLLVM(context), preambleBlock);
    if (!retType->is(types::Void) && !base->isGen())
      retStore = makeAlloca(retType->getLLVMType(context), preambleBlock);
  } else {
    excFlag = root->excFlag;
    catchStore = root->catchStore;
    delegateDepth = root->delegateDepth;
    retStore = root->retStore;
  }

  // initialize state:
  builder.CreateStore(excStateNotThrown, excFlag);
  builder.CreateStore(ConstantAggregateZero::get(padType), catchStore);
  builder.CreateStore(zeroLLVM(context), delegateDepth);

  // finally:
  // logic here is complex since we potentially need to delegate to parent
  // finally based on `depth`
  BasicBlock *finallyBlock = BasicBlock::Create(context, "finally", func);
  finallyStart = finallyBlock;
  finally->codegen(finallyBlock);

  builder.SetInsertPoint(finallyBlock);
  Value *excFlagRead = builder.CreateLoad(excFlag);

  if (this != root) {
    Value *depthRead = builder.CreateLoad(delegateDepth);
    Value *delegate = builder.CreateICmpSGT(depthRead, zeroLLVM(context));
    BasicBlock *finallyNormal =
        BasicBlock::Create(context, "finally_normal", func);
    BasicBlock *finallyDelegate =
        BasicBlock::Create(context, "finally_delegate", func);
    builder.CreateCondBr(delegate, finallyDelegate, finallyNormal);

    builder.SetInsertPoint(finallyDelegate);
    Value *depthNew = builder.CreateSub(depthRead, oneLLVM(context));
    Value *delegateNew = builder.CreateICmpSGT(depthNew, zeroLLVM(context));
    builder.CreateStore(depthNew, delegateDepth);
    builder.CreateCondBr(delegateNew, parents.front()->finallyStart,
                         parents.front()->exceptionRouteBlock);

    finallyBlock = finallyNormal;
  }

  builder.SetInsertPoint(finallyBlock);
  const bool supportBreakAndContinue = inLoop(this);
  SwitchInst *theSwitch = builder.CreateSwitch(excFlagRead, endBlock,
                                               supportBreakAndContinue ? 5 : 3);
  theSwitch->addCase(excStateCaught, endBlock);
  theSwitch->addCase(excStateThrown, unwindResumeBlock);

  if (this == root) {
    BasicBlock *finallyReturn =
        BasicBlock::Create(context, "finally_return", func);
    theSwitch->addCase(excStateReturn, finallyReturn);
    auto *f = dynamic_cast<Func *>(base);

    if (f) { // f could also be a SeqModule, which can't have returns
      Value *ret = nullptr;
      builder.SetInsertPoint(finallyReturn);
      if (retStore)
        ret = builder.CreateLoad(retStore);
      f->codegenReturn(ret, retType, finallyReturn);
    }

    // mark new block returned by `codegenReturn` as unreachable:
    builder.SetInsertPoint(finallyReturn);
    builder.CreateUnreachable();
  } else {
    theSwitch->addCase(excStateReturn, parents.front()->finallyStart);
  }

  if (supportBreakAndContinue) {
    const bool outer = (getInnermostTryCatchBeforeLoop(this) == nullptr);

    if (outer) {
      BasicBlock *finallyBreak =
          BasicBlock::Create(context, "finally_break", func);
      BasicBlock *finallyContinue =
          BasicBlock::Create(context, "finally_continue", func);
      theSwitch->addCase(excStateBreak, finallyBreak);
      theSwitch->addCase(excStateContinue, finallyContinue);

      builder.SetInsertPoint(finallyBreak);
      builder.CreateStore(excStateNotThrown, excFlag);
      BranchInst *instBreak =
          builder.CreateBr(block); // destination will be fixed by `setBreaks`
      addBreakToEnclosingLoop(instBreak);

      builder.SetInsertPoint(finallyContinue);
      builder.CreateStore(excStateNotThrown, excFlag);
      BranchInst *instContinue = builder.CreateBr(
          block); // destination will be fixed by `setContinues`
      addContinueToEnclosingLoop(instContinue);
    } else {
      assert(this != root);
      theSwitch->addCase(excStateBreak, parents.front()->finallyStart);
      theSwitch->addCase(excStateContinue, parents.front()->finallyStart);
    }
  }

  BasicBlock *catchAll = nullptr;
  unsigned catchAllDepth = 0;
  for (unsigned i = 0; i < catchTypes.size(); i++) {
    BasicBlock *catchBlock =
        BasicBlock::Create(context, "catch" + std::to_string(i + 1), func);
    handlers.push_back(catchBlock);

    if (!catchTypes[i]) {
      if (catchAll)
        throw exc::SeqException("can only have at most one except-all clause");
      catchAll = catchBlock;
    }
  }

  // codegen try:
  scope->codegen(entryBlock);

  // make sure we always get to finally block:
  builder.SetInsertPoint(entryBlock);
  builder.CreateBr(finallyStart);

  // rethrow if uncaught:
  builder.SetInsertPoint(unwindResumeBlock);
  builder.CreateResume(builder.CreateLoad(catchStore));

  // make sure we delegate to parent try-catch if necessary:
  std::vector<types::Type *> catchTypesFull(catchTypes);
  std::vector<llvm::BasicBlock *> handlersFull(handlers);
  std::vector<unsigned> depths(catchTypes.size(), 0);
  unsigned depth = 1;

  for (auto *tc : parents) {
    if (catchAll) // can't ever delegate past catch-all
      break;

    assert(tc->catchTypes.size() == tc->handlers.size());
    for (unsigned i = 0; i < tc->catchTypes.size(); i++) {
      if (!anyMatch(tc->catchTypes[i], catchTypesFull)) {
        catchTypesFull.push_back(tc->catchTypes[i]);
        depths.push_back(depth);

        if (!tc->catchTypes[i] && !catchAll) {
          // catch-all is in parent; set finally depth
          catchAll = BasicBlock::Create(context, "fdepth_catchall", func);
          builder.SetInsertPoint(catchAll);
          builder.CreateStore(ConstantInt::get(seqIntLLVM(context), depth),
                              delegateDepth);
          builder.CreateBr(tc->handlers[i]);
          handlersFull.push_back(catchAll);
          catchAllDepth = depth;
        } else {
          handlersFull.push_back(tc->handlers[i]);
        }
      }
    }
    depth += 1;
  }

  // exception handling:
  builder.SetInsertPoint(exceptionBlock);
  LandingPadInst *caughtResult =
      builder.CreateLandingPad(padType, (unsigned)catchTypes.size());
  caughtResult->setCleanup(true);
  std::vector<Value *> typeIndices;

  for (auto *catchType : catchTypesFull) {
    if (catchType && !catchType->asRef())
      throw exc::SeqException("cannot catch non-reference type '" +
                              catchType->getName() + "'");

    const std::string typeVarName =
        "seq.typeidx." + (catchType ? catchType->getName() : "<all>");
    GlobalVariable *tidx = getTypeIdxVar(module, catchType);
    typeIndices.push_back(tidx);
    caughtResult->addClause(tidx);
  }

  Value *unwindException = builder.CreateExtractValue(caughtResult, 0);
  builder.CreateStore(caughtResult, catchStore);
  builder.CreateStore(excStateThrown, excFlag);
  Value *depthMax = ConstantInt::get(seqIntLLVM(context), parents.size());
  builder.CreateStore(depthMax, delegateDepth);

  Value *unwindExceptionClass = builder.CreateLoad(builder.CreateStructGEP(
      unwindType,
      builder.CreatePointerCast(unwindException, unwindType->getPointerTo()),
      0));

  // check for foreign exceptions:
  builder.CreateCondBr(
      builder.CreateICmpEQ(
          unwindExceptionClass,
          ConstantInt::get(IntegerType::getInt64Ty(context), seq_exc_class())),
      exceptionRouteBlock, externalExceptionBlock);

  // external exception (fail fast):
  builder.SetInsertPoint(externalExceptionBlock);
  builder.CreateUnreachable();

  // reroute Seq exceptions:
  builder.SetInsertPoint(exceptionRouteBlock);
  unwindException =
      builder.CreateExtractValue(builder.CreateLoad(catchStore), 0);
  Value *excVal = builder.CreatePointerCast(
      builder.CreateConstGEP1_64(unwindException, (uint64_t)seq_exc_offset()),
      excType->getPointerTo());

  Value *loadedExc = builder.CreateLoad(excVal);
  Value *objType = builder.CreateExtractValue(loadedExc, 0);
  objType = builder.CreateExtractValue(objType, 0);
  Value *objPtr = builder.CreateExtractValue(loadedExc, 1);

  // set depth when catch-all entered:
  BasicBlock *defaultRouteBlock = BasicBlock::Create(context, "fdepth", func);
  builder.SetInsertPoint(defaultRouteBlock);
  if (catchAll)
    builder.CreateStore(ConstantInt::get(seqIntLLVM(context), catchAllDepth),
                        delegateDepth);
  builder.CreateBr(catchAll ? (catchAllDepth > 0 ? finallyStart : catchAll)
                            : finallyStart);

  builder.SetInsertPoint(exceptionRouteBlock);
  SwitchInst *switchToCatchBlock = builder.CreateSwitch(
      objType, defaultRouteBlock, (unsigned)handlersFull.size());
  for (unsigned i = 0; i < handlersFull.size(); i++) {
    BasicBlock *catchBlock = handlersFull[i];
    BasicBlock *catchBlock0 = catchBlock;

    // set finally depth:
    BasicBlock *depthSet = BasicBlock::Create(context, "fdepth", func);
    builder.SetInsertPoint(depthSet);
    builder.CreateStore(ConstantInt::get(seqIntLLVM(context), depths[i]),
                        delegateDepth);
    builder.CreateBr((i < handlers.size()) ? catchBlock : finallyStart);
    catchBlock = depthSet;

    if (catchTypesFull[i]) {
      switchToCatchBlock->addCase(
          ConstantInt::get(IntegerType::getInt32Ty(context),
                           (uint64_t)catchTypesFull[i]->getID(), true),
          catchBlock);
    }

    // codegen catch body if this block is ours (vs. a parent's):
    if (i < catchTypes.size()) {
      builder.SetInsertPoint(catchBlock0);
      Var *var = catchVars[i];

      if (var) {
        Value *obj =
            builder.CreateBitCast(objPtr, catchTypes[i]->getLLVMType(context));
        var->store(base, obj, catchBlock0);
      }

      builder.SetInsertPoint(catchBlock0);
      builder.CreateStore(excStateCaught, excFlag);
      catchBlocks[i]->codegen(catchBlock0);
      builder.SetInsertPoint(
          catchBlock0); // could be different after previous codegen
      builder.CreateBr(finallyStart);
    }
  }

  // link in our new blocks, and update the caller's block:
  builder.SetInsertPoint(block);
  builder.CreateBr(entryBlock0);
  block = endBlock;
}

TryCatch *TryCatch::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (TryCatch *)ref->getClone(this);

  auto *x = new TryCatch();
  ref->addClone(this, x);
  x->scope = scope->clone(ref);

  std::vector<types::Type *> catchTypesCloned;
  std::vector<Block *> catchBlocksCloned;
  std::vector<Var *> catchVarsCloned;

  for (auto *type : catchTypes)
    catchTypesCloned.push_back(type ? type->clone(ref) : nullptr);

  for (auto *block : catchBlocks)
    catchBlocksCloned.push_back(block->clone(ref));

  for (auto *var : catchVars)
    catchVarsCloned.push_back(var ? var->clone(ref) : nullptr);

  x->catchTypes = catchTypesCloned;
  x->catchBlocks = catchBlocksCloned;
  x->catchVars = catchVarsCloned;
  x->finally = finally->clone(ref);

  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

Throw::Throw(Expr *expr) : Stmt("throw"), expr(expr) {}

void Throw::resolveTypes() { expr->resolveTypes(); }

void Throw::codegen0(BasicBlock *&block) {
  types::Type *type = expr->getType();
  types::RefType *refType = type->asRef();

  if (!refType)
    throw exc::SeqException("cannot throw non-reference type '" +
                            type->getName() + "'");

  if (refType->numGenerics() > 0)
    throw exc::SeqException("cannot throw generic type '" + type->getName() +
                            "'");

  static types::RecordType *excHeader = types::RecordType::get(
      {types::Str, types::Str, types::Str, types::Str, types::Int, types::Int},
      {}, "ExcHeader");
  if (refType->numBaseTypes() < 1 || !refType->getBaseType(0)->is(excHeader))
    throw exc::SeqException(
        "first member of thrown exception must be of type 'ExcHeader'");

  LLVMContext &context = block->getContext();
  Module *module = block->getModule();
  Function *excAllocFunc = makeExcAllocFunc(module);
  Function *throwFunc = makeThrowFunc(module);

  Value *obj = expr->codegen(getBase(), block);
  IRBuilder<> builder(block);

  // add meta-info like func/file/line/col
  Type *hdrType = excHeader->getLLVMType(context);
  Value *hdrPtr = builder.CreateBitCast(obj, hdrType->getPointerTo());
  Value *hdr = builder.CreateLoad(hdrPtr);

  std::string funcNameStr = "<main>";
  if (auto *func = dynamic_cast<Func *>(getBase())) {
    funcNameStr = func->genericName();
  }
  std::string fileNameStr = getSrcInfo().file;
  seq_int_t fileLine = getSrcInfo().line;
  seq_int_t fileCol = getSrcInfo().col;

  StrExpr funcNameExpr(funcNameStr);
  Value *funcNameVal = funcNameExpr.codegen(getBase(), block);

  StrExpr fileNameExpr(fileNameStr);
  Value *fileNameVal = fileNameExpr.codegen(getBase(), block);

  Value *fileLineVal = ConstantInt::get(seqIntLLVM(context), fileLine, true);
  Value *fileColVal = ConstantInt::get(seqIntLLVM(context), fileCol, true);

  hdr = excHeader->setMemb(hdr, "3", funcNameVal, block);
  hdr = excHeader->setMemb(hdr, "4", fileNameVal, block);
  hdr = excHeader->setMemb(hdr, "5", fileLineVal, block);
  hdr = excHeader->setMemb(hdr, "6", fileColVal, block);
  builder.SetInsertPoint(block);
  builder.CreateStore(hdr, hdrPtr);

  Value *exc = builder.CreateCall(
      excAllocFunc, {ConstantInt::get(IntegerType::getInt32Ty(context),
                                      (uint64_t)type->getID(), true),
                     obj});

  if (getTryCatch()) {
    Function *parent = block->getParent();
    BasicBlock *unwind = getTryCatch()->getExceptionBlock();
    BasicBlock *normal = BasicBlock::Create(context, "normal", parent);
    builder.CreateInvoke(throwFunc, normal, unwind, exc);
    block = normal;
  } else {
    builder.CreateCall(throwFunc, exc);
  }
}

Throw *Throw::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (Throw *)ref->getClone(this);

  auto *x = new Throw(expr->clone(ref));
  ref->addClone(this, x);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

Match::Match() : Stmt("match"), value(nullptr), patterns(), branches() {}

void Match::resolveTypes() {
  assert(value);
  value->resolveTypes();

  for (auto *pattern : patterns)
    pattern->resolveTypes(value->getType());

  for (auto *branch : branches)
    branch->resolveTypes();
}

void Match::codegen0(BasicBlock *&block) {
  assert(!patterns.empty() && patterns.size() == branches.size() && value);

  LLVMContext &context = block->getContext();
  Function *func = block->getParent();

  IRBuilder<> builder(block);
  types::Type *valType = value->getType();

  bool seenCatchAll = false;
  for (auto *pattern : patterns) {
    pattern->resolveTypes(valType);
    if (pattern->isCatchAll())
      seenCatchAll = true;
  }

  if (!seenCatchAll)
    throw exc::SeqException("match statement missing catch-all pattern");

  Value *val = value->codegen(getBase(), block);

  std::vector<BranchInst *> binsts;

  for (unsigned i = 0; i < patterns.size(); i++) {
    Value *cond = patterns[i]->codegen(getBase(), valType, val, block);
    Block *branch = branches[i];

    builder.SetInsertPoint(block); // recall: expr codegen can change the block

    BasicBlock *b1 = BasicBlock::Create(context, "", func);
    BranchInst *binst1 =
        builder.CreateCondBr(cond, b1, b1); // we set false-branch below

    block = b1;
    branch->codegen(block);
    builder.SetInsertPoint(block);
    BranchInst *binst2 = builder.CreateBr(b1); // we reset this below
    binsts.push_back(binst2);

    BasicBlock *b2 = BasicBlock::Create(context, "", func);
    binst1->setSuccessor(1, b2);
    block = b2;
  }

  BasicBlock *after = BasicBlock::Create(context, "", func);
  builder.SetInsertPoint(block);
  builder.CreateBr(after);

  for (auto *binst : binsts)
    binst->setSuccessor(0, after);

  block = after;
}

void Match::setValue(Expr *value) {
  assert(!this->value);
  this->value = value;
}

Block *Match::addCase(Pattern *pattern) {
  assert(value);
  auto *branch = new Block(this);
  patterns.push_back(pattern);
  branches.push_back(branch);
  return branch;
}

Match *Match::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (Match *)ref->getClone(this);

  auto *x = new Match();
  ref->addClone(this, x);

  std::vector<Pattern *> patternsCloned;
  std::vector<Block *> branchesCloned;

  for (auto *pattern : patterns)
    patternsCloned.push_back(pattern->clone(ref));

  for (auto *branch : branches)
    branchesCloned.push_back(branch->clone(ref));

  if (value)
    x->value = value->clone(ref);
  x->patterns = patternsCloned;
  x->branches = branchesCloned;

  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

While::While(Expr *cond) : Stmt("while"), cond(cond), scope(new Block(this)) {
  loop = true;
}

Block *While::getBlock() { return scope; }

void While::resolveTypes() {
  cond->resolveTypes();
  scope->resolveTypes();
}

void While::codegen0(BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  BasicBlock *entry = block;
  Function *func = entry->getParent();
  IRBuilder<> builder(entry);

  BasicBlock *loop0 = BasicBlock::Create(context, "while", func);
  BasicBlock *loop = loop0;
  builder.CreateBr(loop);

  Value *cond =
      this->cond->codegen(getBase(), loop); // recall: this can change `loop`
  cond = this->cond->getType()->boolValue(cond, loop, getTryCatch());
  builder.SetInsertPoint(loop);
  cond = builder.CreateTrunc(cond, IntegerType::getInt1Ty(context));

  BasicBlock *body = BasicBlock::Create(context, "body", func);
  BranchInst *branch =
      builder.CreateCondBr(cond, body, body); // we set false-branch below

  block = body;

  scope->codegen(block);

  builder.SetInsertPoint(block);
  builder.CreateBr(loop0);

  BasicBlock *exit = BasicBlock::Create(context, "exit", func);
  branch->setSuccessor(1, exit);
  block = exit;

  setBreaks(exit);
  setContinues(loop0);
}

While *While::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (While *)ref->getClone(this);

  auto *x = new While(cond->clone(ref));
  ref->addClone(this, x);
  delete x->scope;
  x->scope = scope->clone(ref);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

For::For(Expr *gen)
    : Stmt("for"), gen(gen), scope(new Block(this)), var(new Var()) {
  loop = true;
}

Expr *For::getGen() { return gen; }

Block *For::getBlock() { return scope; }

Var *For::getVar() { return var; }

void For::setGen(Expr *gen) { this->gen = gen; }

void For::resolveTypes() {
  gen->resolveTypes();

  try {
    types::GenType *genType = gen->getType()->magicOut("__iter__", {})->asGen();

    if (!genType)
      throw exc::SeqException("__iter__ does not return a generator");

    var->setType(genType->getBaseType(0));
  } catch (exc::SeqException &e) {
    e.setSrcInfo(getSrcInfo());
    throw e;
  }

  scope->resolveTypes();
}

void For::codegen0(BasicBlock *&block) {
  types::Type *type = gen->getType()->magicOut("__iter__", {});
  types::GenType *genType = type->asGen();

  if (!genType)
    throw exc::SeqException("cannot iterate over object of type '" +
                            type->getName() + "'");

  LLVMContext &context = block->getContext();
  BasicBlock *entry = block;
  Function *func = entry->getParent();

  Value *gen = this->gen->codegen(getBase(), entry);
  gen = this->gen->getType()->callMagic("__iter__", {}, gen, {}, entry,
                                        getTryCatch());

  IRBuilder<> builder(entry);
  BasicBlock *loopCont = BasicBlock::Create(context, "for_cont", func);
  BasicBlock *loop = BasicBlock::Create(context, "for", func);
  BasicBlock *loop0 = loop;
  builder.CreateBr(loop);

  builder.SetInsertPoint(loopCont);
  builder.CreateBr(loop);

  TryCatch *tc = getTryCatch();
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
  if (!genType->getBaseType(0)->is(types::Void)) {
    Value *val = genType->promise(gen, block);
    var->store(getBase(), val, block);
  }

  scope->codegen(block);

  builder.SetInsertPoint(block);
  builder.CreateBr(loop0);

  BasicBlock *cleanup = BasicBlock::Create(context, "cleanup", func);
  branch->setSuccessor(0, cleanup);
  genType->destroy(gen, cleanup);

  builder.SetInsertPoint(cleanup);
  BasicBlock *exit = BasicBlock::Create(context, "exit", func);
  builder.CreateBr(exit);
  block = exit;

  setBreaks(exit);
  setContinues(loopCont);
}

For *For::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (For *)ref->getClone(this);

  auto *x = new For(gen->clone(ref));
  ref->addClone(this, x);
  delete x->scope;
  x->scope = scope->clone(ref);
  x->var = var->clone(ref);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

Return::Return(Expr *expr) : Stmt("return"), expr(expr) {}

Expr *Return::getExpr() { return expr; }

void Return::resolveTypes() {
  if (expr)
    expr->resolveTypes();
}

void Return::codegen0(BasicBlock *&block) {
  if (TryCatch *tc = getTryCatch()) {
    // make sure we branch to finally block
    tc->codegenReturn(expr, block);
  } else {
    types::Type *type = expr ? expr->getType() : types::Void;
    Value *val = expr ? expr->codegen(getBase(), block) : nullptr;
    auto *func = dynamic_cast<Func *>(getBase());
    if (!func)
      throw exc::SeqException("misplaced return");
    func->codegenReturn(val, type, block);
  }
}

Return *Return::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (Return *)ref->getClone(this);

  auto *x = new Return(expr ? expr->clone(ref) : nullptr);
  ref->addClone(this, x);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

Yield::Yield(Expr *expr) : Stmt("yield"), expr(expr) {}

Expr *Yield::getExpr() { return expr; }

void Yield::resolveTypes() {
  if (expr)
    expr->resolveTypes();
}

void Yield::codegen0(BasicBlock *&block) {
  types::Type *type = expr ? expr->getType() : types::Void;
  Value *val = expr ? expr->codegen(getBase(), block) : nullptr;
  auto *func = dynamic_cast<Func *>(getBase());
  if (!func)
    throw exc::SeqException("misplaced yield");
  func->codegenYield(val, type, block);
}

Yield *Yield::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (Yield *)ref->getClone(this);

  auto *x = new Yield(expr ? expr->clone(ref) : nullptr);
  ref->addClone(this, x);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

Break::Break() : Stmt("break") {}

void Break::codegen0(BasicBlock *&block) {
  if (TryCatch *tc = getInnermostTryCatchBeforeLoop(this)) {
    // make sure we branch to finally block
    tc->codegenBreak(block);
  } else {
    LLVMContext &context = block->getContext();
    IRBuilder<> builder(block);
    BranchInst *inst =
        builder.CreateBr(block); // destination will be fixed by `setBreaks`
    addBreakToEnclosingLoop(inst);
    block = BasicBlock::Create(context, "", block->getParent());
  }
}

Break *Break::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (Break *)ref->getClone(this);

  auto *x = new Break();
  ref->addClone(this, x);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

Continue::Continue() : Stmt("continue") {}

void Continue::codegen0(BasicBlock *&block) {
  if (TryCatch *tc = getInnermostTryCatchBeforeLoop(this)) {
    // make sure we branch to finally block
    tc->codegenContinue(block);
  } else {
    LLVMContext &context = block->getContext();
    IRBuilder<> builder(block);
    BranchInst *inst =
        builder.CreateBr(block); // destination will be fixed by `setContinues`
    addContinueToEnclosingLoop(inst);
    block = BasicBlock::Create(context, "", block->getParent());
  }
}

Continue *Continue::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (Continue *)ref->getClone(this);

  auto *x = new Continue();
  ref->addClone(this, x);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}

Assert::Assert(Expr *expr, Expr *msg) : Stmt("assert"), expr(expr), msg(msg) {}

void Assert::resolveTypes() { expr->resolveTypes(); }

static bool isTest(BaseFunc *base) {
  auto *func = dynamic_cast<Func *>(base);
  return func && func->hasAttribute("test");
}

void Assert::codegen0(BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  Module *module = block->getModule();
  Function *func = block->getParent();
  const bool test = isTest(getBase());

  Value *check = expr->codegen(getBase(), block);
  check = expr->getType()->boolValue(check, block, getTryCatch());

  Value *msgVal = nullptr;
  if (msg) {
    msgVal = msg->codegen(getBase(), block);
    msgVal = msg->getType()->strValue(msgVal, block, getTryCatch());
  } else {
    msgVal = StrExpr("").codegen(getBase(), block);
  }

  BasicBlock *fail = BasicBlock::Create(context, "assert_fail", func);
  BasicBlock *pass = BasicBlock::Create(context, "assert_pass", func);

  IRBuilder<> builder(block);
  check = builder.CreateTrunc(check, builder.getInt1Ty());
  builder.CreateCondBr(check, pass, fail);
  builder.SetInsertPoint(fail);
  if (test) {
    Function *testFailed = Func::getBuiltin("_test_failed")->getFunc(module);
    Value *file = StrExpr(getSrcInfo().file).codegen(getBase(), fail);
    Value *line = IntExpr(getSrcInfo().line).codegen(getBase(), fail);
    builder.CreateCall(testFailed, {file, line, msgVal});
    builder.CreateBr(pass);
  } else {
    Func *f = Func::getBuiltin("_make_assert_error");
    Function *assertExc = f->getFunc(module);
    types::Type *assertErrorType = f->getFuncType()->getBaseType(0);

    Value *excVal = builder.CreateCall(assertExc, msgVal);
    ValueExpr excArg(assertErrorType, excVal);
    Throw raise(&excArg);
    raise.setBase(getBase());
    raise.setSrcInfo(getSrcInfo());
    raise.codegen(fail);

    builder.SetInsertPoint(fail);
    builder.CreateUnreachable();
  }

  block = pass;
}

Assert *Assert::clone(Generic *ref) {
  if (ref->seenClone(this))
    return (Assert *)ref->getClone(this);

  auto *x = new Assert(expr->clone(ref), msg ? msg->clone(ref) : msg);
  ref->addClone(this, x);
  Stmt::setCloneBase(x, ref);
  SEQ_RETURN_CLONE(x);
}
