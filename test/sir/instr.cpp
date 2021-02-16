#include "test.h"

#include "sir/util/matching.h"

using namespace seq::ir;

TEST_F(SIRCoreTest, AssignInstrQueryAndReplace) {
  auto *var = module->Nr<Var>(module->getIntType());
  auto *val = module->Nr<IntConstant>(1, module->getIntType());
  auto *instr = module->Nr<AssignInstr>(var, val);

  ASSERT_EQ(var, instr->getLhs());
  ASSERT_EQ(val, instr->getRhs());
  ASSERT_EQ(module->getVoidType(), instr->getType());

  auto usedVals = instr->getUsedValues();
  ASSERT_EQ(1, usedVals.size());
  ASSERT_EQ(val, usedVals[0]);

  auto usedVars = instr->getUsedVariables();
  ASSERT_EQ(1, usedVars.size());
  ASSERT_EQ(var, usedVars[0]);

  ASSERT_EQ(1, instr->replaceUsedValue(
                   val, module->Nr<IntConstant>(1, module->getIntType())));
  ASSERT_EQ(1, instr->replaceUsedVariable(var, module->Nr<Var>(module->getIntType())));
}

TEST_F(SIRCoreTest, AssignInstrCloning) {
  auto *var = module->Nr<Var>(module->getIntType());
  auto *val = module->Nr<IntConstant>(1, module->getIntType());
  auto *instr = module->Nr<AssignInstr>(var, val);

  ASSERT_TRUE(util::match(instr, instr->clone()));
}

TEST_F(SIRCoreTest, ExtractInstrQueryAndReplace) {
  auto FIELD = "foo";
  auto *type = cast<types::RecordType>(module->getMemberedType("**internal**"));
  type->realize({module->getIntType()}, {FIELD});
  auto *var = module->Nr<Var>(type);
  auto *val = module->Nr<VarValue>(var);
  auto *instr = module->Nr<ExtractInstr>(val, FIELD);

  ASSERT_EQ(FIELD, instr->getField());
  ASSERT_EQ(val, instr->getVal());
  ASSERT_EQ(type->getMemberType(FIELD), instr->getType());

  auto usedVals = instr->getUsedValues();
  ASSERT_EQ(1, usedVals.size());
  ASSERT_EQ(val, usedVals[0]);

  ASSERT_EQ(1, instr->replaceUsedValue(val, module->Nr<VarValue>(var)));
}

TEST_F(SIRCoreTest, ExtractInstrCloning) {
  auto FIELD = "foo";
  auto *type = cast<types::RecordType>(module->getMemberedType("**internal**"));
  type->realize({module->getIntType()}, {FIELD});
  auto *var = module->Nr<Var>(type);
  auto *val = module->Nr<VarValue>(var);
  auto *instr = module->Nr<ExtractInstr>(val, FIELD);

  ASSERT_TRUE(util::match(instr, instr->clone()));
}

TEST_F(SIRCoreTest, InsertInstrQueryAndReplace) {
  auto FIELD = "foo";
  auto *type = cast<types::RefType>(module->getMemberedType("**internal**", true));
  type->realize({module->getIntType()}, {FIELD});
  auto *var = module->Nr<Var>(type);
  auto *lhs = module->Nr<VarValue>(var);
  auto *rhs = module->Nr<IntConstant>(1, module->getIntType());
  auto *instr = module->Nr<InsertInstr>(lhs, FIELD, rhs);

  ASSERT_EQ(type, instr->getType());

  auto usedVals = instr->getUsedValues();
  ASSERT_EQ(2, usedVals.size());

  ASSERT_EQ(1, instr->replaceUsedValue(lhs, module->Nr<VarValue>(var)));
  ASSERT_EQ(1, instr->replaceUsedValue(
                   rhs, module->Nr<IntConstant>(1, module->getIntType())));
}

TEST_F(SIRCoreTest, InsertInstrCloning) {
  auto FIELD = "foo";
  auto *type = cast<types::RefType>(module->getMemberedType("**internal**", true));
  type->realize({module->getIntType()}, {FIELD});
  auto *var = module->Nr<Var>(type);
  auto *lhs = module->Nr<VarValue>(var);
  auto *rhs = module->Nr<IntConstant>(1, module->getIntType());
  auto *instr = module->Nr<InsertInstr>(lhs, FIELD, rhs);

  ASSERT_TRUE(util::match(instr, instr->clone()));
}

