#include "lang/seq.h"

using namespace seq;
using namespace llvm;

Expr::Expr(types::Type *type)
    : SrcObject(), type(type), tc(nullptr), name("") {}

Expr::Expr() : Expr(types::Void) {}

void Expr::setTryCatch(TryCatch *tc) { this->tc = tc; }

TryCatch *Expr::getTryCatch() { return tc; }

void Expr::resolveTypes() {}

Value *Expr::codegen(BaseFunc *base, BasicBlock *&block) {
  try {
    return codegen0(base, block);
  } catch (exc::SeqException &e) {
    if (e.getSrcInfo().line <= 0)
      e.setSrcInfo(getSrcInfo());
    throw e;
  }
}

types::Type *Expr::getType() const {
  try {
    return getType0();
  } catch (exc::SeqException &e) {
    if (e.getSrcInfo().line <= 0)
      e.setSrcInfo(getSrcInfo());
    throw e;
  }
}

types::Type *Expr::getType0() const { return type; }

void Expr::ensure(types::Type *type) {
  types::Type *actual = getType();
  if (!types::is(type, actual))
    throw exc::SeqException("expected '" + type->getName() + "', got '" +
                                getType()->getName() + "'",
                            getSrcInfo());
}

Expr *Expr::clone(Generic *ref) { return this; }

std::string Expr::getName() const { return name; }

TypeExpr::TypeExpr(types::Type *type) : Expr(type) { name = "type"; }

Value *TypeExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  throw exc::SeqException("misplaced type expression");
}

ValueExpr::ValueExpr(types::Type *type, Value *val) : Expr(type), val(val) {}

Value *ValueExpr::codegen0(BaseFunc *base, BasicBlock *&block) { return val; }

NoneExpr::NoneExpr() : ConstExpr(types::RefType::none()) {}

Value *NoneExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  return getType0()->defaultValue(block);
}

Const NoneExpr::constValue() const { return Const(); }

IntExpr::IntExpr(seq_int_t n) : ConstExpr(types::Int), n(n) {}

Value *IntExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  return ConstantInt::get(getType()->getLLVMType(context), (uint64_t)n, true);
}

Const IntExpr::constValue() const { return Const(n); }

seq_int_t IntExpr::value() const { return n; }

FloatExpr::FloatExpr(double f) : ConstExpr(types::Float), f(f) {}

Value *FloatExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  return ConstantFP::get(getType()->getLLVMType(context), f);
}

Const FloatExpr::constValue() const { return Const(f); }

double FloatExpr::value() const { return f; }

BoolExpr::BoolExpr(bool b) : ConstExpr(types::Bool), b(b) {}

Value *BoolExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  return ConstantInt::get(getType()->getLLVMType(context), b ? 1 : 0);
}

Const BoolExpr::constValue() const { return Const(b); }

bool BoolExpr::value() const { return b; }

StrExpr::StrExpr(std::string s) : ConstExpr(types::Str), s(std::move(s)) {}

Value *StrExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  Module *module = block->getModule();
  BasicBlock *preambleBlock = base->getPreamble();

  GlobalVariable *strVar = new GlobalVariable(
      *module,
      llvm::ArrayType::get(IntegerType::getInt8Ty(context), s.length() + 1),
      true, GlobalValue::PrivateLinkage,
      ConstantDataArray::getString(context, s), "str_literal");
  strVar->setAlignment(1);

  IRBuilder<> builder(preambleBlock);
  Value *str =
      builder.CreateBitCast(strVar, IntegerType::getInt8PtrTy(context));
  Value *len = ConstantInt::get(seqIntLLVM(context), s.length());
  return types::Str->make(str, len, preambleBlock);
}

Const StrExpr::constValue() const { return Const(s, /*seq=*/false); }

std::string StrExpr::value() const { return s; }

SeqExpr::SeqExpr(std::string s) : ConstExpr(types::Seq), s(std::move(s)) {}

Value *SeqExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  Module *module = block->getModule();
  BasicBlock *preambleBlock = base->getPreamble();

  GlobalVariable *seqVar = new GlobalVariable(
      *module,
      llvm::ArrayType::get(IntegerType::getInt8Ty(context), s.length() + 1),
      true, GlobalValue::PrivateLinkage,
      ConstantDataArray::getString(context, s), "seq_literal");
  seqVar->setAlignment(1);

  IRBuilder<> builder(preambleBlock);
  Value *seq =
      builder.CreateBitCast(seqVar, IntegerType::getInt8PtrTy(context));
  Value *len = ConstantInt::get(seqIntLLVM(context), s.length());
  return types::Seq->make(seq, len, preambleBlock);
}

Const SeqExpr::constValue() const { return Const(s, /*seq=*/true); }

std::string SeqExpr::value() const { return s; }

ListExpr::ListExpr(std::vector<Expr *> elems, types::Type *listType)
    : Expr(), elems(std::move(elems)), listType(listType) {}

void ListExpr::resolveTypes() {
  for (auto *elem : elems)
    elem->resolveTypes();
}

Value *ListExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  types::Type *type = getType();
  assert(!elems.empty());
  types::Type *elemType = elems[0]->getType();

  ConstructExpr construct(type, {});
  Value *list = construct.codegen(base, block);
  ValueExpr v(type, list);

  for (auto *elem : elems) {
    if (!types::is(elemType, elem->getType()))
      throw exc::SeqException("inconsistent list element types '" +
                              elemType->getName() + "' and '" +
                              elem->getType()->getName() + "'");

    Value *x = elem->codegen(base, block);
    GetElemExpr append(&v, "append");
    ValueExpr arg(elemType, x);
    CallExpr call(&append, {&arg});
    call.resolveTypes();
    call.codegen(base, block);
  }

  return list;
}

types::Type *ListExpr::getType0() const {
  if (elems.empty())
    throw exc::SeqException("cannot infer type of empty list");

  types::Type *elemType = elems[0]->getType();
  auto *ref = dynamic_cast<types::RefType *>(listType);
  assert(ref);
  return ref->realize({elemType});
}

ListExpr *ListExpr::clone(Generic *ref) {
  std::vector<Expr *> elemsCloned;
  for (auto *elem : elems)
    elemsCloned.push_back(elem->clone(ref));
  SEQ_RETURN_CLONE(new ListExpr(elemsCloned, listType->clone(ref)));
}

SetExpr::SetExpr(std::vector<Expr *> elems, types::Type *setType)
    : Expr(), elems(std::move(elems)), setType(setType) {}

void SetExpr::resolveTypes() {
  for (auto *elem : elems)
    elem->resolveTypes();
}

Value *SetExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  types::Type *type = getType();
  assert(!elems.empty());
  types::Type *elemType = elems[0]->getType();

  ConstructExpr construct(type, {});
  Value *set = construct.codegen(base, block);
  ValueExpr v(type, set);

  for (auto *elem : elems) {
    if (!types::is(elemType, elem->getType()))
      throw exc::SeqException("inconsistent set element types '" +
                              elemType->getName() + "' and '" +
                              elem->getType()->getName() + "'");

    Value *x = elem->codegen(base, block);
    GetElemExpr append(&v, "add");
    ValueExpr arg(elemType, x);
    CallExpr call(&append, {&arg});
    call.resolveTypes();
    call.codegen(base, block);
  }

  return set;
}

types::Type *SetExpr::getType0() const {
  if (elems.empty())
    throw exc::SeqException("cannot infer type of empty set");

  types::Type *elemType = elems[0]->getType();
  auto *ref = dynamic_cast<types::RefType *>(setType);
  assert(ref);
  return ref->realize({elemType});
}

SetExpr *SetExpr::clone(Generic *ref) {
  std::vector<Expr *> elemsCloned;
  for (auto *elem : elems)
    elemsCloned.push_back(elem->clone(ref));
  SEQ_RETURN_CLONE(new SetExpr(elemsCloned, setType->clone(ref)));
}

DictExpr::DictExpr(std::vector<Expr *> elems, types::Type *dictType)
    : Expr(), elems(std::move(elems)), dictType(dictType) {}

void DictExpr::resolveTypes() {
  for (auto *elem : elems)
    elem->resolveTypes();
}

Value *DictExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  types::Type *type = getType();
  assert(!elems.empty() && elems.size() % 2 == 0);
  types::Type *keyType = elems[0]->getType();
  types::Type *valType = elems[1]->getType();

  ConstructExpr construct(type, {});
  Value *dict = construct.codegen(base, block);

  for (unsigned i = 0; i < elems.size(); i += 2) {
    Expr *key = elems[i];
    Expr *val = elems[i + 1];

    if (!types::is(keyType, key->getType()))
      throw exc::SeqException("inconsistent dict key types '" +
                              keyType->getName() + "' and '" +
                              key->getType()->getName() + "'");

    if (!types::is(valType, val->getType()))
      throw exc::SeqException("inconsistent dict value types '" +
                              valType->getName() + "' and '" +
                              val->getType()->getName() + "'");

    Value *k = key->codegen(base, block);
    Value *v = val->codegen(base, block);
    type->callMagic("__setitem__", {keyType, valType}, dict, {k, v}, block,
                    getTryCatch());
  }

  return dict;
}

types::Type *DictExpr::getType0() const {
  if (elems.empty())
    throw exc::SeqException("cannot infer type of empty dict");

  assert(elems.size() % 2 == 0);
  types::Type *keyType = elems[0]->getType();
  types::Type *valType = elems[1]->getType();
  auto *ref = dynamic_cast<types::RefType *>(dictType);
  assert(ref);
  return ref->realize({keyType, valType});
}

