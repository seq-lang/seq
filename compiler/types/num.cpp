#include "seq/seq.h"
#include <iostream>
#include <map>
#include <string>

using namespace seq;
using namespace llvm;

types::NumberType::NumberType() : Type("num", BaseType::get(), true) {}

types::IntType::IntType() : Type("int", NumberType::get(), false, true) {}

types::IntNType::IntNType(unsigned len, bool sign)
    : Type(std::string(sign ? "i" : "u") + std::to_string(len),
           NumberType::get(), false, true),
      len(len), sign(sign) {
  if (len == 0 || len > MAX_LEN)
    throw exc::SeqException("integer bit width must be between 1 and " +
                            std::to_string(MAX_LEN));
}

types::FloatType::FloatType() : Type("float", NumberType::get(), false, true) {}

types::BoolType::BoolType() : Type("bool", NumberType::get(), false, true) {}

types::ByteType::ByteType() : Type("byte", NumberType::get(), false, true) {}

Value *types::IntType::defaultValue(BasicBlock *block) {
  return ConstantInt::get(getLLVMType(block->getContext()), 0);
}

Value *types::IntNType::defaultValue(BasicBlock *block) {
  return ConstantInt::get(getLLVMType(block->getContext()), 0, sign);
}

Value *types::FloatType::defaultValue(BasicBlock *block) {
  return ConstantFP::get(getLLVMType(block->getContext()), 0.0);
}

Value *types::BoolType::defaultValue(BasicBlock *block) {
  return ConstantInt::get(getLLVMType(block->getContext()), 0);
}

Value *types::ByteType::defaultValue(BasicBlock *block) {
  return ConstantInt::get(getLLVMType(block->getContext()), 0);
}

