#include "sir/sir.h"
#include "util/llvm.h"
#include <stack>
#include <unordered_map>

namespace seq {
namespace ir {

template <typename V> using CacheBase = std::unordered_map<int, V *>;
template <typename K, typename V> class Cache : public CacheBase<V> {
public:
  using CacheBase<V>::CacheBase;

  V *operator[](const K *key) {
    auto it = CacheBase<V>::find(key->getId());
    return (it != CacheBase<V>::end()) ? it->second : nullptr;
  }

  template <class... Args> void set(const K *key, Args &&... args) {
    CacheBase<V>::emplace(key->getId(), std::forward<Args>(args)...);
  }
};

struct Loop {
  llvm::BasicBlock *breakBlock;
  llvm::BasicBlock *continueBlock;
};

class LLVMVisitor : public util::SIRVisitor {
private:
  /// LLVM context used for compilation
  llvm::LLVMContext context;
  /// Module we are compiling
  std::unique_ptr<llvm::Module> module;
  /// Current function we are compiling
  llvm::Function *func;
  /// Current basic block we are compiling
  llvm::BasicBlock *block;
  /// Last compiled value
  llvm::Value *value;
  /// LLVM type that was just translated from the IR type
  llvm::Type *type;
  /// LLVM values corresponding to IR variables
  Cache<Var, llvm::Value> vars;
  /// Loop data stack, containing break/continue blocks
  std::stack<Loop> loops;
  /// Whether we are compiling in debug mode
  bool debug;

  template <typename T> void process(const T &x) { x->accept(*this); }
  llvm::Value *call(llvm::Value *callee, llvm::ArrayRef<llvm::Value *> args);
  void enterLoop(llvm::BasicBlock *breakBlock, llvm::BasicBlock *continueBlock);
  void exitLoop();

public:
  LLVMVisitor(bool debug = false);

  // void visit(IRModule *) override;
  // void visit(Func *) override;
  // void visit(Var *) override;
  void visit(VarValue *) override;
  void visit(PointerValue *) override;
  // void visit(ValueProxy *) override;

  void visit(types::IntType *) override;
  void visit(types::FloatType *) override;
  void visit(types::BoolType *) override;
  void visit(types::ByteType *) override;
  void visit(types::VoidType *) override;
  void visit(types::RecordType *) override;
  void visit(types::RefType *) override;
  void visit(types::FuncType *) override;
  void visit(types::OptionalType *) override;
  void visit(types::ArrayType *) override;
  void visit(types::PointerType *) override;
  void visit(types::GeneratorType *) override;
  void visit(types::IntNType *) override;

  void visit(IntConstant *) override;
  void visit(FloatConstant *) override;
  void visit(BoolConstant *) override;
  void visit(StringConstant *) override;

  void visit(SeriesFlow *) override;
  void visit(IfFlow *) override;
  void visit(WhileFlow *) override;
  void visit(ForFlow *) override;
  void visit(TryCatchFlow *) override;