TEST_F(SIRCoreTest, CallInstrQueryAndReplace) {
  auto *type = cast<types::FuncType>(module->getDummyFuncType());
  auto *func = module->Nr<BodiedFunc>();
  func->realize(type, {});
  auto *funcVal = module->Nr<VarValue>(func);
  auto *instr = module->Nr<CallInstr>(funcVal);

  ASSERT_EQ(funcVal, instr->getFunc());
  ASSERT_EQ(type->getReturnType(), instr->getType());

  auto usedVals = instr->getUsedValues();
  ASSERT_EQ(1, usedVals.size());
  ASSERT_EQ(1, instr->replaceUsedValue(funcVal, module->Nr<VarValue>(func)));
}

TEST_F(SIRCoreTest, CallInstrCloning) {
  auto *type = module->getDummyFuncType();
  auto *func = module->Nr<BodiedFunc>();
  func->realize(type, {});
  auto *funcVal = module->Nr<VarValue>(func);
  auto *instr = module->Nr<CallInstr>(funcVal);

  ASSERT_TRUE(util::match(instr, instr->clone()));
}

TEST_F(SIRCoreTest, StackAllocInstrQueryAndReplace) {
  auto COUNT = 1;

  auto *arrayType = module->getArrayType(module->getIntType());
  auto *instr = module->Nr<StackAllocInstr>(arrayType, COUNT);

  ASSERT_EQ(COUNT, instr->getCount());
  ASSERT_EQ(arrayType, instr->getType());

  auto usedTypes = instr->getUsedTypes();
  ASSERT_EQ(1, usedTypes.size());
  ASSERT_EQ(arrayType, usedTypes[0]);

  ASSERT_EQ(1, instr->replaceUsedType(arrayType,
                                      module->getArrayType(module->getFloatType())));
}

TEST_F(SIRCoreTest, StackAllocInstrCloning) {
  auto COUNT = 1;
  auto *arrayType = module->getArrayType(module->getIntType());
  auto *instr = module->Nr<StackAllocInstr>(arrayType, COUNT);
  ASSERT_TRUE(util::match(instr, instr->clone()));
}

TEST_F(SIRCoreTest, TypePropertyInstrQueryAndReplace) {
  auto *type = module->getArrayType(module->getIntType());
  auto *instr =
      module->Nr<TypePropertyInstr>(type, TypePropertyInstr::Property::IS_ATOMIC);

  ASSERT_EQ(type, instr->getInspectType());
  ASSERT_EQ(TypePropertyInstr::Property::IS_ATOMIC, instr->getProperty());
  ASSERT_EQ(module->getBoolType(), instr->getType());

  instr->setProperty(TypePropertyInstr::Property::SIZEOF);
  ASSERT_EQ(module->getIntType(), instr->getType());

  auto usedTypes = instr->getUsedTypes();
  ASSERT_EQ(1, usedTypes.size());
  ASSERT_EQ(type, usedTypes[0]);

  ASSERT_EQ(1,
            instr->replaceUsedType(type, module->getArrayType(module->getFloatType())));
}

TEST_F(SIRCoreTest, TypePropertyInstrCloning) {
  auto *type = module->getArrayType(module->getIntType());
  auto *instr =
      module->Nr<TypePropertyInstr>(type, TypePropertyInstr::Property::IS_ATOMIC);
  ASSERT_TRUE(util::match(instr, instr->clone()));
}

TEST_F(SIRCoreTest, YieldInInstrQueryAndReplace) {
  auto *type = module->getArrayType(module->getIntType());
  auto *instr = module->Nr<YieldInInstr>(type);

  ASSERT_EQ(type, instr->getType());

  auto usedTypes = instr->getUsedTypes();
  ASSERT_EQ(1, usedTypes.size());
  ASSERT_EQ(type, usedTypes[0]);

  ASSERT_EQ(1,
            instr->replaceUsedType(type, module->getArrayType(module->getFloatType())));
}

TEST_F(SIRCoreTest, YieldInInstrCloning) {
  auto *type = module->getArrayType(module->getIntType());
  auto *instr = module->Nr<YieldInInstr>(type);
  ASSERT_TRUE(util::match(instr, instr->clone()));
}

TEST_F(SIRCoreTest, TernaryInstrQueryAndReplace) {
  auto *trueValue = module->Nr<BoolConstant>(true, module->getBoolType());
  auto *falseValue = module->Nr<BoolConstant>(false, module->getBoolType());
  auto *cond = module->Nr<BoolConstant>(true, module->getBoolType());
  auto *instr = module->Nr<TernaryInstr>(cond, trueValue, falseValue);

  ASSERT_EQ(trueValue, instr->getTrueValue());
  ASSERT_EQ(falseValue, instr->getFalseValue());
  ASSERT_EQ(cond, instr->getCond());

  ASSERT_EQ(3, instr->getUsedValues().size());

  ASSERT_EQ(1, instr->replaceUsedValue(
                   cond, module->Nr<BoolConstant>(true, module->getBoolType())));
  ASSERT_EQ(1, instr->replaceUsedValue(
                   trueValue, module->Nr<BoolConstant>(true, module->getBoolType())));
  ASSERT_EQ(1, instr->replaceUsedValue(
                   falseValue, module->Nr<BoolConstant>(true, module->getBoolType())));
}