void types::IntType::initOps() {
  if (!vtable.magic.empty())
    return;

  vtable.magic = {
      {"__init__",
       {},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return Int->defaultValue(b.GetInsertBlock());
       },
       false},

      {"__init__",
       {Int},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return args[0];
       },
       false},

      {"__init__",
       {Float},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateFPToSI(args[0], Int->getLLVMType(b.getContext()));
       },
       false},

      {"__init__",
       {Bool},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(args[0], Int->getLLVMType(b.getContext()));
       },
       false},

      {"__init__",
       {Byte},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(args[0], Int->getLLVMType(b.getContext()));
       },
       false},

      {"__str__",
       {},
       Str,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         LLVMContext &context = b.getContext();
         Module *module = b.GetInsertBlock()->getModule();
         auto *strFunc = cast<Function>(module->getOrInsertFunction(
             "seq_str_int", Str->getLLVMType(context), getLLVMType(context)));
         strFunc->setDoesNotThrow();
         return b.CreateCall(strFunc, self);
       },
       false},

      {"__copy__",
       {},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},

      {"__hash__",
       {},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},

      // int unary
      {"__bool__",
       {},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Value *zero = ConstantInt::get(Int->getLLVMType(b.getContext()), 0);
         return b.CreateZExt(b.CreateICmpNE(self, zero),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__pos__",
       {},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},

      {"__neg__",
       {},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateNeg(self);
       },
       false},

      {"__invert__",
       {},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateNot(self);
       },
       false},

      {"__abs__",
       {},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Value *pos = b.CreateICmpSGT(self, zeroLLVM(b.getContext()));
         Value *neg = b.CreateNeg(self);
         return b.CreateSelect(pos, self, neg);
       },
       false},

      // int,int binary
      {"__add__",
       {Int},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateAdd(self, args[0]);
       },
       false},

      {"__sub__",
       {Int},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateSub(self, args[0]);
       },
       false},

      {"__mul__",
       {Int},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateMul(self, args[0]);
       },
       false},

      {"__div__",
       {Int},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateSDiv(self, args[0]);
       },
       false},

      {"__truediv__",
       {Int},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         return b.CreateFDiv(self, args[0]);
       },
       false},

      {"__mod__",
       {Int},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateSRem(self, args[0]);
       },
       false},

      {"__lshift__",
       {Int},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateShl(self, args[0]);
       },
       false},

      {"__rshift__",
       {Int},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateAShr(self, args[0]);
       },
       false},

      {"__eq__",
       {Int},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpEQ(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ne__",
       {Int},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpNE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__lt__",
       {Int},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpSLT(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__gt__",
       {Int},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpSGT(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__le__",
       {Int},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpSLE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ge__",
       {Int},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpSGE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__and__",
       {Int},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateAnd(self, args[0]);
       },
       false},

      {"__or__",
       {Int},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateOr(self, args[0]);
       },
       false},

      {"__xor__",
       {Int},
       Int,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateXor(self, args[0]);
       },
       false},

      // int,float binary
      {"__add__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
         return b.CreateFAdd(self, args[0]);
       },
       false},

      {"__sub__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
         return b.CreateFSub(self, args[0]);
       },
       false},

      {"__mul__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
         return b.CreateFMul(self, args[0]);
       },
       false},

      {"__div__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
         Value *v = b.CreateFDiv(self, args[0]);
         Function *floor = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::floor,
             {Float->getLLVMType(b.getContext())});
         return b.CreateCall(floor, v);
       },
       false},

      {"__truediv__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
         return b.CreateFDiv(self, args[0]);
       },
       false},

      {"__mod__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
         return b.CreateFRem(self, args[0]);
       },
       false},

      {"__eq__",
       {Float},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
         return b.CreateZExt(b.CreateFCmpOEQ(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ne__",
       {Float},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
         return b.CreateZExt(b.CreateFCmpONE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__lt__",
       {Float},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
         return b.CreateZExt(b.CreateFCmpOLT(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__gt__",
       {Float},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
         return b.CreateZExt(b.CreateFCmpOGT(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__le__",
       {Float},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
         return b.CreateZExt(b.CreateFCmpOLE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ge__",
       {Float},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = b.CreateSIToFP(self, Float->getLLVMType(b.getContext()));
         return b.CreateZExt(b.CreateFCmpOGE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},
  };

  for (unsigned i = 1; i <= IntNType::MAX_LEN; i++) {
    vtable.magic.push_back(
        {"__init__",
         {IntNType::get(i, true)},
         Int,
         [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
           return b.CreateSExtOrTrunc(args[0],
                                      Int->getLLVMType(b.getContext()));
         }});

    vtable.magic.push_back(
        {"__init__",
         {IntNType::get(i, false)},
         Int,
         [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
           return b.CreateZExtOrTrunc(args[0],
                                      Int->getLLVMType(b.getContext()));
         }});
  }
}

static Function *getIntNPickleFunc(types::IntNType *type, Module *module) {
  const std::string name = "seq." + type->getName() + ".pickle";
  LLVMContext &context = module->getContext();
  Function *func = module->getFunction(name);

  if (!func) {
    func = cast<Function>(module->getOrInsertFunction(
        name, Type::getVoidTy(context), type->getLLVMType(context),
        IntegerType::getInt8PtrTy(context)));
    func->setDoesNotThrow();
    func->setLinkage(GlobalValue::PrivateLinkage);

    auto *gzWrite = cast<Function>(module->getOrInsertFunction(
        "gzwrite", IntegerType::getInt32Ty(context),
        IntegerType::getInt8PtrTy(context), IntegerType::getInt8PtrTy(context),
        IntegerType::getInt32Ty(context)));
    gzWrite->setDoesNotThrow();

    auto iter = func->arg_begin();
    Value *num = iter++;
    Value *fp = iter;
    BasicBlock *block = BasicBlock::Create(context, "entry", func);
    IRBuilder<> builder(block);
    Value *buf = builder.CreateAlloca(type->getLLVMType(context));
    builder.CreateStore(num, buf);
    Value *ptr = builder.CreateBitCast(buf, IntegerType::getInt8PtrTy(context));
    Value *size =
        ConstantInt::get(IntegerType::getInt32Ty(context), type->size(module));
    builder.CreateCall(gzWrite, {fp, ptr, size});
    builder.CreateRetVoid();
  }

  return func;
}

static Function *getIntNUnpickleFunc(types::IntNType *type, Module *module) {
  const std::string name = "seq." + type->getName() + ".unpickle";
  LLVMContext &context = module->getContext();
  Function *func = module->getFunction(name);

  if (!func) {
    func = cast<Function>(module->getOrInsertFunction(
        name, type->getLLVMType(context), IntegerType::getInt8PtrTy(context)));
    func->setDoesNotThrow();
    func->setLinkage(GlobalValue::PrivateLinkage);

    auto *gzRead = cast<Function>(module->getOrInsertFunction(
        "gzread", IntegerType::getInt32Ty(context),
        IntegerType::getInt8PtrTy(context), IntegerType::getInt8PtrTy(context),
        IntegerType::getInt32Ty(context)));
    gzRead->setDoesNotThrow();

    Value *fp = func->arg_begin();
    BasicBlock *block = BasicBlock::Create(context, "entry", func);
    IRBuilder<> builder(block);
    Value *buf = builder.CreateAlloca(type->getLLVMType(context));
    Value *ptr = builder.CreateBitCast(buf, IntegerType::getInt8PtrTy(context));
    Value *size =
        ConstantInt::get(IntegerType::getInt32Ty(context), type->size(module));
    builder.CreateCall(gzRead, {fp, ptr, size});
    Value *num = builder.CreateLoad(buf);
    builder.CreateRet(num);
  }

  return func;
}

void types::IntNType::initOps() {
  if (!vtable.magic.empty())
    return;

  vtable.magic = {
      {"__init__",
       {},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return defaultValue(b.GetInsertBlock());
       },
       false},

      {"__init__",
       {this},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return args[0];
       },
       false},

      {"__init__",
       {Int},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return sign
                    ? b.CreateSExtOrTrunc(args[0], getLLVMType(b.getContext()))
                    : b.CreateZExtOrTrunc(args[0], getLLVMType(b.getContext()));
       },
       false},

      {"__init__",
       {IntNType::get(len, !sign)},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return args[0];
       },
       false},

      {"__copy__",
       {},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},

      {"__hash__",
       {},
       Int,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return sign ? b.CreateSExtOrTrunc(self, seqIntLLVM(b.getContext()))
                     : b.CreateZExtOrTrunc(self, seqIntLLVM(b.getContext()));
       },
       false},

      // int unary
      {"__bool__",
       {},
       Bool,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Value *zero = defaultValue(b.GetInsertBlock());
         return b.CreateZExt(b.CreateICmpNE(self, zero),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__pos__",
       {},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},

      {"__neg__",
       {},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateNeg(self);
       },
       false},

      {"__invert__",
       {},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateNot(self);
       },
       false},

      // int,int binary
      {"__add__",
       {this},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateAdd(self, args[0]);
       },
       false},

      {"__sub__",
       {this},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateSub(self, args[0]);
       },
       false},

      {"__mul__",
       {this},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateMul(self, args[0]);
       },
       false},

      {"__div__",
       {this},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return sign ? b.CreateSDiv(self, args[0])
                     : b.CreateUDiv(self, args[0]);
       },
       false},

      {"__truediv__",
       {this},
       Float,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         self = sign ? b.CreateSIToFP(self, Float->getLLVMType(b.getContext()))
                     : b.CreateUIToFP(self, Float->getLLVMType(b.getContext()));
         args[0] =
             sign ? b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()))
                  : b.CreateUIToFP(args[0], Float->getLLVMType(b.getContext()));
         return b.CreateFDiv(self, args[0]);
       },
       false},

      {"__mod__",
       {this},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return sign ? b.CreateSRem(self, args[0])
                     : b.CreateURem(self, args[0]);
       },
       false},

      {"__lshift__",
       {this},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateShl(self, args[0]);
       },
       false},

      {"__rshift__",
       {this},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return sign ? b.CreateAShr(self, args[0])
                     : b.CreateLShr(self, args[0]);
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
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Value *cmp = sign ? b.CreateICmpSLT(self, args[0])
                           : b.CreateICmpULT(self, args[0]);
         return b.CreateZExt(cmp, Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__gt__",
       {this},
       Bool,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Value *cmp = sign ? b.CreateICmpSGT(self, args[0])
                           : b.CreateICmpUGT(self, args[0]);
         return b.CreateZExt(cmp, Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__le__",
       {this},
       Bool,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Value *cmp = sign ? b.CreateICmpSLE(self, args[0])
                           : b.CreateICmpULE(self, args[0]);
         return b.CreateZExt(cmp, Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ge__",
       {this},
       Bool,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Value *cmp = sign ? b.CreateICmpSGE(self, args[0])
                           : b.CreateICmpUGE(self, args[0]);
         return b.CreateZExt(cmp, Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__and__",
       {this},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateAnd(self, args[0]);
       },
       false},

      {"__or__",
       {this},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateOr(self, args[0]);
       },
       false},

      {"__xor__",
       {this},
       this,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateXor(self, args[0]);
       },
       false},

      {"__pickle__",
       {PtrType::get(Byte)},
       Void,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Function *pickleFunc =
             getIntNPickleFunc(this, b.GetInsertBlock()->getModule());
         b.CreateCall(pickleFunc, {self, args[0]});
         return (Value *)nullptr;
       },
       false},

      {"__unpickle__",
       {PtrType::get(Byte)},
       this,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Function *unpickleFunc =
             getIntNUnpickleFunc(this, b.GetInsertBlock()->getModule());
         return b.CreateCall(unpickleFunc, args[0]);
       },
       true},
  };

  if (!sign && len % 2 == 0) {
    vtable.magic.push_back(
        {"__init__",
         {KMer::get(len / 2)},
         this,
         [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
           return b.CreateBitCast(args[0], getLLVMType(b.getContext()));
         }});
  }

  addMethod("len",
            new BaseFuncLite(
                {}, types::IntType::get(),
                [this](Module *module) {
                  const std::string name = "seq." + getName() + ".len";
                  Function *func = module->getFunction(name);

                  if (!func) {
                    LLVMContext &context = module->getContext();
                    func = cast<Function>(
                        module->getOrInsertFunction(name, seqIntLLVM(context)));
                    func->setDoesNotThrow();
                    func->setLinkage(GlobalValue::PrivateLinkage);
                    func->addFnAttr(Attribute::AlwaysInline);
                    BasicBlock *block =
                        BasicBlock::Create(context, "entry", func);
                    IRBuilder<> builder(block);
                    builder.CreateRet(
                        ConstantInt::get(seqIntLLVM(context), this->len));
                  }

                  return func;
                }),
            true);

  if (!sign && len % 2 == 0) {
    types::KMer *kType = types::KMer::get(len / 2);
    addMethod(
        "as_kmer",
        new BaseFuncLite(
            {this}, kType,
            [this, kType](Module *module) {
              const std::string name = "seq." + getName() + ".as_kmer";
              Function *func = module->getFunction(name);

              if (!func) {
                LLVMContext &context = module->getContext();
                func = cast<Function>(module->getOrInsertFunction(
                    name, kType->getLLVMType(context), getLLVMType(context)));
                func->setDoesNotThrow();
                func->setLinkage(GlobalValue::PrivateLinkage);
                func->addFnAttr(Attribute::AlwaysInline);
                Value *arg = func->arg_begin();
                BasicBlock *block = BasicBlock::Create(context, "entry", func);
                IRBuilder<> builder(block);
                builder.CreateRet(
                    builder.CreateBitCast(arg, kType->getLLVMType(context)));
              }

              return func;
            }),
        true);
  }
}

