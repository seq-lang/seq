#include "test.h"

#include "sir/util/matching.h"

using namespace seq::ir;

TEST_F(SIRCoreTest, MatchingEquivalentVar) {
  auto *first = module->Nr<Var>(module->getIntType());
  auto *second = module->Nr<Var>(module->getIntType());
  ASSERT_TRUE(util::match(first, second));
  ASSERT_TRUE(
      util::match(first, module->Nr<util::AnyVar>(module->Nr<util::AnyType>("baz"))));
}

TEST_F(SIRCoreTest, MatchingNonEquivalentVar) {
  auto *first = module->Nr<Var>(module->getIntType());
  auto *second = module->Nr<Var>(module->getFloatType());
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingEquivalentFunc) {
  {
    auto *first = module->Nr<BodiedFunc>();
    first->realize(module->getDummyFuncType(), {});
    auto *second = module->Nr<BodiedFunc>();
    second->realize(module->getDummyFuncType(), {});

    first->setBuiltin();
    second->setBuiltin();

    ASSERT_TRUE(util::match(first, second));
  }
  {
    auto *first = module->Nr<ExternalFunc>();
    first->realize(module->getDummyFuncType(), {});
    auto *second = module->Nr<ExternalFunc>();
    second->realize(module->getDummyFuncType(), {});

    first->setUnmangledName("baz");
    second->setUnmangledName("baz");

    ASSERT_TRUE(util::match(first, second));
  }
  {
    auto *first = module->Nr<LLVMFunc>();
    first->realize(module->getDummyFuncType(), {});
    auto *second = module->Nr<LLVMFunc>();
    second->realize(module->getDummyFuncType(), {});

    ASSERT_TRUE(util::match(first, second));
  }
}

TEST_F(SIRCoreTest, MatchingNonEquivalentFunc) {
  {
    auto *first = module->Nr<BodiedFunc>();
    first->realize(module->getDummyFuncType(), {});
    auto *second = module->Nr<BodiedFunc>();
    second->realize(module->getDummyFuncType(), {});

    first->setBuiltin();

    ASSERT_FALSE(util::match(first, second));
  }
  {
    auto *first = module->Nr<ExternalFunc>();
    first->realize(module->getDummyFuncType(), {});
    auto *second = module->Nr<ExternalFunc>();
    second->realize(module->getDummyFuncType(), {});

    first->setUnmangledName("baz");
    second->setUnmangledName("bar");

    ASSERT_FALSE(util::match(first, second));
  }
  {
    auto *first = module->Nr<LLVMFunc>();
    first->realize(module->getDummyFuncType(), {});
    auto *second = module->Nr<LLVMFunc>();
    second->realize(module->getDummyFuncType(), {});

    first->setLLVMLiterals({types::Generic(1)});

    ASSERT_FALSE(util::match(first, second));
  }
}

TEST_F(SIRCoreTest, MatchingAnyValue) {
  auto *first = module->Nr<VarValue>(module->Nr<Var>(module->getIntType()));
  ASSERT_TRUE(util::match(first, module->Nr<util::AnyValue>()));
}

