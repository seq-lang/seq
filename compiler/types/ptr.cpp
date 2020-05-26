#include "lang/seq.h"

using namespace seq;
using namespace llvm;

types::PtrType::PtrType(Type *baseType)
    : Type("ptr", BaseType::get()), baseType(baseType) {}

Value *types::PtrType::defaultValue(BasicBlock *block) {
  LLVMContext &context = block->getContext();
  return ConstantPointerNull::get(
      PointerType::get(getBaseType(0)->getLLVMType(context), 0));
}

void types::PtrType::initOps() {
  if (!vtable.magic.empty())
    return;

  vtable.magic = {
      {"__init__",
       {},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return ConstantPointerNull::get(
             getBaseType(0)->getLLVMType(b.getContext())->getPointerTo(0));
       },
       false},

      {"__init__",
       {Int},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return getBaseType(0)->alloc(args[0], b.GetInsertBlock());
       },
       false},

      {"__init__",
       {PtrType::get(Base)},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateBitCast(args[0], getLLVMType(b.getContext()));
       },
       false},

      {"__int__",
       {},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreatePtrToInt(self, seqIntLLVM(b.getContext()));
       },
       false},

      {"__copy__",
       {},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},

      {"__bool__",
       {},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateIsNotNull(self),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__getitem__",
       {Int},
       getBaseType(0),
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Value *ptr = b.CreateGEP(self, args[0]);
         return b.CreateLoad(ptr);
       },
       false},

      {"__setitem__",
       {Int, getBaseType(0)},
       Void,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Value *ptr = b.CreateGEP(self, args[0]);
         b.CreateStore(args[1], ptr);
         return (Value *)nullptr;
       },
       false},

      {"__add__",
       {Int},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateGEP(self, args[0]);
       },
       false},

      {"__sub__",
       {this},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreatePtrDiff(self, args[0]);
       },
       false},

      {"__eq__",
       {this},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpEQ(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ne__",
       {this},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpNE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__lt__",
       {this},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpSLT(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__gt__",
       {this},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpSGT(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__le__",
       {this},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpSLE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ge__",
       {this},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpSGE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      /*
       * Prefetch magics are labeled [rw][0123] representing read/write and
       * locality. Instruction cache prefetch is not supported.
       * https://llvm.org/docs/LangRef.html#llvm-prefetch-intrinsic
       */

      {"__prefetch_r0__",
       {},
       Void,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateBitCast(self, b.getInt8PtrTy());
         Function *prefetch = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
         b.CreateCall(prefetch,
                      {self, b.getInt32(0), b.getInt32(0), b.getInt32(1)});
         return (Value *)nullptr;
       },
       false},

      {"__prefetch_r1__",
       {},
       Void,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateBitCast(self, b.getInt8PtrTy());
         Function *prefetch = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
         b.CreateCall(prefetch,
                      {self, b.getInt32(0), b.getInt32(1), b.getInt32(1)});
         return (Value *)nullptr;
       },
       false},

      {"__prefetch_r2__",
       {},
       Void,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateBitCast(self, b.getInt8PtrTy());
         Function *prefetch = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
         b.CreateCall(prefetch,
                      {self, b.getInt32(0), b.getInt32(2), b.getInt32(1)});
         return (Value *)nullptr;
       },
       false},

      {"__prefetch_r3__",
       {},
       Void,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateBitCast(self, b.getInt8PtrTy());
         Function *prefetch = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
         b.CreateCall(prefetch,
                      {self, b.getInt32(0), b.getInt32(3), b.getInt32(1)});
         return (Value *)nullptr;
       },
       false},

      {"__prefetch_w0__",
       {},
       Void,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateBitCast(self, b.getInt8PtrTy());
         Function *prefetch = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
         b.CreateCall(prefetch,
                      {self, b.getInt32(1), b.getInt32(0), b.getInt32(1)});
         return (Value *)nullptr;
       },
       false},

      {"__prefetch_w1__",
       {},
       Void,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateBitCast(self, b.getInt8PtrTy());
         Function *prefetch = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
         b.CreateCall(prefetch,
                      {self, b.getInt32(1), b.getInt32(1), b.getInt32(1)});
         return (Value *)nullptr;
       },
       false},

      {"__prefetch_w2__",
       {},
       Void,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateBitCast(self, b.getInt8PtrTy());
         Function *prefetch = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
         b.CreateCall(prefetch,
                      {self, b.getInt32(1), b.getInt32(2), b.getInt32(1)});
         return (Value *)nullptr;
       },
       false},

      {"__prefetch_w3__",
       {},
       Void,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateBitCast(self, b.getInt8PtrTy());
         Function *prefetch = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::prefetch);
         b.CreateCall(prefetch,
                      {self, b.getInt32(1), b.getInt32(3), b.getInt32(1)});
         return (Value *)nullptr;
       },
       false},
  };
}

bool types::PtrType::isAtomic() const { return false; }

bool types::PtrType::is(types::Type *type) const {
  return isGeneric(type) && (getBaseType(0)->is(types::Base) ||
                             type->getBaseType(0)->is(types::Base) ||
                             types::is(getBaseType(0), type->getBaseType(0)));
}

unsigned types::PtrType::numBaseTypes() const { return 1; }

types::Type *types::PtrType::getBaseType(unsigned idx) const {
  return baseType;
}

Type *types::PtrType::getLLVMType(LLVMContext &context) const {
  return PointerType::get(getBaseType(0)->getLLVMType(context), 0);
}

size_t types::PtrType::size(Module *module) const {
  return module->getDataLayout().getTypeAllocSize(
      getLLVMType(module->getContext()));
}

types::PtrType *types::PtrType::get(Type *baseType) noexcept {
  return new PtrType(baseType);
}

types::PtrType *types::PtrType::get() noexcept {
  return new PtrType(types::BaseType::get());
}

types::PtrType *types::PtrType::clone(Generic *ref) {
  return get(getBaseType(0)->clone(ref));
}