DictExpr *DictExpr::clone(Generic *ref) {
  std::vector<Expr *> elemsCloned;
  for (auto *elem : elems)
    elemsCloned.push_back(elem->clone(ref));
  SEQ_RETURN_CLONE(new DictExpr(elemsCloned, dictType->clone(ref)));
}

/*
 * Assumes that comprehension bodies are structured so that `if` or `for`
 * parts are always the last statements in the blocks. This should really
 * always be the case though.
 */
static void setBodyBase(For *body, BaseFunc *base) {
  Block *inner = body->getBlock();
  body->setBase(base);
  while (!inner->stmts.empty()) {
    for (auto *stmt : inner->stmts)
      stmt->setBase(base);

    Stmt *stmt = inner->stmts.back();
    auto *next1 = dynamic_cast<For *>(stmt);
    auto *next2 = dynamic_cast<If *>(stmt);

    if (next1) {
      inner = next1->getBlock();
    } else if (next2) {
      inner = next2->getBlock();
    } else {
      break;
    }
  }
}

ListCompExpr::ListCompExpr(Expr *val, For *body, types::Type *listType,
                           bool realize)
    : Expr(), val(val), body(body), listType(listType), realize(realize) {}

void ListCompExpr::setBody(For *body) { this->body = body; }

void ListCompExpr::resolveTypes() {
  body->resolveTypes();
  val->resolveTypes();
}

Value *ListCompExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  BaseFunc *oldBase = body->getBase();
  setBodyBase(body, base);

  /*
   * If single-level and generator supports __len__, we want to call that method
   * and allocate a list of the correct length beforehand.
   */
  Expr *oldGen = nullptr;
  Expr *listLen = nullptr;
  if (body->getBlock()->stmts.empty()) {
    Expr *gen = body->getGen();
    types::Type *genType = gen->getType();
    if (genType->hasMethod("__len__")) {
      // codegen gen here:
      Value *genVal = gen->codegen(base, block);
      Value *lenVal = genType->lenValue(genVal, block, getTryCatch());
      listLen = new ValueExpr(types::Int, lenVal);

      // make sure we don't codegen gen twice, so swap out body's gen:
      oldGen = gen;
      body->setGen(new ValueExpr(genType, genVal));
    }
  }

  types::Type *type = getType();
  std::vector<Expr *> constructArgs;
  if (listLen)
    constructArgs.push_back(listLen);
  ConstructExpr construct(type, constructArgs);
  Value *list = construct.codegen(base, block);
  ValueExpr v(type, list);

  // find the innermost block, where we'll be adding to the collection
  Block *inner = body->getBlock();
  while (!inner->stmts.empty()) {
    Stmt *stmt = inner->stmts.back();
    auto *next1 = dynamic_cast<For *>(stmt);
    auto *next2 = dynamic_cast<If *>(stmt);

    if (next1) {
      inner = next1->getBlock();
    } else if (next2) {
      inner = next2->getBlock();
    } else {
      break;
    }
  }

  GetElemExpr append(&v, "append");
  CallExpr call(&append, {val});
  ExprStmt callStmt(&call);
  callStmt.setBase(base);
  callStmt.resolveTypes();

  inner->stmts.push_back(&callStmt);
  body->codegen(block);
  inner->stmts.pop_back();
  setBodyBase(body, oldBase);
  if (oldGen)
    body->setGen(oldGen);

  return list;
}

types::Type *ListCompExpr::getType0() const {
  if (!realize)
    return listType;

  types::Type *elemType = val->getType();
  auto *generic = dynamic_cast<Generic *>(listType);
  assert(generic);
  auto *realized =
      dynamic_cast<types::Type *>(generic->realizeGeneric({elemType}));
  assert(realized);
  return realized;
}

ListCompExpr *ListCompExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new ListCompExpr(val->clone(ref), body->clone(ref),
                                    listType->clone(ref), realize));
}

SetCompExpr::SetCompExpr(Expr *val, For *body, types::Type *setType,
                         bool realize)
    : Expr(), val(val), body(body), setType(setType), realize(realize) {}

void SetCompExpr::setBody(For *body) { this->body = body; }

void SetCompExpr::resolveTypes() {
  body->resolveTypes();
  val->resolveTypes();
}

Value *SetCompExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  BaseFunc *oldBase = body->getBase();
  setBodyBase(body, base);
  types::Type *type = getType();
  ConstructExpr construct(type, {});
  Value *set = construct.codegen(base, block);
  ValueExpr v(type, set);

  // find the innermost block, where we'll be adding to the collection
  Block *inner = body->getBlock();
  while (!inner->stmts.empty()) {
    Stmt *stmt = inner->stmts.back();
    auto *next1 = dynamic_cast<For *>(stmt);
    auto *next2 = dynamic_cast<If *>(stmt);

    if (next1) {
      inner = next1->getBlock();
    } else if (next2) {
      inner = next2->getBlock();
    } else {
      break;
    }
  }

  GetElemExpr append(&v, "add");
  CallExpr call(&append, {val});
  ExprStmt callStmt(&call);
  callStmt.setBase(base);
  callStmt.resolveTypes();

  inner->stmts.push_back(&callStmt);
  body->codegen(block);
  inner->stmts.pop_back();
  setBodyBase(body, oldBase);

  return set;
}

types::Type *SetCompExpr::getType0() const {
  if (!realize)
    return setType;

  types::Type *elemType = val->getType();
  auto *generic = dynamic_cast<Generic *>(setType);
  assert(generic);
  auto *realized =
      dynamic_cast<types::Type *>(generic->realizeGeneric({elemType}));
  assert(realized);
  return realized;
}

SetCompExpr *SetCompExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new SetCompExpr(val->clone(ref), body->clone(ref),
                                   setType->clone(ref), realize));
}

DictCompExpr::DictCompExpr(Expr *key, Expr *val, For *body,
                           types::Type *dictType, bool realize)
    : Expr(), key(key), val(val), body(body), dictType(dictType),
      realize(realize) {}

void DictCompExpr::setBody(For *body) { this->body = body; }

void DictCompExpr::resolveTypes() {
  body->resolveTypes();
  key->resolveTypes();
  val->resolveTypes();
}

Value *DictCompExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  BaseFunc *oldBase = body->getBase();
  setBodyBase(body, base);
  types::Type *type = getType();
  ConstructExpr construct(type, {});
  Value *dict = construct.codegen(base, block);
  ValueExpr v(type, dict);

  // find the innermost block, where we'll be adding to the collection
  Block *inner = body->getBlock();
  while (!inner->stmts.empty()) {
    Stmt *stmt = inner->stmts.back();
    auto *next1 = dynamic_cast<For *>(stmt);
    auto *next2 = dynamic_cast<If *>(stmt);

    if (next1) {
      inner = next1->getBlock();
    } else if (next2) {
      inner = next2->getBlock();
    } else {
      break;
    }
  }

  AssignIndex assignStmt(&v, key, val);
  assignStmt.setBase(base);
  assignStmt.resolveTypes();

  inner->stmts.push_back(&assignStmt);
  body->codegen(block);
  inner->stmts.pop_back();
  setBodyBase(body, oldBase);

  return dict;
}

types::Type *DictCompExpr::getType0() const {
  if (!realize)
    return dictType;

  types::Type *keyType = key->getType();
  types::Type *valType = val->getType();
  auto *generic = dynamic_cast<Generic *>(dictType);
  assert(generic);
  auto *realized =
      dynamic_cast<types::Type *>(generic->realizeGeneric({keyType, valType}));
  assert(realized);
  return realized;
}

DictCompExpr *DictCompExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new DictCompExpr(key->clone(ref), val->clone(ref),
                                    body->clone(ref), dictType->clone(ref),
                                    realize));
}

GenExpr::GenExpr(Expr *val, For *body, std::vector<Var *> captures)
    : Expr(), val(val), body(body), captures(std::move(captures)) {}

void GenExpr::setBody(For *body) { this->body = body; }

void GenExpr::resolveTypes() {
  body->resolveTypes();
  val->resolveTypes();
}

static std::string argName(unsigned i) { return "arg" + std::to_string(i); }

Value *GenExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  // find the innermost block, where we'll be yielding
  Block *inner = body->getBlock();
  while (!inner->stmts.empty()) {
    Stmt *stmt = inner->stmts.back();
    auto *next1 = dynamic_cast<For *>(stmt);
    auto *next2 = dynamic_cast<If *>(stmt);

    if (next1) {
      inner = next1->getBlock();
    } else if (next2) {
      inner = next2->getBlock();
    } else {
      break;
    }
  }

  Yield yieldStmt(val);
  inner->stmts.push_back(&yieldStmt);
  static int idx = 1;
  Func implicitGen;
  implicitGen.setName("seq.implicit_gen." + std::to_string(idx++));

  BaseFunc *oldBase = body->getBase();
  setBodyBase(body, &implicitGen);

  std::vector<types::Type *> inTypes;
  std::vector<std::string> names;
  for (unsigned i = 0; i < captures.size(); i++) {
    inTypes.push_back(captures[i]->getType());
    names.push_back(argName(i));
  }

  implicitGen.setIns(inTypes);
  implicitGen.setArgNames(names);

  // gather the args:
  std::vector<Value *> args;
  for (auto *var : captures)
    args.push_back(var->load(base, block));

  // make sure we codegen wrt function argument vars:
  for (unsigned i = 0; i < captures.size(); i++)
    captures[i]->mapTo(implicitGen.getArgVar(argName(i)));

  implicitGen.getBlock()->add(body);
  implicitGen.sawYield(&yieldStmt);
  implicitGen.codegen(block->getModule());

  // now call the generator:
  Module *module = block->getModule();
  Function *func = implicitGen.getFunc(module);
  Value *gen;
  // We codegen calls ourselves rather than going through funcType
  // to avoid problems with automatic optional conversions (we don't
  // want them here).
  IRBuilder<> builder(block);
  if (getTryCatch()) {
    LLVMContext &context = block->getContext();
    Function *parent = block->getParent();
    BasicBlock *unwind = getTryCatch()->getExceptionBlock();
    BasicBlock *normal = BasicBlock::Create(context, "normal", parent);
    gen = builder.CreateInvoke(func, normal, unwind, args);
    block = normal;
  } else {
    gen = builder.CreateCall(func, args);
  }

  setBodyBase(body, oldBase);
  inner->stmts.pop_back();
  for (auto *var : captures)
    var->unmap();

  return gen;
}

