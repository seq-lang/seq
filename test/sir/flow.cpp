#include "test.h"

#include <algorithm>
#include <vector>

#include "sir/util/matching.h"

using namespace seq::ir;

TEST_F(SIRCoreTest, FlowTypeIsVoid) {
  auto *f = module->Nr<SeriesFlow>();
  ASSERT_EQ(module->getVoidType(), f->getType());
}

TEST_F(SIRCoreTest, SeriesFlowInsertionEraseAndIterators) {
  auto FIRST_VALUE = 2;
  auto SECOND_VALUE = 1;

  auto *f = module->Nr<SeriesFlow>();
  f->push_back(module->Nr<IntConstant>(SECOND_VALUE, module->getIntType()));
  f->insert(f->begin(), module->Nr<IntConstant>(FIRST_VALUE, module->getIntType()));

  std::vector<Value *> contents(f->begin(), f->end());

  ASSERT_EQ(2, contents.size());
  ASSERT_EQ(FIRST_VALUE, cast<IntConstant>(contents[0])->getVal());
  ASSERT_EQ(SECOND_VALUE, cast<IntConstant>(contents[1])->getVal());

  f->erase(f->begin());
  ASSERT_EQ(SECOND_VALUE, cast<IntConstant>(*f->begin())->getVal());
}

TEST_F(SIRCoreTest, SeriesFlowQueryAndReplace) {
  auto FIRST_VALUE = 2;
  auto SECOND_VALUE = 1;

  auto *f = module->Nr<SeriesFlow>();
  f->push_back(module->Nr<IntConstant>(SECOND_VALUE, module->getIntType()));
  f->insert(f->begin(), module->Nr<IntConstant>(FIRST_VALUE, module->getIntType()));

  std::vector<Value *> contents(f->begin(), f->end());
  auto used = f->getUsedValues();

  ASSERT_TRUE(std::equal(used.begin(), used.end(), contents.begin(), contents.end()));
  ASSERT_EQ(0, f->getUsedTypes().size());
  ASSERT_EQ(0, f->getUsedVariables().size());

  ASSERT_EQ(1, f->replaceUsedValue(contents[0], contents[1]));
}

TEST_F(SIRCoreTest, SeriesFlowCloning) {
  auto FIRST_VALUE = 2;
  auto SECOND_VALUE = 1;

  auto *f = module->Nr<SeriesFlow>();
  f->push_back(module->Nr<IntConstant>(SECOND_VALUE, module->getIntType()));
  f->insert(f->begin(), module->Nr<IntConstant>(FIRST_VALUE, module->getIntType()));

  ASSERT_TRUE(util::match(f, cv->clone(f)));
}

TEST_F(SIRCoreTest, WhileFlowQueryAndReplace) {
  auto *cond = module->Nr<BoolConstant>(true, module->getBoolType());
  auto *body = module->Nr<SeriesFlow>();
  auto *f = module->Nr<WhileFlow>(cond, body);

  ASSERT_EQ(cond, f->getCond());
  ASSERT_EQ(body, f->getBody());

  std::vector<Value *> usedValues = {cond, body};
  auto queried = f->getUsedValues();
  ASSERT_TRUE(
      std::equal(usedValues.begin(), usedValues.end(), queried.begin(), queried.end()));
  ASSERT_EQ(0, f->getUsedTypes().size());
  ASSERT_EQ(0, f->getUsedVariables().size());

  ASSERT_EQ(1, f->replaceUsedValue(
                   cond, module->Nr<BoolConstant>(true, module->getBoolType())));
  ASSERT_EQ(1, f->replaceUsedValue(body, module->Nr<SeriesFlow>()));
  ASSERT_DEATH(f->replaceUsedValue(
                   f->getBody(), module->Nr<BoolConstant>(true, module->getBoolType())),
               "");
  queried = f->getUsedValues();
  ASSERT_FALSE(
      std::equal(usedValues.begin(), usedValues.end(), queried.begin(), queried.end()));
}

TEST_F(SIRCoreTest, WhileFlowCloning) {
  auto *cond = module->Nr<BoolConstant>(true, module->getBoolType());
  auto *body = module->Nr<SeriesFlow>();
  auto *f = module->Nr<WhileFlow>(cond, body);
  ASSERT_TRUE(util::match(f, cv->clone(f)));
}