  void visit(AssignInstr *) override;
  void visit(ExtractInstr *) override;
  void visit(InsertInstr *) override;
  void visit(CallInstr *) override;
  void visit(StackAllocInstr *) override;
  void visit(YieldInInstr *) override;
  void visit(TernaryInstr *) override;
  void visit(BreakInstr *) override;
  void visit(ContinueInstr *) override;
  void visit(ReturnInstr *) override;
  void visit(YieldInstr *) override;
  void visit(ThrowInstr *) override;
  void visit(AssertInstr *) override;
  void visit(FlowInstr *) override;
};

LLVMVisitor::LLVMVisitor(bool debug)
    : util::SIRVisitor(), context(), module(), func(nullptr), block(nullptr),
      value(nullptr), type(nullptr), vars(), loops(), debug(debug) {
  (void)(this->debug);
}

llvm::Value *LLVMVisitor::call(llvm::Value *callee,
                               llvm::ArrayRef<llvm::Value *> args) {
  // TODO: check for invoke
  llvm::IRBuilder<> builder(block);
  return builder.CreateCall(callee, args);
}

void LLVMVisitor::enterLoop(llvm::BasicBlock *breakBlock,
                            llvm::BasicBlock *continueBlock) {
  loops.push({breakBlock, continueBlock});
}

void LLVMVisitor::exitLoop() {
  assert(!loops.empty());
  loops.pop();
}

/*
 * General values, functions, vars
 */

void LLVMVisitor::visit(VarValue *x) {
  llvm::Value *var = vars[x->getVar()];
  assert(var);
  llvm::IRBuilder<> builder(block);
  value = builder.CreateLoad(var);
}

void LLVMVisitor::visit(PointerValue *x) {
  llvm::Value *var = vars[x->getVar()];
  assert(var);
  value = var; // note: we don't load the pointer
}

/*
 * Types
 */

void LLVMVisitor::visit(types::IntType *x) { type = llvm::Type::getInt64Ty(context); }

void LLVMVisitor::visit(types::FloatType *x) {
  type = llvm::Type::getDoubleTy(context);
}

void LLVMVisitor::visit(types::BoolType *x) { type = llvm::Type::getInt8Ty(context); }

void LLVMVisitor::visit(types::ByteType *x) { type = llvm::Type::getInt8Ty(context); }

void LLVMVisitor::visit(types::VoidType *x) { type = llvm::Type::getVoidTy(context); }

void LLVMVisitor::visit(types::RecordType *x) {
  std::vector<llvm::Type *> body;
  for (const auto &field : *x) {
    process(field.type);
    body.push_back(type);
  }
  type = llvm::StructType::get(context, body);
}

void LLVMVisitor::visit(types::RefType *x) { type = llvm::Type::getInt8PtrTy(context); }

void LLVMVisitor::visit(types::FuncType *x) {
  process(x->getReturnType());
  llvm::Type *returnType = type;
  std::vector<llvm::Type *> argTypes;
  for (const auto &argType : *x) {
    process(argType);
    argTypes.push_back(type);
  }
  type = llvm::FunctionType::get(returnType, argTypes, /*isVarArg=*/false);
}

void LLVMVisitor::visit(types::OptionalType *x) {}

void LLVMVisitor::visit(types::ArrayType *x) {
  process(x->getBase());
  type = llvm::StructType::get(llvm::Type::getInt64Ty(context), type->getPointerTo());
}

void LLVMVisitor::visit(types::PointerType *x) {
  process(x->getBase());
  type = type->getPointerTo();
}

void LLVMVisitor::visit(types::GeneratorType *x) {
  type = llvm::Type::getInt8PtrTy(context);
}

void LLVMVisitor::visit(types::IntNType *x) {
  type = llvm::Type::getIntNTy(context, x->getLen());
}

/*
 * Constants
 */

void LLVMVisitor::visit(IntConstant *x) {
  llvm::IRBuilder<> builder(block);
  value = builder.getInt64(x->getVal());
}

void LLVMVisitor::visit(FloatConstant *x) {
  llvm::IRBuilder<> builder(block);
  value = llvm::ConstantFP::get(builder.getDoubleTy(), x->getVal());
}

void LLVMVisitor::visit(BoolConstant *x) {
  llvm::IRBuilder<> builder(block);
  value = builder.getInt8(x->getVal() ? 1 : 0);
}

void LLVMVisitor::visit(StringConstant *x) {
  llvm::IRBuilder<> builder(block);
  std::string s = x->getVal();
  auto *strVar = new llvm::GlobalVariable(
      *module, llvm::ArrayType::get(builder.getInt8Ty(), s.length() + 1), true,
      llvm::GlobalValue::PrivateLinkage, llvm::ConstantDataArray::getString(context, s),
      "str_literal");
  auto *strType = llvm::StructType::get(builder.getInt64Ty(), builder.getInt8PtrTy());
  llvm::Value *ptr = builder.CreateBitCast(strVar, builder.getInt8PtrTy());
  llvm::Value *len = builder.getInt64(s.length());
  llvm::Value *str = llvm::UndefValue::get(strType);
  str = builder.CreateInsertValue(str, ptr, 0);
  str = builder.CreateInsertValue(str, len, 1);
  value = str;
}

/*
 * Control flow
 */

void LLVMVisitor::visit(SeriesFlow *x) {
  for (const auto &value : *x) {
    process(value);
  }
}

void LLVMVisitor::visit(IfFlow *x) {
  auto *trueBlock = llvm::BasicBlock::Create(context, "if.true", func);
  auto *falseBlock = llvm::BasicBlock::Create(context, "if.false", func);
  auto *exitBlock = llvm::BasicBlock::Create(context, "if.exit", func);

  process(x->getCond());
  llvm::Value *cond = value;
  llvm::IRBuilder<> builder(block);
  cond = builder.CreateTrunc(cond, builder.getInt1Ty());
  builder.CreateCondBr(cond, trueBlock, falseBlock);

  block = trueBlock;
  process(x->getTrueBranch());
  builder.SetInsertPoint(block);
  builder.CreateBr(exitBlock);

  block = falseBlock;
  process(x->getFalseBranch());
  builder.SetInsertPoint(block);
  builder.CreateBr(exitBlock);

  block = exitBlock;
}

void LLVMVisitor::visit(WhileFlow *x) {
  auto *condBlock = llvm::BasicBlock::Create(context, "while.cond", func);
  auto *bodyBlock = llvm::BasicBlock::Create(context, "while.body", func);
  auto *exitBlock = llvm::BasicBlock::Create(context, "while.exit", func);

  llvm::IRBuilder<> builder(block);
  builder.CreateBr(condBlock);

  block = condBlock;
  process(x->getCond());
  llvm::Value *cond = value;
  builder.SetInsertPoint(block);
  cond = builder.CreateTrunc(cond, builder.getInt1Ty());
  builder.CreateCondBr(cond, bodyBlock, exitBlock);

  block = bodyBlock;
  enterLoop(/*breakBlock=*/exitBlock, /*continueBlock=*/condBlock);
  process(x->getBody());
  exitLoop();
  builder.SetInsertPoint(block);
  builder.CreateBr(condBlock);

  block = exitBlock;
}

void LLVMVisitor::visit(ForFlow *x) {
  process(x->getVar()->getType());
  llvm::Type *loopVarType = type;
  llvm::Value *loopVar = vars[x->getVar()];
  assert(loopVar);

  auto *condBlock = llvm::BasicBlock::Create(context, "for.cond", func);
  auto *bodyBlock = llvm::BasicBlock::Create(context, "for.body", func);
  auto *cleanupBlock = llvm::BasicBlock::Create(context, "for.cleanup", func);
  auto *exitBlock = llvm::BasicBlock::Create(context, "for.exit", func);

  // LLVM coroutine intrinsics
  // https://prereleases.llvm.org/6.0.0/rc3/docs/Coroutines.html
  llvm::Function *coroResume =
      llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_resume);
  llvm::Function *coroDone =
      llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_done);
  llvm::Function *coroPromise =
      llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_promise);
  llvm::Function *coroDestroy =
      llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::coro_destroy);

  process(x->getIter());
  llvm::Value *iter = value;
  llvm::IRBuilder<> builder(block);
  builder.CreateBr(condBlock);

  block = condBlock;
  call(coroResume, {iter});
  builder.SetInsertPoint(block);
  llvm::Value *done = builder.CreateCall(coroDone, iter);
  builder.CreateCondBr(done, cleanupBlock, bodyBlock);

  if (!loopVarType->isVoidTy()) {
    builder.SetInsertPoint(bodyBlock);
    llvm::Value *alignment =
        builder.getInt32(module->getDataLayout().getPrefTypeAlignment(type));
    llvm::Value *from = builder.getFalse();
    llvm::Value *promise = builder.CreateCall(coroPromise, {iter, alignment, from});
    promise = builder.CreateBitCast(promise, loopVarType->getPointerTo());
    llvm::Value *generatedValue = builder.CreateLoad(promise);
    builder.CreateStore(generatedValue, loopVar);
  }

  block = bodyBlock;
  enterLoop(/*breakBlock=*/exitBlock, /*continueBlock=*/condBlock);
  process(x->getBody());
  exitLoop();
  builder.SetInsertPoint(block);
  builder.CreateBr(condBlock);

  builder.SetInsertPoint(cleanupBlock);
  builder.CreateCall(coroDestroy, iter);
  builder.CreateBr(exitBlock);

  block = exitBlock;
}