types::Type *GenExpr::getType0() const {
  return types::GenType::get(val->getType());
}

GenExpr *GenExpr::clone(Generic *ref) {
  std::vector<Var *> capturesCloned;
  for (auto *var : captures)
    capturesCloned.push_back(var->clone(ref));
  SEQ_RETURN_CLONE(
      new GenExpr(val->clone(ref), body->clone(ref), capturesCloned));
}

VarExpr::VarExpr(Var *var, bool atomic) : var(var), atomic(atomic) {}

Var *VarExpr::getVar() { return var; }

void VarExpr::setAtomic() { atomic = true; }

Value *VarExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  return var->load(base, block, atomic);
}

Var *VarExpr::getVar() const { return var; }

types::Type *VarExpr::getType0() const { return var->getType(); }

VarExpr *VarExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new VarExpr(var->clone(ref), atomic));
}

VarPtrExpr::VarPtrExpr(Var *var) : var(var) {}

Value *VarPtrExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  return var->getPtr(base);
}

types::Type *VarPtrExpr::getType0() const {
  return types::PtrType::get(var->getType());
}

VarPtrExpr *VarPtrExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new VarPtrExpr(var->clone(ref)));
}

FuncExpr::FuncExpr(BaseFunc *func, Expr *orig, std::vector<types::Type *> types)
    : func(func), types(std::move(types)), orig(orig) {
  name = "func";
}

FuncExpr::FuncExpr(BaseFunc *func, std::vector<types::Type *> types)
    : FuncExpr(func, nullptr, std::move(types)) {}

BaseFunc *FuncExpr::getFunc() { return func; }

std::vector<types::Type *> FuncExpr::getTypes() const { return types; }

bool FuncExpr::isRealized() const { return !types.empty(); }

void FuncExpr::setRealizeTypes(std::vector<seq::types::Type *> types) {
  this->types = std::move(types);
}

void FuncExpr::resolveTypes() {
  try {
    auto *f = dynamic_cast<Func *>(func);
    if (f) {
      if (!f->realized() && !types.empty()) {
        orig = new FuncExpr(func, types);
        func = f->realize(types);
      }
    } else if (!types.empty()) {
      throw exc::SeqException("cannot type-instantiate non-generic function");
    }

    func->resolveTypes();
  } catch (exc::SeqException &e) {
    e.setSrcInfo(getSrcInfo());
    throw e;
  }
}

Value *FuncExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  return func->getFunc(block->getModule());
}

types::Type *FuncExpr::getType0() const { return func->getFuncType(); }

Expr *FuncExpr::clone(Generic *ref) {
  if (orig)
    return orig->clone(ref);

  const bool cloneFunc =
      (dynamic_cast<Func *>(func) != dynamic_cast<Func *>(ref));
  std::vector<types::Type *> typesCloned;
  for (auto *type : types)
    typesCloned.push_back(type->clone(ref));
  SEQ_RETURN_CLONE(
      new FuncExpr(cloneFunc ? func->clone(ref) : func, typesCloned));
}

ArrayExpr::ArrayExpr(types::Type *type, Expr *count, bool doAlloca)
    : Expr(types::ArrayType::get(type)), count(count), doAlloca(doAlloca) {}

void ArrayExpr::resolveTypes() { count->resolveTypes(); }

Value *ArrayExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  auto *type = dynamic_cast<types::ArrayType *>(getType());
  assert(type != nullptr);
  count->ensure(types::Int);

  LLVMContext &context = block->getContext();
  Value *ptr = nullptr;
  Value *len = nullptr;

  if (doAlloca) {
    uint64_t lenLit = 0;
    if (auto *intExpr = dynamic_cast<IntExpr *>(count)) {
      lenLit = (uint64_t)intExpr->value();
    } else {
      throw exc::SeqException("alloca'd arrays require constant length");
    }

    BasicBlock *preambleBlock = base->getPreamble();
    IRBuilder<> builder(preambleBlock);
    len = ConstantInt::get(seqIntLLVM(context), lenLit);
    ptr = builder.CreateAlloca(type->getBaseType(0)->getLLVMType(context), len);
  } else {
    len = count->codegen(base, block);
    ptr = type->getBaseType(0)->alloc(len, block);
  }

  Value *arr = type->make(ptr, len, block);
  return arr;
}

ArrayExpr *ArrayExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new ArrayExpr(getType()->clone(ref)->getBaseType(0),
                                 count->clone(ref), doAlloca));
}

RecordExpr::RecordExpr(std::vector<Expr *> exprs,
                       std::vector<std::string> names)
    : exprs(std::move(exprs)), names(std::move(names)) {}

void RecordExpr::resolveTypes() {
  for (auto *expr : exprs)
    expr->resolveTypes();
}

Value *RecordExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  types::Type *type = getType();
  Value *rec = UndefValue::get(type->getLLVMType(context));
  unsigned idx = 0;

  IRBuilder<> builder(block);
  for (auto *expr : exprs) {
    Value *val = expr->codegen(base, block);
    builder.SetInsertPoint(block); // recall: 'codegen' can change the block
    rec = builder.CreateInsertValue(rec, val, idx++);
  }

  return rec;
}

types::Type *RecordExpr::getType0() const {
  std::vector<types::Type *> types;
  for (auto *expr : exprs)
    types.push_back(expr->getType());
  return names.empty() ? types::RecordType::get(types)
                       : types::RecordType::get(types, names);
}

RecordExpr *RecordExpr::clone(Generic *ref) {
  std::vector<Expr *> exprsCloned;
  for (auto *expr : exprs)
    exprsCloned.push_back(expr->clone(ref));
  SEQ_RETURN_CLONE(new RecordExpr(exprsCloned, names));
}

IsExpr::IsExpr(Expr *lhs, Expr *rhs) : Expr(), lhs(lhs), rhs(rhs) {}

void IsExpr::resolveTypes() {
  lhs->resolveTypes();
  rhs->resolveTypes();
}

Value *IsExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  types::RefType *ref1 = lhs->getType()->asRef();
  types::RefType *ref2 = rhs->getType()->asRef();

  if (!(ref1 && ref2 && types::is(ref1, ref2)))
    throw exc::SeqException(
        "both sides of 'is' expression must be of same reference type");

  Value *lhs = this->lhs->codegen(base, block);
  Value *rhs = this->rhs->codegen(base, block);
  IRBuilder<> builder(block);
  Value *is = builder.CreateICmpEQ(lhs, rhs);
  is = builder.CreateZExt(is, types::Bool->getLLVMType(context));
  return is;
}

types::Type *IsExpr::getType0() const { return types::Bool; }

IsExpr *IsExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new IsExpr(lhs->clone(ref), rhs->clone(ref)));
}

UOpExpr::UOpExpr(Op op, Expr *lhs) : Expr(), op(std::move(op)), lhs(lhs) {}

void UOpExpr::resolveTypes() { lhs->resolveTypes(); }

static void unsupportedUOpError(std::string symbol, types::Type *lhs) {
  throw exc::SeqException("bad operand type for unary " + symbol + ": '" +
                          lhs->getName() + "'");
}

Value *UOpExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  types::Type *lhsType = lhs->getType();
  Value *self = lhs->codegen(base, block);

  if (op == uop("!")) {
    Value *b = lhsType->boolValue(self, block, getTryCatch());
    return types::Bool->callMagic("__invert__", {}, b, {}, block,
                                  getTryCatch());
  } else {
    types::Type *outType =
        lhsType->magicOut(op.magic, {}, /*nullOnMissing=*/true);
    if (outType)
      return lhsType->callMagic(op.magic, {}, self, {}, block, getTryCatch());
    unsupportedUOpError(op.symbol, lhsType);
    return nullptr;
  }
}

types::Type *UOpExpr::getType0() const {
  types::Type *lhsType = lhs->getType();

  if (op == uop("!")) {
    return types::Bool;
  } else {
    types::Type *outType =
        lhsType->magicOut(op.magic, {}, /*nullOnMissing=*/true);
    if (outType)
      return outType;
    unsupportedUOpError(op.symbol, lhsType);
    return nullptr;
  }
}

UOpExpr *UOpExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new UOpExpr(op, lhs->clone(ref)));
}

BOpExpr::BOpExpr(Op op, Expr *lhs, Expr *rhs, bool inPlace)
    : Expr(), op(std::move(op)), lhs(lhs), rhs(rhs), inPlace(inPlace) {}

void BOpExpr::resolveTypes() {
  lhs->resolveTypes();
  rhs->resolveTypes();
}

static void unsupportedBOpError(std::string symbol, types::Type *lhs,
                                types::Type *rhs) {
  throw exc::SeqException("unsupported operand type(s) for " + symbol + ": '" +
                          lhs->getName() + "' and '" + rhs->getName() + "'");
}