TEST_F(SIRCoreTest, TernaryInstrCloning) {
  auto *trueValue = module->Nr<BoolConstant>(true, module->getBoolType());
  auto *falseValue = module->Nr<BoolConstant>(false, module->getBoolType());
  auto *cond = module->Nr<BoolConstant>(true, module->getBoolType());
  auto *instr = module->Nr<TernaryInstr>(cond, trueValue, falseValue);

  ASSERT_TRUE(util::match(instr, instr->clone()));
}

TEST_F(SIRCoreTest, ContinueInstrQueryReplaceAndCloning) {
  auto *dst = module->Nr<SeriesFlow>();
  auto *instr = module->Nr<ContinueInstr>(dst);

  ASSERT_EQ(dst, instr->getTarget());

  ASSERT_EQ(1, instr->getUsedValues().size());
  ASSERT_EQ(1, instr->replaceUsedValue(dst, module->Nr<SeriesFlow>()));

  ASSERT_TRUE(util::match(instr, instr->clone()));
}

TEST_F(SIRCoreTest, BreakInstrQueryReplaceAndCloning) {
  auto *dst = module->Nr<SeriesFlow>();
  auto *instr = module->Nr<BreakInstr>(dst);

  ASSERT_EQ(dst, instr->getTarget());

  ASSERT_EQ(1, instr->getUsedValues().size());
  ASSERT_EQ(1, instr->replaceUsedValue(dst, module->Nr<SeriesFlow>()));

  ASSERT_TRUE(util::match(instr, instr->clone()));
}

TEST_F(SIRCoreTest, ReturnInstrQueryReplaceAndCloning) {
  auto *val = module->Nr<IntConstant>(1, module->getIntType());
  auto *instr = module->Nr<ReturnInstr>(val);
  ASSERT_EQ(val, instr->getValue());

  ASSERT_EQ(1, instr->getUsedValues().size());
  ASSERT_EQ(1, instr->replaceUsedValue(val, nullptr));
  ASSERT_EQ(0, instr->getUsedValues().size());

  ASSERT_TRUE(util::match(instr, instr->clone()));
}

TEST_F(SIRCoreTest, YieldInstrQueryReplaceAndCloning) {
  auto *val = module->Nr<IntConstant>(1, module->getIntType());
  auto *instr = module->Nr<YieldInstr>(val);
  ASSERT_EQ(val, instr->getValue());

  ASSERT_EQ(1, instr->getUsedValues().size());
  ASSERT_EQ(1, instr->replaceUsedValue(val, nullptr));
  ASSERT_EQ(0, instr->getUsedValues().size());

  ASSERT_TRUE(util::match(instr, instr->clone()));
}

TEST_F(SIRCoreTest, ThrowInstrQueryReplaceAndCloning) {
  auto *val = module->Nr<IntConstant>(1, module->getIntType());
  auto *instr = module->Nr<ThrowInstr>(val);
  ASSERT_EQ(val, instr->getValue());

  ASSERT_EQ(1, instr->getUsedValues().size());
  ASSERT_EQ(1, instr->replaceUsedValue(val, nullptr));
  ASSERT_EQ(0, instr->getUsedValues().size());

  ASSERT_TRUE(util::match(instr, instr->clone()));
}

TEST_F(SIRCoreTest, FlowInstrQueryAndReplace) {
  auto *flow = module->Nr<SeriesFlow>();
  auto *val = module->Nr<IntConstant>(1, module->getIntType());
  auto *instr = module->Nr<FlowInstr>(flow, val);

  ASSERT_EQ(module->getIntType(), instr->getType());
  ASSERT_EQ(val, instr->getValue());
  ASSERT_EQ(flow, instr->getFlow());

  ASSERT_EQ(2, instr->getUsedValues().size());
  ASSERT_EQ(1, instr->replaceUsedValue(
                   val, module->Nr<IntConstant>(2, module->getIntType())));
  ASSERT_EQ(1, instr->replaceUsedValue(flow, module->Nr<SeriesFlow>()));
}

TEST_F(SIRCoreTest, FlowInstrCloning) {
  auto *flow = module->Nr<SeriesFlow>();
  auto *val = module->Nr<IntConstant>(1, module->getIntType());
  auto *instr = module->Nr<FlowInstr>(flow, val);
  ASSERT_TRUE(util::match(instr, instr->clone()));
}