TEST_F(SIRCoreTest, MatchingVarValue) {
  auto *first = module->Nr<VarValue>(module->Nr<Var>(module->getIntType()));
  auto *second = module->Nr<VarValue>(module->Nr<Var>(module->getIntType()));
  ASSERT_TRUE(util::match(first, second));
  first->setVar(module->Nr<Var>(module->getFloatType()));
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingPointerValue) {
  auto *first = module->Nr<PointerValue>(module->Nr<Var>(module->getIntType()),
                                         module->getPointerType(module->getIntType()));
  auto *second = module->Nr<PointerValue>(module->Nr<Var>(module->getIntType()),
                                          module->getPointerType(module->getIntType()));
  ASSERT_TRUE(util::match(first, second));
  first->setVar(module->Nr<Var>(module->getFloatType()));
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingSeriesFlow) {
  auto *first = module->Nr<SeriesFlow>();
  auto *second = module->Nr<SeriesFlow>();

  first->push_back(module->Nr<IntConstant>(1, module->getIntType()));
  second->push_back(module->Nr<IntConstant>(1, module->getIntType()));
  ASSERT_TRUE(util::match(first, second));
  second->push_back(module->Nr<IntConstant>(1, module->getIntType()));
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingIfFlow) {
  auto *cond = module->Nr<BoolConstant>(true, module->getBoolType());
  auto *tVal = module->Nr<SeriesFlow>();
  auto *first = module->Nr<IfFlow>(cond, tVal);
  auto *second = module->Nr<IfFlow>(cond->clone(), tVal->clone());

  ASSERT_TRUE(util::match(first, second));
  second->setFalseBranch(tVal->clone());
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingForFlow) {
  auto *body = module->Nr<SeriesFlow>();
  auto *var = module->Nr<Var>(module->getIntType());
  auto *iter = module->Nr<StringConstant>("hello", module->getStringType());
  auto *first = module->Nr<ForFlow>(iter, body, var);
  auto *second = module->Nr<ForFlow>(iter->clone(), body->clone(), var->clone());

  ASSERT_TRUE(util::match(first, second));
  second->setIter(module->Nr<StringConstant>("foo", module->getStringType()));
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingIntConstant) {
  auto *first = module->Nr<IntConstant>(0, module->getIntType());
  auto *second = module->Nr<IntConstant>(0, module->getIntType());
  ASSERT_TRUE(util::match(first, second));
  first->setVal(2);
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingFloatConstant) {
  auto *first = module->Nr<FloatConstant>(0.0, module->getFloatType());
  auto *second = module->Nr<FloatConstant>(0.0, module->getFloatType());
  ASSERT_TRUE(util::match(first, second));
  first->setVal(2.0);
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingBoolConstant) {
  auto *first = module->Nr<BoolConstant>(false, module->getBoolType());
  auto *second = module->Nr<BoolConstant>(false, module->getBoolType());
  ASSERT_TRUE(util::match(first, second));
  first->setVal(true);
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingStringConstant) {
  auto *first = module->Nr<StringConstant>("hi", module->getStringType());
  auto *second = module->Nr<StringConstant>("hi", module->getStringType());
  ASSERT_TRUE(util::match(first, second));
  first->setVal("bye");
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingAssignInstr) {
  auto *var = module->Nr<Var>(module->getIntType());
  auto *val = module->Nr<IntConstant>(1, module->getIntType());
  auto *first = module->Nr<AssignInstr>(var, val);
  auto *second = module->Nr<AssignInstr>(var->clone(), val->clone());

  ASSERT_TRUE(util::match(first, second));
  second->setRhs(module->Nr<IntConstant>(5, module->getIntType()));
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingExtractInstr) {
  auto FIELD = "foo";
  auto *type = cast<types::RecordType>(module->getMemberedType("**internal**"));
  type->realize({module->getIntType()}, {FIELD});
  auto *var = module->Nr<Var>(type);
  auto *val = module->Nr<VarValue>(var);
  auto *first = module->Nr<ExtractInstr>(val, FIELD);
  auto *second = module->Nr<ExtractInstr>(val->clone(), FIELD);

  ASSERT_TRUE(util::match(first, second));
  second->setField("");
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingInsertInstr) {
  auto FIELD = "foo";
  auto *type = cast<types::RecordType>(module->getMemberedType("**internal**"));
  type->realize({module->getIntType()}, {FIELD});
  auto *var = module->Nr<Var>(type);
  auto *val = module->Nr<VarValue>(var);
  auto *toInsert = module->Nr<IntConstant>(1, module->getIntType());
  auto *first = module->Nr<InsertInstr>(val, FIELD, toInsert);
  auto *second = module->Nr<InsertInstr>(val->clone(), FIELD, toInsert->clone());

  ASSERT_TRUE(util::match(first, second));
  second->setField("");
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingCallInstr) {
  auto *type = module->getDummyFuncType();
  auto *func = module->Nr<BodiedFunc>();
  func->realize(type, {});
  auto *func2 = module->Nr<BodiedFunc>();
  func2->realize(module->getFuncType("baz", module->getIntType(), {}), {});

  auto *funcVal = module->Nr<VarValue>(func);
  auto *first = module->Nr<CallInstr>(funcVal);
  auto *second = module->Nr<CallInstr>(funcVal->clone());

  ASSERT_TRUE(util::match(first, second));
  second->setFunc(module->Nr<VarValue>(func2));
  ASSERT_FALSE(util::match(first, second));
}

TEST_F(SIRCoreTest, MatchingTernaryInstr) {
  auto *trueValue = module->Nr<BoolConstant>(true, module->getBoolType());
  auto *falseValue = module->Nr<BoolConstant>(false, module->getBoolType());
  auto *cond = module->Nr<BoolConstant>(true, module->getBoolType());

  auto *first = module->Nr<TernaryInstr>(cond, trueValue, falseValue);
  auto *second =
      module->Nr<TernaryInstr>(cond->clone(), trueValue->clone(), falseValue->clone());

  ASSERT_TRUE(util::match(first, second));
  second->setFalseValue(module->Nr<BoolConstant>(true, module->getBoolType()));
  ASSERT_FALSE(util::match(first, second));
}