Value *BOpExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  LLVMContext &context = block->getContext();

  /*
   * && and || are special-cased because of short-circuiting
   */
  if (op == bop("&&") || op == bop("||")) {
    const bool isAnd = (op == bop("&&"));

    Value *lhs = this->lhs->codegen(base, block);
    lhs = this->lhs->getType()->boolValue(lhs, block, getTryCatch());

    BasicBlock *b1 = BasicBlock::Create(context, "", block->getParent());

    IRBuilder<> builder(block);
    lhs = builder.CreateTrunc(lhs, IntegerType::getInt1Ty(context));
    BranchInst *branch =
        builder.CreateCondBr(lhs, b1, b1); // one branch changed below

    Value *rhs = this->rhs->codegen(base, b1);
    rhs = this->rhs->getType()->boolValue(rhs, b1, getTryCatch());
    builder.SetInsertPoint(b1);

    BasicBlock *b2 = BasicBlock::Create(context, "", block->getParent());
    builder.CreateBr(b2);
    builder.SetInsertPoint(b2);

    Type *boolTy = types::Bool->getLLVMType(context);
    Value *t = ConstantInt::get(boolTy, 1);
    Value *f = ConstantInt::get(boolTy, 0);

    PHINode *result = builder.CreatePHI(boolTy, 2);
    result->addIncoming(isAnd ? f : t, block);
    result->addIncoming(rhs, b1);

    branch->setSuccessor(isAnd ? 1 : 0, b2);
    block = b2;
    return result;
  } else {
    types::Type *lhsType = lhs->getType();
    types::Type *rhsType = rhs->getType();
    types::Type *outType = nullptr;
    Value *self = lhs->codegen(base, block);
    Value *arg = rhs->codegen(base, block);

    if (inPlace) {
      assert(!op.magicInPlace.empty());
      types::Type *outType =
          lhsType->magicOut(op.magicInPlace, {rhsType}, /*nullOnMissing=*/true);
      if (outType && types::is(outType, lhsType))
        return lhsType->callMagic(op.magicInPlace, {rhsType}, self, {arg},
                                  block, getTryCatch());
    }

    outType = lhsType->magicOut(op.magic, {rhsType}, /*nullOnMissing=*/true);
    if (outType)
      return lhsType->callMagic(op.magic, {rhsType}, self, {arg}, block,
                                getTryCatch());

    if (!op.magicReflected.empty()) {
      outType = rhsType->magicOut(op.magicReflected, {lhsType},
                                  /*nullOnMissing=*/true);
      if (outType)
        return rhsType->callMagic(op.magicReflected, {lhsType}, arg, {self},
                                  block, getTryCatch());
    }

    unsupportedBOpError(op.symbol, lhsType, rhsType);
    return nullptr;
  }
}

types::Type *BOpExpr::getType0() const {
  if (op == bop("&&") || op == bop("||")) {
    return types::Bool;
  } else {
    types::Type *lhsType = lhs->getType();
    types::Type *rhsType = rhs->getType();
    types::Type *outType = nullptr;

    if (inPlace) {
      assert(!op.magicInPlace.empty());
      outType =
          lhsType->magicOut(op.magicInPlace, {rhsType}, /*nullOnMissing=*/true);
      if (outType && types::is(outType, lhsType))
        return outType;
    }

    outType = lhsType->magicOut(op.magic, {rhsType}, /*nullOnMissing=*/true);
    if (outType)
      return outType;

    if (!op.magicReflected.empty()) {
      outType = rhsType->magicOut(op.magicReflected, {lhsType},
                                  /*nullOnMissing=*/true);
      if (outType)
        return outType;
    }

    unsupportedBOpError(op.symbol, lhsType, rhsType);
    return nullptr;
  }
}

BOpExpr *BOpExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new BOpExpr(op, lhs->clone(ref), rhs->clone(ref), inPlace));
}

AtomicExpr::AtomicExpr(AtomicExpr::Op op, Var *lhs, Expr *rhs)
    : Expr(types::Int), op(op), lhs(lhs), rhs(rhs) {}

void AtomicExpr::resolveTypes() { rhs->resolveTypes(); }

Value *AtomicExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  if (!lhs->getType()->is(types::Int) || !rhs->getType()->is(types::Int))
    throw exc::SeqException("atomic operations can only be done on ints");

  const auto ord = AtomicOrdering::SequentiallyConsistent;
  Value *ptr = lhs->getPtr(base);
  Value *val = rhs->codegen(base, block);
  IRBuilder<> builder(block);

  switch (op) {
  case XCHG:
    return builder.CreateAtomicRMW(AtomicRMWInst::BinOp::Xchg, ptr, val, ord);
  case ADD:
    return builder.CreateAtomicRMW(AtomicRMWInst::BinOp::Add, ptr, val, ord);
  case SUB:
    return builder.CreateAtomicRMW(AtomicRMWInst::BinOp::Sub, ptr, val, ord);
  case AND:
    return builder.CreateAtomicRMW(AtomicRMWInst::BinOp::And, ptr, val, ord);
  case NAND:
    return builder.CreateAtomicRMW(AtomicRMWInst::BinOp::Nand, ptr, val, ord);
  case OR:
    return builder.CreateAtomicRMW(AtomicRMWInst::BinOp::Or, ptr, val, ord);
  case XOR:
    return builder.CreateAtomicRMW(AtomicRMWInst::BinOp::Xor, ptr, val, ord);
  case MAX:
    return builder.CreateAtomicRMW(AtomicRMWInst::BinOp::Max, ptr, val, ord);
  case MIN:
    return builder.CreateAtomicRMW(AtomicRMWInst::BinOp::Min, ptr, val, ord);
  default:
    assert(0);
  }

  return nullptr;
}

AtomicExpr *AtomicExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new AtomicExpr(op, lhs->clone(ref), rhs->clone(ref)));
}

ArrayLookupExpr::ArrayLookupExpr(Expr *arr, Expr *idx) : arr(arr), idx(idx) {}

void ArrayLookupExpr::resolveTypes() {
  arr->resolveTypes();
  idx->resolveTypes();
}

static seq_int_t translateIndex(seq_int_t idx, seq_int_t len,
                                bool clamp = false) {
  if (idx < 0)
    idx += len;

  if (clamp) {
    if (idx < 0)
      idx = 0;
    if (idx > len)
      idx = len;
  } else if (idx < 0 || idx >= len) {
    throw exc::SeqException("tuple index " + std::to_string(idx) +
                            " out of bounds (len: " + std::to_string(len) +
                            ")");
  }

  return idx;
}

static types::RecordType *sliceTypeFromName(const std::string &name) {
  using types::Int;
  using types::RecordType;
  static RecordType *eslice = RecordType::get({Int}, {}, "eslice");
  static RecordType *rslice = RecordType::get({Int}, {}, "rslice");
  static RecordType *lslice = RecordType::get({Int}, {}, "lslice");
  static RecordType *slice = RecordType::get({Int, Int}, {}, "slice");
  static RecordType *esslice = RecordType::get({Int}, {}, "esslice");
  static RecordType *rsslice = RecordType::get({Int, Int}, {}, "rsslice");
  static RecordType *lsslice = RecordType::get({Int, Int}, {}, "lsslice");
  static RecordType *sslice = RecordType::get({Int, Int, Int}, {}, "sslice");
  static std::vector<RecordType *> slices = {eslice,  rslice,  lslice,  slice,
                                             esslice, rsslice, lsslice, sslice};
  for (RecordType *rec : slices) {
    if (rec->getName() == name)
      return rec;
  }
  return nullptr;
}

struct Slice {
  seq_int_t start;
  seq_int_t stop;
  seq_int_t step;
};

static Slice getSliceIndices(const std::vector<seq_int_t> &args,
                             types::RecordType *sliceType, seq_int_t length) {
  const std::string name = sliceType->getName();
  if (name == "eslice") {
    return {0, length, 1};
  } else if (name == "rslice") {
    return {args[0], length, 1};
  } else if (name == "lslice") {
    return {0, args[0], 1};
  } else if (name == "slice") {
    return {args[0], args[1], 1};
  } else if (name == "esslice") {
    const bool pos = args[0] > 0;
    return {pos ? 0 : length, pos ? length : 0, args[0]};
  } else if (name == "rsslice") {
    const bool pos = args[1] > 0;
    return {args[0], pos ? length : 0, args[1]};
  } else if (name == "lsslice") {
    const bool pos = args[1] > 0;
    return {pos ? 0 : length, args[0], args[1]};
  } else if (name == "sslice") {
    return {args[0], args[1], args[2]};
  } else {
    assert(0);
    return {0, 0, 0};
  }
}

// adapted from Python's PySlice_AdjustIndices
static seq_int_t sliceAdjustIndices(seq_int_t length, seq_int_t *start,
                                    seq_int_t *stop, seq_int_t step) {
  if (step == 0)
    throw exc::SeqException("slice step cannot be 0");

  if (*start < 0) {
    *start += length;
    if (*start < 0) {
      *start = (step < 0) ? -1 : 0;
    }
  } else if (*start >= length) {
    *start = (step < 0) ? length - 1 : length;
  }

  if (*stop < 0) {
    *stop += length;
    if (*stop < 0) {
      *stop = (step < 0) ? -1 : 0;
    }
  } else if (*stop >= length) {
    *stop = (step < 0) ? length - 1 : length;
  }

  if (step < 0) {
    if (*stop < *start) {
      return (*start - *stop - 1) / (-step) + 1;
    }
  } else {
    if (*start < *stop) {
      return (*stop - *start - 1) / step + 1;
    }
  }
  return 0;
}