void LLVMVisitor::visit(TryCatchFlow *x) {
  // TODO
  process(x->getBody());
  process(x->getFinally());
}

/*
 * Instructions
 */

void LLVMVisitor::visit(AssignInstr *x) {
  llvm::Value *var = vars[x->getLhs()];
  assert(var);
  process(x->getRhs());
  llvm::IRBuilder<> builder(block);
  builder.CreateStore(value, var);
}

void LLVMVisitor::visit(ExtractInstr *x) {
  auto *memberedType = x->getVal()->getType()->as<types::MemberedType>();
  assert(memberedType);
  const int index = memberedType->getMemberIndex(x->getField());
  assert(index >= 0);

  process(x->getVal());
  llvm::IRBuilder<> builder(block);
  if (memberedType->is<types::RefType>()) {
    value = builder.CreateLoad(value);
  }
  value = builder.CreateExtractValue(value, index);
}

void LLVMVisitor::visit(InsertInstr *x) {
  auto *refType = x->getLhs()->getType()->as<types::RefType>();
  assert(refType);
  const int index = refType->getMemberIndex(x->getField());
  assert(index >= 0);

  process(x->getLhs());
  llvm::Value *lhs = value;
  process(x->getRhs());
  llvm::Value *rhs = value;

  llvm::IRBuilder<> builder(block);
  llvm::Value *load = builder.CreateLoad(lhs);
  load = builder.CreateInsertValue(load, rhs, index);
  builder.CreateStore(load, lhs);
}

