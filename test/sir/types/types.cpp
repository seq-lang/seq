#include "test.h"

#include <algorithm>

using namespace seq::ir;

TEST_F(SIRCoreTest, RecordTypeQueryAndReplace) {
  auto MEMBER_NAME = "1";
  auto *type = module->Nr<types::RecordType>(
      "foo", std::vector<types::Type *>{module->getIntType()});

  ASSERT_EQ(module->getIntType(), type->getMemberType(MEMBER_NAME));
  ASSERT_EQ(0, type->getMemberIndex(MEMBER_NAME));

  ASSERT_EQ(1, std::distance(type->begin(), type->end()));
  ASSERT_EQ(module->getIntType(), type->front().getType());

  MEMBER_NAME = "baz";
  type->realize({module->getIntType()}, {MEMBER_NAME});

  ASSERT_TRUE(type->isAtomic());

  ASSERT_EQ(1, type->getUsedTypes().size());
  ASSERT_EQ(module->getIntType(), type->getUsedTypes()[0]);

  ASSERT_EQ(1, type->replaceUsedType(module->getIntType(), module->getFloatType()));
}

TEST_F(SIRCoreTest, RefTypeQueryAndReplace) {
  auto MEMBER_NAME = "1";
  auto *contents = module->Nr<types::RecordType>(
      "foo", std::vector<types::Type *>{module->getIntType()});
  auto *type = module->Nr<types::RefType>("baz", contents);

  ASSERT_EQ(module->getIntType(), type->getMemberType(MEMBER_NAME));
  ASSERT_EQ(0, type->getMemberIndex(MEMBER_NAME));

  ASSERT_EQ(1, std::distance(type->begin(), type->end()));
  ASSERT_EQ(module->getIntType(), type->front().getType());

  MEMBER_NAME = "baz";
  type->realize({module->getIntType()}, {MEMBER_NAME});

  ASSERT_FALSE(type->isAtomic());

  ASSERT_EQ(1, type->getUsedTypes().size());
  ASSERT_EQ(contents, type->getUsedTypes()[0]);

  ASSERT_EQ(1, type->replaceUsedType(contents, contents));
}

TEST_F(SIRCoreTest, FuncTypeQueryAndReplace) {
  auto *type = module->Nr<types::FuncType>(
      "foo", module->getIntType(), std::vector<types::Type *>{module->getFloatType()});

  ASSERT_EQ(1, std::distance(type->begin(), type->end()));
  ASSERT_EQ(module->getFloatType(), type->front());

  ASSERT_EQ(2, type->getUsedTypes().size());
  ASSERT_EQ(1, type->replaceUsedType(module->getIntType(), module->getFloatType()));
  ASSERT_EQ(2, type->replaceUsedType(module->getFloatType(), module->getFloatType()));
}