static bool extractIntLiteral(Expr *expr, seq_int_t &result) {
  if (auto *idx = dynamic_cast<IntExpr *>(expr)) {
    result = idx->value();
    return true;
  } else {
    return false;
  }
}

static bool extractSliceLiteral(Expr *expr, std::vector<seq_int_t> &args,
                                types::RecordType *&sliceType) {
  auto *construct = dynamic_cast<ConstructExpr *>(expr);
  if (!construct)
    return false;
  auto *rec = dynamic_cast<types::RecordType *>(construct->getConstructType());
  if (!rec)
    return false;
  sliceType = sliceTypeFromName(rec->getName());
  if (!sliceType)
    return false;

  for (Expr *expr : construct->getArgs()) {
    seq_int_t n = 0;
    if (extractIntLiteral(expr, n)) {
      args.push_back(n);
    } else {
      return false;
    }
  }

  return args.size() == sliceType->numBaseTypes();
}

static bool getExprForTupleIndex(Expr *arr, Expr *idx, types::RecordType *rec,
                                 GetElemExpr *result) {
  seq_int_t idxLit = 0;
  if (extractIntLiteral(idx, idxLit) &&
      !rec->magicOut("__getitem__", {types::Int}, /*nullOnMissing=*/true,
                     /*overloadsOnly=*/true)) {
    seq_int_t idx = translateIndex(idxLit, rec->numBaseTypes());
    *result = GetElemExpr(arr,
                          (unsigned)(idx + 1)); // GetElemExpr is 1-based
    return true;
  }
  return false;
}

static bool getExprForTupleSlice(Expr *arr, Expr *idx, types::RecordType *rec,
                                 RecordExpr *result) {
  std::vector<seq_int_t> args;
  types::RecordType *sliceType = nullptr;
  if (extractSliceLiteral(idx, args, sliceType) &&
      !rec->magicOut("__getitem__", {sliceType}, /*nullOnMissing=*/true,
                     /*overloadsOnly=*/true)) {
    const seq_int_t length = rec->numBaseTypes();
    Slice s = getSliceIndices(args, sliceType, length);
    const seq_int_t resultLength =
        sliceAdjustIndices(length, &s.start, &s.stop, s.step);

    std::vector<Expr *> values;
    if (resultLength > 0) {
      for (seq_int_t i = s.start; (s.step >= 0) ? (i < s.stop) : (i >= s.stop);
           i += s.step) {
        values.push_back(new GetElemExpr(arr, (unsigned)(i + 1)));
      }
    }
    *result = RecordExpr(values);
    return true;
  }
  return false;
}

Value *ArrayLookupExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  types::Type *type = arr->getType();
  types::RecordType *rec = type->asRec();
  types::Type *idxType = this->idx->getType();

  // check if this is a record lookup, and that __getitem__ is not overriden
  if (rec) {
    // simple x[i]
    GetElemExpr e1(nullptr, {});
    if (getExprForTupleIndex(arr, idx, rec, &e1))
      return e1.codegen(base, block);

    // slice x[i:j:k] (or variant thereof)
    RecordExpr e2({});
    if (getExprForTupleSlice(arr, idx, rec, &e2))
      return e2.codegen(base, block);
  }

  Value *arr = this->arr->codegen(base, block);
  Value *idx = this->idx->codegen(base, block);

  if (auto *func = dynamic_cast<Func *>(base)) {
    if (func->hasAttribute("prefetch") &&
        type->magicOut("__prefetch__", {idxType}, /*nullOnMissing=*/true)) {
      type->callMagic("__prefetch__", {idxType}, arr, {idx}, block,
                      getTryCatch());
      func->codegenYield(nullptr, nullptr, block, true);
    }
  }

  return type->callMagic("__getitem__", {idxType}, arr, {idx}, block,
                         getTryCatch());
}

types::Type *ArrayLookupExpr::getType0() const {
  types::Type *type = arr->getType();
  types::RecordType *rec = type->asRec();

  // check if this is a record lookup, and that __getitem__ is not overriden
  if (rec) {
    // simple x[i]
    GetElemExpr e1(nullptr, {});
    if (getExprForTupleIndex(arr, idx, rec, &e1))
      return e1.getType();

    // slice x[i:j:k] (or variant thereof)
    RecordExpr e2({});
    if (getExprForTupleSlice(arr, idx, rec, &e2))
      return e2.getType();
  }

  return type->magicOut("__getitem__", {idx->getType()});
}

ArrayLookupExpr *ArrayLookupExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new ArrayLookupExpr(arr->clone(ref), idx->clone(ref)));
}

ArrayContainsExpr::ArrayContainsExpr(Expr *val, Expr *arr)
    : val(val), arr(arr) {}

void ArrayContainsExpr::resolveTypes() {
  val->resolveTypes();
  arr->resolveTypes();
}

Value *ArrayContainsExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  types::Type *valType = val->getType();
  types::Type *arrType = arr->getType();

  if (!arrType->magicOut("__contains__", {valType})->is(types::Bool))
    throw exc::SeqException("__contains__ does not return a boolean value");

  Value *val = this->val->codegen(base, block);
  Value *arr = this->arr->codegen(base, block);
  return arrType->callMagic("__contains__", {valType}, arr, {val}, block,
                            getTryCatch());
}

types::Type *ArrayContainsExpr::getType0() const { return types::Bool; }

ArrayContainsExpr *ArrayContainsExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new ArrayContainsExpr(val->clone(ref), arr->clone(ref)));
}

GetElemExpr::GetElemExpr(Expr *rec, std::string memb, GetElemExpr *orig,
                         std::vector<types::Type *> types)
    : rec(rec), memb(std::move(memb)), types(std::move(types)), orig(orig) {
  name = "elem";
}

GetElemExpr::GetElemExpr(Expr *rec, std::string memb,
                         std::vector<types::Type *> types)
    : GetElemExpr(rec, std::move(memb), nullptr, std::move(types)) {}

GetElemExpr::GetElemExpr(Expr *rec, unsigned memb,
                         std::vector<types::Type *> types)
    : GetElemExpr(rec, std::to_string(memb), std::move(types)) {}

Expr *GetElemExpr::getRec() { return rec; }

std::string GetElemExpr::getMemb() { return memb; }

bool GetElemExpr::isRealized() const { return !types.empty(); }

void GetElemExpr::setRealizeTypes(std::vector<types::Type *> types) {
  this->types = std::move(types);
}

void GetElemExpr::resolveTypes() { rec->resolveTypes(); }

static void noGenericMethodError(const std::string &typeName,
                                 const std::string &methodName) {
  throw exc::SeqException("generic method '" + methodName + "' of type '" +
                          typeName + "' does not exist");
}

Value *GetElemExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  types::Type *type = rec->getType();
  types::Type *outType = getType0();
  auto *func = type->hasMethod(memb)
                   ? dynamic_cast<Func *>(type->getMethod(memb))
                   : nullptr;
  Value *self = rec->codegen(base, block);
  ;

  if (!types.empty()) {
    if (!func)
      noGenericMethodError(type->getName(), memb);
    func = func->realize(types);
  }

  if (func && func->hasAttribute("property")) {
    Module *module = block->getModule();
    IRBuilder<> builder(block);
    return builder.CreateCall(func->getFunc(module), self);
  }

  if (!types.empty()) {
    Value *method = FuncExpr(func).codegen(base, block);
    auto *methodType = dynamic_cast<types::MethodType *>(outType);
    assert(methodType);
    return methodType->make(self, method, block);
  }

  return type->memb(self, memb, block);
}

types::Type *GetElemExpr::getType0() const {
  types::Type *type = rec->getType();
  auto *func = type->hasMethod(memb)
                   ? dynamic_cast<Func *>(type->getMethod(memb))
                   : nullptr;

  if (!types.empty()) {
    if (!func)
      noGenericMethodError(type->getName(), memb);
    func = func->realize(types);
  }

  if (func && func->hasAttribute("property"))
    return func->getFuncType()->getCallType({type});

  if (!types.empty())
    return types::MethodType::get(rec->getType(), func->getFuncType());

  return type->membType(memb);
}

GetElemExpr *GetElemExpr::clone(Generic *ref) {
  if (orig)
    return orig->clone(ref);

  std::vector<types::Type *> typesCloned;
  for (auto *type : types)
    typesCloned.push_back(type->clone(ref));
  SEQ_RETURN_CLONE(new GetElemExpr(rec->clone(ref), memb, typesCloned));
}

GetStaticElemExpr::GetStaticElemExpr(types::Type *type, std::string memb,
                                     GetStaticElemExpr *orig,
                                     std::vector<types::Type *> types)
    : Expr(), type(type), memb(std::move(memb)), types(std::move(types)),
      orig(orig) {
  name = "static";
}

GetStaticElemExpr::GetStaticElemExpr(types::Type *type, std::string memb,
                                     std::vector<types::Type *> types)
    : GetStaticElemExpr(type, std::move(memb), nullptr, std::move(types)) {}

types::Type *GetStaticElemExpr::getTypeInExpr() const { return type; }

std::string GetStaticElemExpr::getMemb() const { return memb; }

bool GetStaticElemExpr::isRealized() const { return !types.empty(); }

void GetStaticElemExpr::setRealizeTypes(std::vector<types::Type *> types) {
  this->types = std::move(types);
}

void GetStaticElemExpr::resolveTypes() {}

Value *GetStaticElemExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  FuncExpr f(type->getMethod(memb), types);
  f.resolveTypes();
  return f.codegen(base, block);
}

