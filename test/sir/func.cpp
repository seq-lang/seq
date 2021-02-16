#include "test.h"

#include <algorithm>

#include "sir/util/matching.h"

using namespace seq::ir;

TEST_F(SIRTest, FuncRealizationAndVarInsertionEraseAndIterators) {
  auto *fn = module->Nr<BodiedFunc>();
  fn->realize(module->getDummyFuncType(), {});

  auto *fnType = module->getFuncType("**test_type**", module->getIntType(),
                                     {module->getIntType()});
  std::vector<std::string> names = {"foo"};
  fn->realize(cast<types::FuncType>(fnType), names);
  ASSERT_TRUE(fn->isGlobal());

  ASSERT_EQ(1, std::distance(fn->arg_begin(), fn->arg_end()));
  ASSERT_EQ(module->getIntType(), fn->arg_front()->getType());

  auto *var = module->Nr<Var>(module->getIntType(), false, "hi");
  fn->push_back(var);
  ASSERT_EQ(1, std::distance(fn->begin(), fn->end()));
  fn->erase(fn->begin());
  ASSERT_EQ(0, std::distance(fn->begin(), fn->end()));
  fn->insert(fn->begin(), var);
  ASSERT_EQ(1, std::distance(fn->begin(), fn->end()));
  ASSERT_EQ(module->getIntType(), fn->front()->getType());
}

TEST_F(SIRTest, BodiedFuncQueryAndReplace) {
  auto *fn = module->Nr<BodiedFunc>();
  fn->realize(module->getDummyFuncType(), {});
  fn->setBuiltin();
  ASSERT_TRUE(fn->isBuiltin());

  auto *body = fn->getBody();
  ASSERT_FALSE(body);
  ASSERT_EQ(0, fn->getUsedValues().size());

  body = module->Nr<SeriesFlow>();
  fn->setBody(body);
  ASSERT_EQ(body, fn->getBody());

  auto used = fn->getUsedValues();
  ASSERT_EQ(1, used.size());
  ASSERT_EQ(body, used[0]);

  ASSERT_EQ(1, fn->replaceUsedValue(body, module->Nr<SeriesFlow>()));
  ASSERT_DEATH(fn->replaceUsedValue(fn->getBody(), module->Nr<VarValue>(nullptr)), "");
  ASSERT_NE(fn->getBody(), body);
}

TEST_F(SIRTest, BodiedFuncUnmangledName) {
  auto *fn = module->Nr<BodiedFunc>("Int.foo");
  fn->realize(module->getDummyFuncType(), {});
  ASSERT_EQ("foo", fn->getUnmangledName());
}

TEST_F(SIRTest, BodiedFuncCloning) {
  auto *fn = module->Nr<BodiedFunc>("fn");
  fn->realize(module->getDummyFuncType(), {});

  fn->setBuiltin();
  fn->setBody(module->Nr<SeriesFlow>());
  ASSERT_TRUE(util::match(fn, fn->clone()));
}

TEST_F(SIRTest, ExternalFuncUnmangledNameAndCloning) {
  auto *fn = module->Nr<ExternalFunc>("fn");
  fn->realize(module->getDummyFuncType(), {});

  fn->setUnmangledName("foo");
  ASSERT_EQ("foo", fn->getUnmangledName());
  ASSERT_TRUE(util::match(fn, fn->clone()));
}

TEST_F(SIRTest, InternalFuncParentTypeUnmangledNameAndCloning) {
  auto *fn = module->Nr<InternalFunc>("fn.1");
  fn->realize(module->getDummyFuncType(), {});

  fn->setParentType(module->getIntType());
  ASSERT_EQ("fn", fn->getUnmangledName());
  ASSERT_EQ(fn->getParentType(), module->getIntType());
  ASSERT_TRUE(util::match(fn, fn->clone()));
}

TEST_F(SIRTest, LLVMFuncUnmangledNameQueryAndReplace) {
  auto *fn = module->Nr<LLVMFunc>("fn");
  fn->realize(module->getDummyFuncType(), {});

  fn->setLLVMBody("body");
  fn->setLLVMDeclarations("decl");

  std::vector<types::Generic> literals = {types::Generic(1),
                                          types::Generic(module->getIntType())};
  fn->setLLVMLiterals(literals);

  ASSERT_EQ("body", fn->getLLVMBody());
  ASSERT_EQ("decl", fn->getLLVMDeclarations());

  ASSERT_EQ(1, fn->replaceUsedType(module->getIntType(), module->getFloatType()));
  ASSERT_EQ(module->getFloatType(), fn->literal_back().getTypeValue());
}