void LLVMVisitor::visit(CallInstr *x) {
  llvm::IRBuilder<> builder(block);
  process(x->getFunc());
  llvm::Value *func = value;

  std::vector<llvm::Value *> args;
  for (const ValuePtr &arg : *x) {
    builder.SetInsertPoint(block);
    process(arg);
    args.push_back(value);
  }

  value = call(func, args);
}

void LLVMVisitor::visit(YieldInInstr *x) {
  // TODO
}

void LLVMVisitor::visit(StackAllocInstr *x) {
  if (auto *arrayType = x->getType()->as<types::ArrayType>()) {
    process(arrayType->getBase());
  } else {
    assert(0 && "StackAllocInstr type is not an array type");
  }

  seq_int_t size = 0;
  if (auto *constSize = x->getCount()->as<IntConstant>()) {
    size = constSize->getVal();
  } else {
    assert(0 && "StackAllocInstr size is not constant");
  }

  llvm::IRBuilder<> builder(&func->getEntryBlock());
  auto *arrType = llvm::StructType::get(builder.getInt64Ty(), type->getPointerTo());
  llvm::Value *len = builder.getInt64(size);
  llvm::Value *ptr = builder.CreateAlloca(type, len);
  llvm::Value *arr = llvm::UndefValue::get(arrType);
  arr = builder.CreateInsertValue(arr, ptr, 0);
  arr = builder.CreateInsertValue(arr, len, 1);
  value = arr;
}

void LLVMVisitor::visit(TernaryInstr *x) {
  auto *trueBlock = llvm::BasicBlock::Create(context, "ternary.true", func);
  auto *falseBlock = llvm::BasicBlock::Create(context, "ternary.false", func);
  auto *exitBlock = llvm::BasicBlock::Create(context, "ternary.exit", func);

  process(x->getType());
  llvm::Type *valueType = type;

  process(x->getCond());
  llvm::Value *cond = value;

  llvm::IRBuilder<> builder(block);
  cond = builder.CreateTrunc(cond, builder.getInt1Ty());
  builder.CreateCondBr(cond, trueBlock, falseBlock);

  block = trueBlock;
  process(x->getTrueValue());
  llvm::Value *trueValue = value;
  builder.SetInsertPoint(block);
  builder.CreateBr(exitBlock);

  block = falseBlock;
  process(x->getFalseValue());
  llvm::Value *falseValue = value;
  builder.SetInsertPoint(block);
  builder.CreateBr(exitBlock);

  builder.SetInsertPoint(exitBlock);
  llvm::PHINode *phi = builder.CreatePHI(valueType, 2);
  phi->addIncoming(trueValue, trueBlock);
  phi->addIncoming(falseValue, falseBlock);
  value = phi;
  block = exitBlock;
}

void LLVMVisitor::visit(BreakInstr *x) {
  // TODO: check for finally block
  assert(!loops.empty());
  llvm::IRBuilder<> builder(block);
  builder.CreateBr(loops.top().breakBlock);
  block = llvm::BasicBlock::Create(context, "break.new", func);
}

void LLVMVisitor::visit(ContinueInstr *x) {
  // TODO: check for finally block
  assert(!loops.empty());
  llvm::IRBuilder<> builder(block);
  builder.CreateBr(loops.top().continueBlock);
  block = llvm::BasicBlock::Create(context, "continue.new", func);
}

void LLVMVisitor::visit(ReturnInstr *x) {
  // TODO: check for finally block
  const bool voidReturn = bool(x->getValue());
  if (!voidReturn) {
    process(x->getValue());
  }
  llvm::IRBuilder<> builder(block);
  if (!voidReturn) {
    builder.CreateRet(value);
  } else {
    builder.CreateRetVoid();
  }
  block = llvm::BasicBlock::Create(context, "return.new", func);
}

void LLVMVisitor::visit(YieldInstr *x) {
  // TODO
}

void LLVMVisitor::visit(ThrowInstr *x) {
  // TODO
}

void LLVMVisitor::visit(AssertInstr *x) {
  // TODO
}

void LLVMVisitor::visit(FlowInstr *x) {
  process(x->getFlow());
  process(x->getValue());
}

} // namespace ir
} // namespace seq