types::Type *GetStaticElemExpr::getType0() const {
  FuncExpr f(type->getMethod(memb), types);
  f.resolveTypes();
  return f.getType();
}

GetStaticElemExpr *GetStaticElemExpr::clone(Generic *ref) {
  if (orig)
    return orig->clone(ref);

  std::vector<types::Type *> typesCloned;
  for (auto *type : types)
    typesCloned.push_back(type->clone(ref));
  SEQ_RETURN_CLONE(new GetStaticElemExpr(type->clone(ref), memb, typesCloned));
}

CallExpr::CallExpr(Expr *func, std::vector<Expr *> args,
                   std::vector<std::string> names)
    : func(func), args(std::move(args)), names(std::move(names)) {}

Expr *CallExpr::getFuncExpr() const { return func; }

std::vector<Expr *> CallExpr::getArgs() const { return args; }

void CallExpr::setFuncExpr(Expr *func) { this->func = func; }

void CallExpr::resolveTypes() {
  func->resolveTypes();
  for (auto *arg : args)
    arg->resolveTypes();
}

static Func *getFuncFromFuncExpr(Expr *func) {
  auto *funcExpr = dynamic_cast<FuncExpr *>(func);
  if (funcExpr) {
    auto *f = dynamic_cast<Func *>(funcExpr->getFunc());
    if (f)
      return f;
  }

  return nullptr;
}

static bool
getFullCallTypesForPartial(Func *func, types::PartialFuncType *parType,
                           const std::vector<types::Type *> &argTypes,
                           std::vector<types::Type *> &typesFull) {
  if (func && func->numGenerics() > 0 && !func->realized() && parType) {
    // painful process of organizing types correctly...
    std::vector<types::Type *> callTypes = parType->getCallTypes();

    unsigned next = 0;
    for (auto *type : callTypes) {
      if (type) {
        typesFull.push_back(type);
      } else {
        if (next < argTypes.size())
          typesFull.push_back(argTypes[next++]);
        else
          return false;
      }
    }

    return true;
  }

  return false;
}

static void
deduceTypeParametersIfNecessary(Expr *&func,
                                const std::vector<types::Type *> &argTypes) {
  /*
   * Important note: If we're able to deduce type parameters, we change the
   * structure of the AST by replacing functions etc. However, in order for
   * generics/cloning to work properly, we need to preserve the original AST.
   * For this reason, FuncExpr and Get(Static)ElemExpr take an optional 'orig'
   * argument representing the original Expr to be returned when cloning.
   */

  try {
    {
      // simple call
      auto *funcExpr = dynamic_cast<FuncExpr *>(func);
      if (funcExpr && !funcExpr->isRealized()) {
        Func *f = getFuncFromFuncExpr(func);
        if (f && f->numGenerics() > 0 && !f->realized())
          func = new FuncExpr(f->realize(f->deduceTypesFromArgTypes(argTypes)),
                              func);
      }
    }

    {
      // partial call I -- known partial
      auto *partialExpr = dynamic_cast<PartialCallExpr *>(func);
      if (partialExpr) {
        auto *parType =
            dynamic_cast<types::PartialFuncType *>(partialExpr->getType());
        auto *funcExpr = dynamic_cast<FuncExpr *>(partialExpr->getFuncExpr());
        if (funcExpr && !funcExpr->isRealized()) {
          Func *f = getFuncFromFuncExpr(partialExpr->getFuncExpr());
          std::vector<types::Type *> typesFull;
          if (getFullCallTypesForPartial(f, parType, argTypes, typesFull))
            partialExpr->setFuncExpr(
                new FuncExpr(f->realize(f->deduceTypesFromArgTypes(typesFull)),
                             partialExpr->getFuncExpr()));
        }
      }
    }

    {
      // partial call II -- partial masquerading as regular call
      auto *callExpr = dynamic_cast<CallExpr *>(func);
      if (callExpr) {
        auto *parType =
            dynamic_cast<types::PartialFuncType *>(callExpr->getType());
        auto *funcExpr = dynamic_cast<FuncExpr *>(callExpr->getFuncExpr());
        if (funcExpr && !funcExpr->isRealized()) {
          Func *f = getFuncFromFuncExpr(callExpr->getFuncExpr());
          std::vector<types::Type *> typesFull;
          if (getFullCallTypesForPartial(f, parType, argTypes, typesFull))
            callExpr->setFuncExpr(
                new FuncExpr(f->realize(f->deduceTypesFromArgTypes(typesFull)),
                             callExpr->getFuncExpr()));
        }
      }
    }

    {
      // method call
      auto *elemExpr = dynamic_cast<GetElemExpr *>(func);
      if (elemExpr && !elemExpr->isRealized()) {
        std::string name = elemExpr->getMemb();
        types::Type *type = elemExpr->getRec()->getType();
        if (type->hasMethod(name)) {
          auto *f = dynamic_cast<Func *>(type->getMethod(name));
          if (f && f->numGenerics() > 0 && !f->realized()) {
            std::vector<types::Type *> typesFull(argTypes);
            typesFull.insert(typesFull.begin(),
                             type); // methods take 'self' as first argument
            func = new GetElemExpr(elemExpr->getRec(), name, elemExpr,
                                   f->deduceTypesFromArgTypes(typesFull));
          }
        }
      }
    }

    {
      // static method call
      auto *elemStaticExpr = dynamic_cast<GetStaticElemExpr *>(func);
      if (elemStaticExpr && !elemStaticExpr->isRealized()) {
        std::string name = elemStaticExpr->getMemb();
        types::Type *type = elemStaticExpr->getTypeInExpr();
        if (type->hasMethod(name)) {
          auto *f = dynamic_cast<Func *>(type->getMethod(name));
          if (f && f->numGenerics() > 0 && !f->realized())
            func = new GetStaticElemExpr(type, name, elemStaticExpr,
                                         f->deduceTypesFromArgTypes(argTypes));
        }
      }
    }
  } catch (exc::SeqException &) {
    /*
     * We weren't able to deduce type parameters now,
     * but we may be able to do so later, so ignore
     * any exceptions.
     */
  }
}

static std::vector<Expr *> rectifyCallArgs(Expr *func, std::vector<Expr *> args,
                                           std::vector<std::string> names) {
  {
    // simple call
    auto *funcExpr = dynamic_cast<FuncExpr *>(func);
    if (funcExpr) {
      Func *f = getFuncFromFuncExpr(func);
      if (f)
        return f->rectifyCallArgs(args, names);
    }
  }

  {
    // method call
    auto *elemExpr = dynamic_cast<GetElemExpr *>(func);
    if (elemExpr) {
      std::string name = elemExpr->getMemb();
      types::Type *type = elemExpr->getRec()->getType();
      if (type->hasMethod(name)) {
        auto *f = dynamic_cast<Func *>(type->getMethod(name));
        if (f)
          return f->rectifyCallArgs(args, names, /*methodCall=*/true);
      }
    }
  }

  {
    // static method call
    auto *elemStaticExpr = dynamic_cast<GetStaticElemExpr *>(func);
    if (elemStaticExpr) {
      std::string name = elemStaticExpr->getMemb();
      types::Type *type = elemStaticExpr->getTypeInExpr();
      if (type->hasMethod(name)) {
        auto *f = dynamic_cast<Func *>(type->getMethod(name));
        if (f)
          return f->rectifyCallArgs(args, names);
      }
    }
  }

  return args;
}

static bool isLiteralFalse(Expr *e) {
  if (auto *b = dynamic_cast<BoolExpr *>(e))
    return b->value() == false;
  return false;
}

static bool isLiteralNegOne(Expr *e) {
  if (auto *i = dynamic_cast<IntExpr *>(e))
    return i->value() == -1;
  return false;
}