TEST_F(SIRCoreTest, ForFlowQueryAndReplace) {
  auto *iter = module->Nr<StringConstant>("hi", module->getStringType());
  auto *body = module->Nr<SeriesFlow>();
  auto *var = module->Nr<Var>(module->getStringType(), false, "x");
  auto *f = module->Nr<ForFlow>(iter, body, var);

  ASSERT_EQ(iter, f->getIter());
  ASSERT_EQ(body, f->getBody());
  ASSERT_EQ(var, f->getVar());

  std::vector<Value *> usedValues = {iter, body};
  auto qVal = f->getUsedValues();
  ASSERT_TRUE(
      std::equal(usedValues.begin(), usedValues.end(), qVal.begin(), qVal.end()));
  ASSERT_EQ(0, f->getUsedTypes().size());

  std::vector<Var *> usedVars = {var};
  auto qVar = f->getUsedVariables();
  ASSERT_TRUE(std::equal(usedVars.begin(), usedVars.end(), qVar.begin(), qVar.end()));

  ASSERT_EQ(1, f->replaceUsedValue(
                   iter, module->Nr<StringConstant>("hi", module->getStringType())));
  ASSERT_EQ(1, f->replaceUsedValue(body, module->Nr<SeriesFlow>()));
  qVal = f->getUsedValues();
  ASSERT_FALSE(
      std::equal(usedValues.begin(), usedValues.end(), qVal.begin(), qVal.end()));

  ASSERT_EQ(1, f->replaceUsedVariable(
                   var, module->Nr<Var>(module->getStringType(), false, "x")));
  ASSERT_NE(var, f->getVar());
}

TEST_F(SIRCoreTest, ForFlowCloning) {
  auto *iter = module->Nr<StringConstant>("hi", module->getStringType());
  auto *body = module->Nr<SeriesFlow>();
  auto *var = module->Nr<Var>(module->getStringType(), false, "x");
  auto *f = module->Nr<ForFlow>(iter, body, var);

  ASSERT_TRUE(util::match(f, cv->clone(f)));
}

TEST_F(SIRCoreTest, IfFlowQueryAndReplace) {
  auto *cond = module->Nr<BoolConstant>(true, module->getBoolType());
  auto *tBody = module->Nr<SeriesFlow>();
  auto *fBody = module->Nr<SeriesFlow>();
  auto *f = module->Nr<IfFlow>(cond, tBody, fBody);

  ASSERT_EQ(cond, f->getCond());
  ASSERT_EQ(tBody, f->getTrueBranch());
  ASSERT_EQ(fBody, f->getFalseBranch());

  std::vector<Value *> usedValues = {cond, tBody, fBody};
  auto qVal = f->getUsedValues();
  ASSERT_TRUE(
      std::equal(usedValues.begin(), usedValues.end(), qVal.begin(), qVal.end()));
  ASSERT_EQ(0, f->getUsedTypes().size());
  ASSERT_EQ(0, f->getUsedVariables().size());

  usedValues.pop_back();
  f->setFalseBranch(nullptr);
  qVal = f->getUsedValues();
  ASSERT_TRUE(
      std::equal(usedValues.begin(), usedValues.end(), qVal.begin(), qVal.end()));
  f->setFalseBranch(fBody);

  ASSERT_EQ(1, f->replaceUsedValue(
                   cond, module->Nr<BoolConstant>(true, module->getBoolType())));
  ASSERT_EQ(1, f->replaceUsedValue(tBody, module->Nr<SeriesFlow>()));
  ASSERT_EQ(1, f->replaceUsedValue(fBody, module->Nr<SeriesFlow>()));

  ASSERT_DEATH(
      f->replaceUsedValue(f->getTrueBranch(),
                          module->Nr<BoolConstant>(true, module->getBoolType())),
      "");
  ASSERT_DEATH(
      f->replaceUsedValue(f->getFalseBranch(),
                          module->Nr<BoolConstant>(true, module->getBoolType())),
      "");

  qVal = f->getUsedValues();
  ASSERT_FALSE(
      std::equal(usedValues.begin(), usedValues.end(), qVal.begin(), qVal.end()));
}

TEST_F(SIRCoreTest, IfFlowCloning) {
  auto *cond = module->Nr<BoolConstant>(true, module->getBoolType());
  auto *tBody = module->Nr<SeriesFlow>();
  auto *fBody = module->Nr<SeriesFlow>();
  auto *f = module->Nr<IfFlow>(cond, tBody, fBody);

  ASSERT_TRUE(util::match(f, cv->clone(f)));
}

TEST_F(SIRCoreTest, TryCatchFlowSingleCatchQueryAndReplace) {
  auto *body = module->Nr<SeriesFlow>();
  auto *finally = module->Nr<SeriesFlow>();
  auto *f = module->Nr<TryCatchFlow>(body, finally);
  auto *handler = module->Nr<SeriesFlow>();
  auto *var = module->Nr<Var>(module->getIntType());

  f->emplace_back(handler, module->getIntType(), var);

  ASSERT_EQ(1, f->replaceUsedVariable(var, module->Nr<Var>(module->getFloatType())));
  ASSERT_EQ(1, f->replaceUsedType(module->getIntType(), module->getFloatType()));
  ASSERT_EQ(1, f->replaceUsedValue(handler, module->Nr<SeriesFlow>()));
}
