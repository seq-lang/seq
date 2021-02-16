#include <algorithm>

#include "sir/sir.h"
#include "gtest/gtest.h"

using namespace seq::ir;

class SIRTest : public testing::Test {
protected:
  std::unique_ptr<IRModule> module;

  void SetUp() override {
    IdMixin::resetId();
    module = std::make_unique<IRModule>("test");
  }
};

TEST_F(SIRTest, NodeBuildingAndRemoval) {
  {
    auto n1 = module->Nr<types::OptionalType>(module->getIntType());
    ASSERT_EQ(n1->getModule(), module.get());
    auto numTypes = std::distance(module->types_begin(), module->types_end());
    ASSERT_TRUE(std::find(module->types_begin(), module->types_end(), n1) !=
                module->types_end());
    module->remove(n1);
    ASSERT_EQ(numTypes - 1, std::distance(module->types_begin(), module->types_end()));
  }
  {
    auto n1 = module->N<IntConstant>(seq::SrcInfo{}, 1, module->getIntType());
    ASSERT_EQ(n1->getModule(), module.get());
    auto numVals = std::distance(module->values_begin(), module->values_end());
    ASSERT_TRUE(std::find(module->values_begin(), module->values_end(), n1) !=
                module->values_end());
    module->remove(n1);
    ASSERT_EQ(numVals - 1, std::distance(module->values_begin(), module->values_end()));
  }
  {
    auto n1 = module->Nr<Var>(module->getIntType());
    ASSERT_EQ(n1->getModule(), module.get());
    auto numVars = std::distance(module->begin(), module->end());
    ASSERT_TRUE(std::find(module->begin(), module->end(), n1) != module->end());
    module->remove(n1);
    ASSERT_EQ(numVars - 1, std::distance(module->begin(), module->end()));
  }
}