Value *CallExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  types::Type *type = getType(); // validates call
  std::vector<Expr *> args = rectifyCallArgs(func, this->args, names);

  // catch inter-sequence alignment calls
  // arguments are defined in stdlib/align.seq:
  /*
      def align(self: seq,
              other: seq,
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
              splice_flank: bool = False,
              glob: bool = False):
    */
  // not all are supported for inter-sequence alignment!
  std::vector<std::string> argNames = {"",           "",
                                       "",           "",
                                       "",           "",
                                       "gapo2",      "gape2",
                                       "",           "",
                                       "",           "",
                                       "right",      "generic_sc",
                                       "approx_max", "approx_drop",
                                       "",           "",
                                       "splice",     "splice_fwd",
                                       "splice_rev", "splice_flank"};
  auto *baseFunc = dynamic_cast<Func *>(base);
  if (baseFunc && baseFunc->hasAttribute("inter_align")) {
    if (auto *elemExpr = dynamic_cast<GetElemExpr *>(func)) {
      Expr *self = elemExpr->getRec();
      std::string name = elemExpr->getMemb();
      if (name == "align") {
        types::Type *type = self->getType();
        if (type->is(types::Seq) && type->hasMethod(name)) {
          auto *f = dynamic_cast<Func *>(type->getMethod(name));
          if (f && f->hasAttribute("builtin")) {
            // make sure call is not partial
            bool isPartial = false;
            if (args.size() != argNames.size()) {
              isPartial = true;
            } else {
              for (Expr *e : args) {
                if (!e) {
                  isPartial = true;
                  break;
                }
              }
            }
            if (isPartial)
              throw exc::SeqException(
                  "inter-sequence alignment call cannot be partial");

            // make sure we're not using unsupported parameters
            assert(args.size() == argNames.size());
            for (unsigned i = 0; i < argNames.size(); i++) {
              if (argNames[i].empty())
                continue;
              bool unsupported = false;
              if (argNames[i] == "gapo2" || argNames[i] == "gape2")
                unsupported = !isLiteralNegOne(args[i]);
              else
                unsupported = !isLiteralFalse(args[i]);
              if (unsupported)
                throw exc::SeqException(
                    "inter-sequence alignment does not support argument '" +
                    argNames[i] + "'");
            }

            // expose params to pipeline codegen via the generator type
            types::GenType::InterAlignParams paramExprs;
            paramExprs.a = args[1];
            paramExprs.b = args[2];
            paramExprs.ambig = args[3];
            paramExprs.gapo = args[4];
            paramExprs.gape = args[5];
            paramExprs.score_only = args[11];
            paramExprs.bandwidth = args[8];
            paramExprs.zdrop = args[9];
            paramExprs.end_bonus = args[10];

            types::GenType *gen =
                baseFunc->getFuncType()->getBaseType(0)->asGen();
            assert(gen);
            gen->setAlignParams(paramExprs);

            // now do the codegen for this function:
            // first yield the sequences to be aligned, then read score
            // back from coroutine promise via yield expresssion.
            if (!self->getType()->is(types::Seq))
              throw exc::SeqException(
                  "query for inter-sequence alignment is not of type seq");
            if (!args[0]->getType()->is(types::Seq))
              throw exc::SeqException(
                  "target for inter-sequence alignment is not of type seq");
            Value *query = self->codegen(base, block);
            Value *target = args[0]->codegen(base, block);
            types::RecordType *yieldType = PipeExpr::getInterAlignYieldType();
            types::Type *resultType =
                yieldType->getBaseType(2); // (CIGAR, score)
            Value *yieldVal = yieldType->defaultValue(block);
            yieldVal = yieldType->setMemb(yieldVal, "query", query, block);
            yieldVal = yieldType->setMemb(yieldVal, "target", target, block);

            // set up flags
            static const int KSW_EZ_EXTZ_ONLY = 0x40;
            static const int KSW_EZ_REV_CIGAR = 0x80;
            IRBuilder<> builder(block);
            Value *flags = builder.getInt32(0);
            Value *b;
            // ext_only
            b = args[16]->codegen(base, block);
            builder.SetInsertPoint(block);
            b = builder.CreateTrunc(b, builder.getInt1Ty());
            b = builder.CreateSelect(b, builder.getInt32(KSW_EZ_EXTZ_ONLY),
                                     builder.getInt32(0));
            flags = builder.CreateOr(flags, b);
            // rev_cigar
            b = args[17]->codegen(base, block);
            builder.SetInsertPoint(block);
            b = builder.CreateTrunc(b, builder.getInt1Ty());
            b = builder.CreateSelect(b, builder.getInt32(KSW_EZ_REV_CIGAR),
                                     builder.getInt32(0));
            flags = builder.CreateOr(flags, b);

            // use the score field of the alignment to send flags
            flags = builder.CreateZExt(flags, seqIntLLVM(block->getContext()));
            Value *resultVal = resultType->defaultValue(block);
            resultVal = resultType->setMemb(resultVal, "_score", flags, block);
            yieldVal =
                yieldType->setMemb(yieldVal, "alignment", resultVal, block);

            baseFunc->codegenYield(yieldVal, yieldType, block);
            yieldVal = baseFunc->codegenYieldExpr(block, /*suspend=*/false);
            Value *alignment = yieldType->memb(yieldVal, "alignment", block);
            return alignment;
          }
        }
      }
    }
  }

  Value *f = func->codegen(base, block);
  std::vector<Value *> x;
  bool saw_null = false;
  for (auto *e : args) {
    if (e)
      x.push_back(e->codegen(base, block));
    else
      saw_null = true;
  }

  // check if this is really a partial function
  if (saw_null) {
    auto *partial = dynamic_cast<types::PartialFuncType *>(type);
    assert(partial);
    return partial->make(f, x, block);
  }

  if (getTryCatch()) {
    LLVMContext &context = block->getContext();
    Function *parent = block->getParent();
    BasicBlock *unwind = getTryCatch()->getExceptionBlock();
    BasicBlock *normal = BasicBlock::Create(context, "normal", parent);
    Value *v = func->getType()->call(base, f, x, block, normal, unwind);
    block = normal;
    return v;
  } else {
    return func->getType()->call(base, f, x, block, nullptr, nullptr);
  }
}

types::Type *CallExpr::getType0() const {
  std::vector<Expr *> args = rectifyCallArgs(func, this->args, names);
  std::vector<types::Type *> types;
  bool saw_null = false;
  for (auto *e : args) {
    if (e) {
      types.push_back(e->getType());
    } else {
      types.push_back(nullptr);
      saw_null = true;
    }
  }

  // check if this is really a partial function
  if (saw_null) {
    deduceTypeParametersIfNecessary(func, types);
    return types::PartialFuncType::get(func->getType(), types);
  }

  deduceTypeParametersIfNecessary(func, types);
  return func->getType()->getCallType(types);
}

CallExpr *CallExpr::clone(Generic *ref) {
  std::vector<Expr *> argsCloned;
  for (auto *arg : args)
    argsCloned.push_back(arg->clone(ref));
  SEQ_RETURN_CLONE(new CallExpr(func->clone(ref), argsCloned, names));
}

PartialCallExpr::PartialCallExpr(Expr *func, std::vector<Expr *> args,
                                 std::vector<std::string> names)
    : func(func), args(std::move(args)), names(std::move(names)) {}

Expr *PartialCallExpr::getFuncExpr() const { return func; }

std::vector<Expr *> PartialCallExpr::getArgs() const { return args; }

void PartialCallExpr::setFuncExpr(Expr *func) { this->func = func; }

void PartialCallExpr::resolveTypes() {
  func->resolveTypes();
  for (auto *arg : args) {
    if (arg)
      arg->resolveTypes();
  }
}

Value *PartialCallExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  types::PartialFuncType *par = getType0();
  std::vector<Expr *> args = rectifyCallArgs(func, this->args, names);

  Value *f = func->codegen(base, block);
  std::vector<Value *> x;
  for (auto *e : args) {
    if (e)
      x.push_back(e->codegen(base, block));
  }

  return par->make(f, x, block);
}

types::PartialFuncType *PartialCallExpr::getType0() const {
  std::vector<Expr *> args = rectifyCallArgs(func, this->args, names);
  std::vector<types::Type *> types;
  for (auto *e : args)
    types.push_back(e ? e->getType() : nullptr);

  deduceTypeParametersIfNecessary(func, types);
  return types::PartialFuncType::get(func->getType(), types);
}

PartialCallExpr *PartialCallExpr::clone(seq::Generic *ref) {
  std::vector<Expr *> argsCloned;
  for (auto *arg : args)
    argsCloned.push_back(arg ? arg->clone(ref) : nullptr);
  SEQ_RETURN_CLONE(new PartialCallExpr(func->clone(ref), argsCloned, names));
}

CondExpr::CondExpr(Expr *cond, Expr *ifTrue, Expr *ifFalse)
    : Expr(), cond(cond), ifTrue(ifTrue), ifFalse(ifFalse) {}

void CondExpr::resolveTypes() {
  cond->resolveTypes();
  ifTrue->resolveTypes();
  ifFalse->resolveTypes();
}

Value *CondExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  LLVMContext &context = block->getContext();
  Value *cond = this->cond->codegen(base, block);
  cond = this->cond->getType()->boolValue(cond, block, getTryCatch());
  IRBuilder<> builder(block);
  cond = builder.CreateTrunc(cond, IntegerType::getInt1Ty(context));

  BasicBlock *b1 = BasicBlock::Create(context, "", block->getParent());
  BranchInst *branch0 =
      builder.CreateCondBr(cond, b1, b1); // we set false-branch below

  Value *ifTrue = this->ifTrue->codegen(base, b1);
  builder.SetInsertPoint(b1);
  BranchInst *branch1 = builder.CreateBr(b1); // changed below

  BasicBlock *b2 = BasicBlock::Create(context, "", block->getParent());
  branch0->setSuccessor(1, b2);
  Value *ifFalse = this->ifFalse->codegen(base, b2);
  builder.SetInsertPoint(b2);
  BranchInst *branch2 = builder.CreateBr(b2); // changed below

  block = BasicBlock::Create(context, "", block->getParent());
  branch1->setSuccessor(0, block);
  branch2->setSuccessor(0, block);
  builder.SetInsertPoint(block);
  PHINode *result = builder.CreatePHI(getType()->getLLVMType(context), 2);
  result->addIncoming(ifTrue, b1);
  result->addIncoming(ifFalse, b2);
  return result;
}

types::Type *CondExpr::getType0() const {
  types::Type *trueType = ifTrue->getType();
  types::Type *falseType = ifFalse->getType();
  if (!types::is(trueType, falseType))
    throw exc::SeqException("inconsistent types '" + trueType->getName() +
                            "' and '" + falseType->getName() +
                            "' in conditional expression");

  return trueType;
}

CondExpr *CondExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(
      new CondExpr(cond->clone(ref), ifTrue->clone(ref), ifFalse->clone(ref)));
}

MatchExpr::MatchExpr() : Expr(), value(nullptr), patterns(), exprs() {}

void MatchExpr::setValue(Expr *value) {
  assert(!this->value);
  this->value = value;
}

void MatchExpr::addCase(Pattern *pattern, Expr *expr) {
  patterns.push_back(pattern);
  exprs.push_back(expr);
}

void MatchExpr::resolveTypes() {
  assert(value);
  value->resolveTypes();

  for (auto *pattern : patterns)
    pattern->resolveTypes(value->getType());

  for (auto *expr : exprs)
    expr->resolveTypes();
}

