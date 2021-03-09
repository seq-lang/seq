#include "test.h"

#include <sstream>

#include "util/fmt/format.h"

using namespace seq::ir;

TEST_F(SIRCoreTest, ConstantTypeQueryAndReplace) {
  auto *node = module->Nr<IntConstant>(1, module->getIntType());
  ASSERT_EQ(module->getIntType(), node->getType());

  auto usedTypes = node->getUsedTypes();
  ASSERT_EQ(1, usedTypes.size());
  ASSERT_EQ(module->getIntType(), usedTypes[0]);
  ASSERT_EQ(1, node->replaceUsedType(module->getIntType(), module->getIntType()));
}

TEST_F(SIRCoreTest, ConstantValueMatches) {
  auto VALUE = 1;

  auto *node = module->Nr<IntConstant>(VALUE, module->getIntType());
  ASSERT_EQ(VALUE, node->getVal());

  std::stringstream s;
  s << *node;
  ASSERT_EQ(std::to_string(VALUE), s.str());
}

TEST_F(SIRCoreTest, ConstantCloning) {
  auto VALUE = 1;
  auto *node = module->Nr<IntConstant>(VALUE, module->getIntType());
  auto *clone = cast<IntConstant>(cv->clone(node));

  ASSERT_TRUE(clone);
  ASSERT_EQ(VALUE, clone->getVal());
  ASSERT_EQ(module->getIntType(), clone->getType());
}

TEST_F(SIRCoreTest, StringConstantFormatting) {
  auto VALUE = "hi";

  auto *node = module->Nr<StringConstant>(VALUE, module->getStringType());

  std::stringstream s;
  s << *node;
  ASSERT_EQ(fmt::format(FMT_STRING("\"{}\""), VALUE), s.str());
}
