#include "test.h"

using namespace seq::ir;

TEST_F(SIRCoreTest, VarQueryMethodsDelegate) {
  Var *original = module->Nr<Var>(module->getIntType());
  Var *replacement = module->Nr<Var>(module->getFloatType());
  original->replaceAll(replacement);

  ASSERT_EQ(module->getFloatType(), original->getType());
  ASSERT_EQ(module->getFloatType(), original->getUsedTypes().back());
}

TEST_F(SIRCoreTest, VarReplaceMethodsDelegate) {
  Var *original = module->Nr<Var>(module->getIntType());
  Var *replacement = module->Nr<Var>(module->getFloatType());
  original->replaceAll(replacement);

  ASSERT_EQ(1, original->replaceUsedType(module->getFloatType(), module->getIntType()));
}