Value *MatchExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  assert(!patterns.empty());
  assert(patterns.size() == exprs.size() && value);

  LLVMContext &context = block->getContext();
  Function *func = block->getParent();

  IRBuilder<> builder(block);
  types::Type *valType = value->getType();
  types::Type *resType = getType();

  bool seenCatchAll = false;
  for (auto *pattern : patterns) {
    pattern->resolveTypes(valType);
    if (pattern->isCatchAll())
      seenCatchAll = true;
  }

  if (!seenCatchAll)
    throw exc::SeqException("match expression missing catch-all pattern");

  Value *val = value->codegen(base, block);

  std::vector<std::pair<BranchInst *, Value *>> binsts;

  for (unsigned i = 0; i < patterns.size(); i++) {
    Value *cond = patterns[i]->codegen(base, valType, val, block);

    builder.SetInsertPoint(block); // recall: expr codegen can change the block
    block = BasicBlock::Create(context, "", func); // match block
    BranchInst *binst1 =
        builder.CreateCondBr(cond, block, block); // we set false-branch below

    Value *result = exprs[i]->codegen(base, block);
    builder.SetInsertPoint(block);
    BranchInst *binst2 = builder.CreateBr(block); // we reset this below
    binsts.emplace_back(binst2, result);

    block = BasicBlock::Create(context, "",
                               func); // mismatch block (eval next pattern)
    binst1->setSuccessor(1, block);
  }

  builder.SetInsertPoint(block);
  builder.CreateUnreachable();

  block = BasicBlock::Create(context, "", func);
  builder.SetInsertPoint(block);

  PHINode *result = builder.CreatePHI(resType->getLLVMType(context),
                                      (unsigned)patterns.size());
  for (auto &binst : binsts) {
    binst.first->setSuccessor(0, block);
    result->addIncoming(binst.second, binst.first->getParent());
  }

  return result;
}

types::Type *MatchExpr::getType0() const {
  assert(!exprs.empty());
  types::Type *type = exprs[0]->getType();

  for (auto *expr : exprs) {
    if (!types::is(type, expr->getType()))
      throw exc::SeqException("inconsistent result types in match expression");
  }

  return type;
}

MatchExpr *MatchExpr::clone(Generic *ref) {
  auto *x = new MatchExpr();

  std::vector<Pattern *> patternsCloned;
  std::vector<Expr *> exprsCloned;

  for (auto *pattern : patterns)
    patternsCloned.push_back(pattern->clone(ref));

  for (auto *expr : exprs)
    exprsCloned.push_back(expr->clone(ref));

  if (value)
    x->value = value->clone(ref);
  x->patterns = patternsCloned;
  x->exprs = exprsCloned;

  SEQ_RETURN_CLONE(x);
}

ConstructExpr::ConstructExpr(types::Type *type, std::vector<Expr *> args,
                             std::vector<std::string> names)
    : Expr(), type(type), type0(nullptr), args(std::move(args)),
      names(std::move(names)) {
  // if all names are empty, clear names vector
  bool empty = true;
  for (const std::string &name : this->names) {
    if (!name.empty()) {
      empty = false;
      break;
    }
  }
  if (empty)
    this->names.clear();
}

types::Type *ConstructExpr::getConstructType() { return type; }

std::vector<Expr *> ConstructExpr::getArgs() { return args; }

void ConstructExpr::resolveTypes() {
  for (auto *arg : args)
    arg->resolveTypes();
}

Value *ConstructExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  LLVMContext &context = block->getContext();

  // special-case bool catch-all constructor:
  if (type->is(types::Bool) && args.size() == 1) {
    Value *val = args[0]->codegen(base, block);
    return args[0]->getType()->boolValue(val, block, getTryCatch());
  }

  // special-case str catch-all constructor:
  if (type->is(types::Str) && args.size() == 1) {
    Value *val = args[0]->codegen(base, block);
    return args[0]->getType()->strValue(val, block, getTryCatch());
  }

  Module *module = block->getModule();
  getType(); // validates construction

  std::vector<types::Type *> types;
  for (auto *arg : args)
    types.push_back(arg->getType());

  std::vector<Value *> vals;
  for (auto *arg : args)
    vals.push_back(arg->codegen(base, block));

  Value *self;

  if (type->hasMethod("__new__")) {
    self = type->callMagic("__new__", {}, nullptr, {}, block, getTryCatch());

    if (type->hasMethod("__del__")) {
      // make and register the finalizer
      static int idx = 1;
      auto *finalizeFunc = cast<Function>(module->getOrInsertFunction(
          "seq.finalizer." + std::to_string(idx++), Type::getVoidTy(context),
          IntegerType::getInt8PtrTy(context),
          IntegerType::getInt8PtrTy(context)));

      BasicBlock *entry = BasicBlock::Create(context, "entry", finalizeFunc);
      Value *obj = finalizeFunc->arg_begin();
      IRBuilder<> builder(entry);
      obj = builder.CreateBitCast(obj, type->getLLVMType(context));
      type->callMagic("__del__", {}, obj, {}, entry, nullptr);
      builder.SetInsertPoint(entry);
      builder.CreateRetVoid();

      auto *registerFunc = cast<Function>(module->getOrInsertFunction(
          "seq_register_finalizer", Type::getVoidTy(context),
          IntegerType::getInt8PtrTy(context), finalizeFunc->getType()));
      registerFunc->setDoesNotThrow();

      builder.SetInsertPoint(block);
      obj = builder.CreateBitCast(self, IntegerType::getInt8PtrTy(context));
      builder.CreateCall(registerFunc, {obj, finalizeFunc});
    }
  } else {
    // no __new__ defined, so just pass default value to __init__
    self = type->defaultValue(block);
  }

  Value *ret = type->callInit(types, names, self, vals, block, getTryCatch());
  return type->initOut(types, names)->is(types::Void) ? self : ret;
}

types::Type *ConstructExpr::getType0() const {
  // special-case bool catch-all constructor:
  if (type->is(types::Bool) && args.size() == 1)
    return types::Bool;

  // special-case str catch-all constructor:
  if (type->is(types::Str) && args.size() == 1)
    return types::Str;

  std::vector<types::Type *> types;
  for (auto *arg : args)
    types.push_back(arg->getType());

  // type parameter deduction if constructing generic class:
  auto *ref = dynamic_cast<types::RefType *>(type);
  if (ref && ref->numGenerics() > 0 && !ref->realized()) {
    type0 = type;
    type = ref->realize(ref->deduceTypesFromArgTypes(types, names));
  }

  types::Type *ret = type->initOut(types, names);
  return ret->is(types::Void) ? type : ret;
}

ConstructExpr *ConstructExpr::clone(Generic *ref) {
  std::vector<Expr *> argsCloned;
  for (auto *arg : args)
    argsCloned.push_back(arg->clone(ref));
  SEQ_RETURN_CLONE(
      new ConstructExpr((type0 ? type0 : type)->clone(ref), argsCloned, names));
}

MethodExpr::MethodExpr(Expr *self, Func *func)
    : Expr(), self(self), func(func) {}

void MethodExpr::resolveTypes() {
  self->resolveTypes();
  func->resolveTypes();
}

Value *MethodExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  types::MethodType *type = getType0();
  Value *self = this->self->codegen(base, block);
  Value *func = this->func->getFunc(block->getModule());
  return type->make(self, func, block);
}

types::MethodType *MethodExpr::getType0() const {
  return types::MethodType::get(self->getType(), func->getFuncType());
}

MethodExpr *MethodExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new MethodExpr(self->clone(ref), func->clone(ref)));
}

OptExpr::OptExpr(Expr *val) : Expr(), val(val) {}

void OptExpr::resolveTypes() { val->resolveTypes(); }

Value *OptExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  Value *val = this->val->codegen(base, block);
  return ((types::OptionalType *)getType())->make(val, block);
}

types::Type *OptExpr::getType0() const {
  return types::OptionalType::get(val->getType());
}

OptExpr *OptExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new OptExpr(val->clone(ref)));
}

YieldExpr::YieldExpr(BaseFunc *base) : Expr(), base(base) {}

void YieldExpr::resolveTypes() {
  if (dynamic_cast<Func *>(base))
    base->resolveTypes();
}

Value *YieldExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  auto *func = dynamic_cast<Func *>(base);
  assert(func);
  assert(base == this->base);
  return func->codegenYieldExpr(block);
}

types::Type *YieldExpr::getType0() const {
  if (auto *func = dynamic_cast<Func *>(base)) {
    types::GenType *gen = func->getFuncType()->getBaseType(0)->asGen();
    if (!gen)
      throw exc::SeqException("yield expression in non-generator");
    return gen->getBaseType(0);
  }
  throw exc::SeqException("yield expression outside function");
}

YieldExpr *YieldExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new YieldExpr(base->clone(ref)));
}

DefaultExpr::DefaultExpr(types::Type *type) : Expr(type) {}

Value *DefaultExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  return getType()->defaultValue(block);
}

DefaultExpr *DefaultExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new DefaultExpr(getType()->clone(ref)));
}

TypeOfExpr::TypeOfExpr(Expr *val) : Expr(types::Str), val(val) {}

void TypeOfExpr::resolveTypes() { val->resolveTypes(); }

Value *TypeOfExpr::codegen0(BaseFunc *base, BasicBlock *&block) {
  StrExpr s(val->getType()->getName());
  return s.codegen(base, block);
}

TypeOfExpr *TypeOfExpr::clone(Generic *ref) {
  SEQ_RETURN_CLONE(new TypeOfExpr(val->clone(ref)));
}
