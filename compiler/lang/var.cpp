#include "lang/seq.h"

using namespace seq;
using namespace llvm;

static void ensureNonVoid(types::Type *type) {
  if (type->is(types::Void))
    throw exc::SeqException("cannot load or store void variable");
}

static int nameIdx = 0;

Var::Var(types::Type *type)
    : name("seq.var." + std::to_string(nameIdx++)), type(type), ptr(nullptr),
      module(nullptr), global(false), tls(false), repl(false), external(false),
      mapped() {}

std::string Var::getName() { return name; }

void Var::setName(std::string name) { this->name = std::move(name); }

void Var::allocaIfNeeded(BaseFunc *base) {
  if (!mapped.empty())
    mapped.top()->allocaIfNeeded(base);

  if (module != base->getPreamble()->getModule()) {
    ptr = nullptr;
    module = base->getPreamble()->getModule();
  }

  if (ptr)
    return;

  LLVMContext &context = base->getContext();
  if (global) {
    Type *llvmType = getType()->getLLVMType(context);

    if (repl)
      llvmType = llvmType->getPointerTo();

    auto *g = new GlobalVariable(*module, llvmType, false,
                                 external ? GlobalValue::ExternalLinkage
                                          : GlobalValue::PrivateLinkage,
                                 Constant::getNullValue(llvmType), name);

    if (tls)
      g->setThreadLocalMode(GlobalVariable::ThreadLocalMode::LocalExecTLSModel);
    ptr = g;
  } else {
    ptr = makeAlloca(getType()->getLLVMType(context), base->getPreamble());
  }
}

bool Var::isGlobal() {
  if (!mapped.empty())
    return mapped.top()->isGlobal();
  return global;
}

void Var::setGlobal() {
  if (!mapped.empty())
    mapped.top()->setGlobal();
  else
    global = true;
}

void Var::setThreadLocal() {
  if (!mapped.empty())
    mapped.top()->setThreadLocal();
  else {
    global = true;
    tls = true;
  }
}

void Var::setREPL() {
  if (!mapped.empty())
    mapped.top()->setREPL();
  else {
    global = true;
    repl = true;
  }
}

void Var::setExternal() {
  if (!mapped.empty())
    mapped.top()->setExternal();
  else {
    global = true;
    external = true;
  }
}

void Var::reset() {
  ptr = nullptr;
  module = nullptr;
  mapped = std::stack<Var *>();
}

void Var::mapTo(Var *other) { mapped.push(other); }

void Var::unmap() { mapped.pop(); }

Value *Var::getPtr(BaseFunc *base) {
  if (!mapped.empty())
    return mapped.top()->getPtr(base);

  allocaIfNeeded(base);
  assert(ptr);
  return ptr;
}

Value *Var::load(BaseFunc *base, BasicBlock *block, bool atomic) {
  if (!mapped.empty())
    return mapped.top()->load(base, block);

  ensureNonVoid(getType());
  allocaIfNeeded(base);
  IRBuilder<> builder(block);
  auto *inst = builder.CreateLoad(ptr);
  Value *val = inst;

  if (repl) {
    inst = builder.CreateLoad(val);
    val = inst;
  }

  if (atomic) {
    inst->setAtomic(AtomicOrdering::SequentiallyConsistent);
    inst->setAlignment((unsigned)getType()->size(block->getModule()));
  }

  return val;
}

void Var::store(BaseFunc *base, Value *val, BasicBlock *block, bool atomic) {
  if (!mapped.empty()) {
    mapped.top()->store(base, val, block);
    return;
  }

  ensureNonVoid(getType());
  allocaIfNeeded(base);
  IRBuilder<> builder(block);
  Value *dest = repl ? builder.CreateLoad(ptr) : ptr;
  auto *inst = builder.CreateStore(val, dest);

  if (atomic) {
    inst->setAtomic(AtomicOrdering::SequentiallyConsistent);
    inst->setAlignment((unsigned)getType()->size(block->getModule()));
  }
}

void Var::setType(types::Type *type) {
  if (!mapped.empty())
    mapped.top()->setType(type);
  else
    this->type = type;
}

types::Type *Var::getType() {
  if (!mapped.empty())
    return mapped.top()->getType();
  return type ? type : types::Void;
}
