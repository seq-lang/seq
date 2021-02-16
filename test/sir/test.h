#include <algorithm>

#include "sir/sir.h"
#include "gtest/gtest.h"

class SIRCoreTest : public testing::Test {
protected:
  std::unique_ptr<seq::ir::IRModule> module;

  void SetUp() override {
    seq::ir::IdMixin::resetId();
    module = std::make_unique<seq::ir::IRModule>("test");
  }
};
