#include "test.h"

#include <algorithm>

namespace {
class TestVisitor : public seq::ir::util::IRVisitor {
public:
  void visit(seq::ir::IntConstant *) override { FAIL(); }
  void visit(seq::ir::BoolConstant *) override {}
};

class ConstTestVisitor : public seq::ir::util::ConstIRVisitor {
public:
  void visit(const seq::ir::IntConstant *) override { FAIL(); }
  void visit(const seq::ir::BoolConstant *) override {}
};

} // namespace

using namespace seq::ir;

TEST_F(SIRTest, IRNodeNoReplacementRTTI) {
  auto *derived = module->Nr<IntConstant>(1, module->getIntType());
  ASSERT_TRUE(derived);
  ASSERT_FALSE(derived->hasReplacement());
  auto *base = cast<Value>(derived);
  ASSERT_TRUE(base);
  ASSERT_TRUE(isA<IntConstant>(base));
  ASSERT_TRUE(isA<Constant>(base));
  ASSERT_TRUE(isA<Value>(base));
  ASSERT_FALSE(isA<Flow>(base));

  const auto *constBase = base;
  ASSERT_TRUE(isA<Constant>(constBase));
  ASSERT_TRUE(cast<Constant>(constBase));
}

TEST_F(SIRTest, IRNodeNoReplacementAttributes) {
  auto *node = module->Nr<IntConstant>(1, module->getIntType());
  ASSERT_FALSE(node->hasReplacement());
  ASSERT_FALSE(node->hasAttribute<FuncAttribute>());

  ASSERT_TRUE(node->hasAttribute<SrcInfoAttribute>());
  ASSERT_TRUE(node->getAttribute<SrcInfoAttribute>());
  ASSERT_EQ(1, std::distance(node->attributes_begin(), node->attributes_end()));
}

TEST_F(SIRTest, IRNodeReplacementRTTI) {
  Value *node = module->Nr<IntConstant>(1, module->getIntType());
  ASSERT_TRUE(node);
  ASSERT_FALSE(node->hasReplacement());
  ASSERT_TRUE(isA<IntConstant>(node));

  node->replaceAll(module->Nr<BoolConstant>(false, module->getBoolType()));
  ASSERT_TRUE(node->hasReplacement());
  ASSERT_FALSE(isA<IntConstant>(node));
  ASSERT_TRUE(isA<BoolConstant>(node));
  ASSERT_TRUE(cast<BoolConstant>(node));
}

TEST_F(SIRTest, IRNodeReplacementDelegates) {
  auto NODE_NAME = "foo";

  Value *originalNode = module->Nr<IntConstant>(1, module->getIntType());
  Value *newNode = module->Nr<BoolConstant>(false, module->getBoolType(), NODE_NAME);
  newNode->setAttribute(std::make_unique<FuncAttribute>());

  ASSERT_EQ(0, originalNode->getName().size());
  ASSERT_EQ(1, std::distance(originalNode->attributes_begin(),
                             originalNode->attributes_end()));

  originalNode->replaceAll(newNode);
  ASSERT_EQ(NODE_NAME, originalNode->getName());
  ASSERT_EQ(2, std::distance(originalNode->attributes_begin(),
                             originalNode->attributes_end()));

  TestVisitor v;
  originalNode->accept(v);
  newNode->accept(v);

  ConstTestVisitor v2;
  originalNode->accept(v2);
  newNode->accept(v2);
}

TEST_F(SIRTest, IRNodeNonReplaceableFails) {
  Value *originalNode = module->Nr<IntConstant>(1, module->getIntType());
  originalNode->setReplaceable(false);
  ASSERT_DEATH(originalNode->replaceAll(originalNode), "");
}