void types::FloatType::initOps() {
  if (!vtable.magic.empty())
    return;

  vtable.magic = {
      {"__init__",
       {},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return Float->defaultValue(b.GetInsertBlock());
       },
       false},

      {"__init__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return args[0];
       },
       false},

      {"__init__",
       {Int},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
       },
       false},

      {"__str__",
       {},
       Str,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         LLVMContext &context = b.getContext();
         Module *module = b.GetInsertBlock()->getModule();
         auto *strFunc = cast<Function>(module->getOrInsertFunction(
             "seq_str_float", Str->getLLVMType(context), getLLVMType(context)));
         strFunc->setDoesNotThrow();
         return b.CreateCall(strFunc, self);
       },
       false},

      {"__copy__",
       {},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},

      // float unary
      {"__bool__",
       {},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Value *zero = ConstantFP::get(Float->getLLVMType(b.getContext()), 0.0);
         return b.CreateZExt(b.CreateFCmpONE(self, zero),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__pos__",
       {},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},

      {"__neg__",
       {},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateFNeg(self);
       },
       false},

      {"__abs__",
       {},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Function *abs = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::fabs,
             {Float->getLLVMType(b.getContext())});
         return b.CreateCall(abs, self);
       },
       false},

      // float,float binary
      {"__add__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateFAdd(self, args[0]);
       },
       false},

      {"__sub__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateFSub(self, args[0]);
       },
       false},

      {"__mul__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateFMul(self, args[0]);
       },
       false},

      {"__div__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Value *v = b.CreateFDiv(self, args[0]);
         Function *floor = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::floor,
             {Float->getLLVMType(b.getContext())});
         return b.CreateCall(floor, v);
       },
       false},

      {"__truediv__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateFDiv(self, args[0]);
       },
       false},

      {"__mod__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateFRem(self, args[0]);
       },
       false},

      {"__pow__",
       {Float},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Function *pow = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::pow,
             {Float->getLLVMType(b.getContext())});
         return b.CreateCall(pow, {self, args[0]});
       },
       false},

      {"__eq__",
       {Float},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateFCmpOEQ(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ne__",
       {Float},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateFCmpONE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__lt__",
       {Float},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateFCmpOLT(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__gt__",
       {Float},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateFCmpOGT(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__le__",
       {Float},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateFCmpOLE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ge__",
       {Float},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateFCmpOGE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      // float,int binary
      {"__add__",
       {Int},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         return b.CreateFAdd(self, args[0]);
       },
       false},

      {"__sub__",
       {Int},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         return b.CreateFSub(self, args[0]);
       },
       false},

      {"__mul__",
       {Int},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         return b.CreateFMul(self, args[0]);
       },
       false},

      {"__div__",
       {Int},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         Value *v = b.CreateFDiv(self, args[0]);
         Function *floor = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::floor,
             {Float->getLLVMType(b.getContext())});
         return b.CreateCall(floor, v);
       },
       false},

      {"__truediv__",
       {Int},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         return b.CreateFDiv(self, args[0]);
       },
       false},

      {"__mod__",
       {Int},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         return b.CreateFRem(self, args[0]);
       },
       false},

      {"__pow__",
       {Int},
       Float,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         Function *pow = Intrinsic::getDeclaration(
             b.GetInsertBlock()->getModule(), Intrinsic::pow,
             {Float->getLLVMType(b.getContext())});
         return b.CreateCall(pow, {self, args[0]});
       },
       false},

      {"__eq__",
       {Int},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         return b.CreateZExt(b.CreateFCmpOEQ(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ne__",
       {Int},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         return b.CreateZExt(b.CreateFCmpONE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__lt__",
       {Int},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         return b.CreateZExt(b.CreateFCmpOLT(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__gt__",
       {Int},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         return b.CreateZExt(b.CreateFCmpOGT(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__le__",
       {Int},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         return b.CreateZExt(b.CreateFCmpOLE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ge__",
       {Int},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         args[0] = b.CreateSIToFP(args[0], Float->getLLVMType(b.getContext()));
         return b.CreateZExt(b.CreateFCmpOGE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},
  };
}

void types::BoolType::initOps() {
  if (!vtable.magic.empty())
    return;

  vtable.magic = {
      {"__init__",
       {},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return Bool->defaultValue(b.GetInsertBlock());
       },
       false},

      {"__str__",
       {},
       Str,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         LLVMContext &context = b.getContext();
         Module *module = b.GetInsertBlock()->getModule();
         auto *strFunc = cast<Function>(module->getOrInsertFunction(
             "seq_str_bool", Str->getLLVMType(context), getLLVMType(context)));
         strFunc->setDoesNotThrow();
         return b.CreateCall(strFunc, self);
       },
       false},

      {"__copy__",
       {},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},

      {"__bool__",
       {},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},

      {"__invert__",
       {},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateNot(b.CreateTrunc(
                                 self, IntegerType::getInt1Ty(b.getContext()))),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__eq__",
       {Bool},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpEQ(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ne__",
       {Bool},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpNE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__and__",
       {Bool},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateAnd(self, args[0]);
       },
       false},

      {"__or__",
       {Bool},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateOr(self, args[0]);
       },
       false},

      {"__xor__",
       {Bool},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateXor(self, args[0]);
       },
       false},
  };
}

// ASCII complement table
static GlobalVariable *
getByteCompTable(Module *module,
                 const std::string &name = "seq.byte_comp_table") {
  LLVMContext &context = module->getContext();
  Type *ty = IntegerType::getInt8Ty(context);
  GlobalVariable *table = module->getGlobalVariable(name);

  if (!table) {
    std::vector<Constant *> v(256, ConstantInt::get(ty, 0));

    for (auto &a : v)
      a = ConstantInt::get(ty, 'N');

    std::string from = "ACBDGHKMNSRUTWVYacbdghkmnsrutwvy";
    std::string to = "TGVHCDMKNSYAAWBRTGVHCDMKNSYAAWBR";

    for (unsigned i = 0; i < from.size(); i++)
      v[from[i]] = ConstantInt::get(ty, (uint64_t)to[i]);

    auto *arrTy = llvm::ArrayType::get(ty, v.size());
    table =
        new GlobalVariable(*module, arrTy, true, GlobalValue::PrivateLinkage,
                           ConstantArray::get(arrTy, v), name);
  }

  return table;
}

void types::ByteType::initOps() {
  if (!vtable.magic.empty())
    return;

  vtable.magic = {
      {"__init__",
       {},
       Byte,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return Byte->defaultValue(b.GetInsertBlock());
       },
       false},

      {"__init__",
       {Byte},
       Byte,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return args[0];
       },
       false},

      {"__init__",
       {Int},
       Byte,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateTrunc(args[0], Byte->getLLVMType(b.getContext()));
       },
       false},

      {"__str__",
       {},
       Str,
       [this](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         LLVMContext &context = b.getContext();
         Module *module = b.GetInsertBlock()->getModule();
         auto *strFunc = cast<Function>(module->getOrInsertFunction(
             "seq_str_byte", Str->getLLVMType(context), getLLVMType(context)));
         strFunc->setDoesNotThrow();
         return b.CreateCall(strFunc, self);
       },
       false},

      {"__copy__",
       {},
       Byte,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return self;
       },
       false},

      {"__bool__",
       {},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         Value *zero = ConstantInt::get(Byte->getLLVMType(b.getContext()), 0);
         return b.CreateZExt(b.CreateICmpNE(self, zero),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__eq__",
       {Byte},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpEQ(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},

      {"__ne__",
       {Byte},
       Bool,
       [](Value *self, std::vector<Value *> args, IRBuilder<> &b) {
         return b.CreateZExt(b.CreateICmpNE(self, args[0]),
                             Bool->getLLVMType(b.getContext()));
       },
       false},
  };

  addMethod(
      "comp",
      new BaseFuncLite({Byte}, Byte,
                       [](Module *module) {
                         const std::string name = "seq.byte_comp";
                         Function *func = module->getFunction(name);

                         if (!func) {
                           GlobalVariable *table = getByteCompTable(module);

                           LLVMContext &context = module->getContext();
                           func = cast<Function>(module->getOrInsertFunction(
                               name, Byte->getLLVMType(context),
                               Byte->getLLVMType(context)));
                           func->setDoesNotThrow();
                           func->setLinkage(GlobalValue::PrivateLinkage);
                           func->addFnAttr(Attribute::AlwaysInline);
                           Value *arg = func->arg_begin();
                           BasicBlock *block =
                               BasicBlock::Create(context, "entry", func);
                           IRBuilder<> builder(block);
                           arg = builder.CreateZExt(arg, builder.getInt64Ty());
                           arg = builder.CreateInBoundsGEP(
                               table, {builder.getInt64(0), arg});
                           arg = builder.CreateLoad(arg);
                           builder.CreateRet(arg);
                         }

                         return func;
                       }),
      true);
}

Type *types::IntType::getLLVMType(LLVMContext &context) const {
  return seqIntLLVM(context);
}

Type *types::IntNType::getLLVMType(LLVMContext &context) const {
  return IntegerType::getIntNTy(context, len);
}

Type *types::FloatType::getLLVMType(LLVMContext &context) const {
  return llvm::Type::getDoubleTy(context);
}

Type *types::BoolType::getLLVMType(LLVMContext &context) const {
  return IntegerType::getInt8Ty(context);
}

Type *types::ByteType::getLLVMType(LLVMContext &context) const {
  return IntegerType::getInt8Ty(context);
}

size_t types::IntType::size(Module *module) const { return sizeof(seq_int_t); }

size_t types::IntNType::size(Module *module) const {
  return module->getDataLayout().getTypeAllocSize(
      getLLVMType(module->getContext()));
}

size_t types::FloatType::size(Module *module) const { return sizeof(double); }

size_t types::BoolType::size(Module *module) const { return sizeof(bool); }

size_t types::ByteType::size(Module *module) const { return 1; }

types::NumberType *types::NumberType::get() noexcept {
  static NumberType instance;
  return &instance;
}

types::IntType *types::IntType::get() noexcept {
  static IntType instance;
  return &instance;
}

types::IntNType *types::IntNType::get(unsigned len, bool sign) {
  static std::map<int, IntNType *> cache;

  int key = (sign ? 1 : -1) * (int)len;
  if (cache.find(key) != cache.end())
    return cache.find(key)->second;

  auto *intNType = new IntNType(len, sign);
  cache.insert({key, intNType});
  return intNType;
}

types::FloatType *types::FloatType::get() noexcept {
  static FloatType instance;
  return &instance;
}

types::BoolType *types::BoolType::get() noexcept {
  static BoolType instance;
  return &instance;
}

types::ByteType *types::ByteType::get() noexcept {
  static ByteType instance;
  return &instance;
}

bool types::IntNType::is(types::Type *type) const {
  auto *iN = dynamic_cast<types::IntNType *>(type);
  return iN && (len == iN->len && sign == iN->sign);
}
