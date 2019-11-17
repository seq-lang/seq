#include <algorithm>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <seq/parser.h>
#include <seq/seq.h>
#include <sstream>
#include <string>
#include <tuple>
#include <unistd.h>
#include <vector>

#include "gtest/gtest.h"

using namespace seq;
using namespace std;

class SeqTest : public testing::TestWithParam<
                    tuple<const char * /*filename*/, bool /*debug*/>> {
protected:
  vector<char> buf;
  int out_pipe[2];
  int save;

  SeqTest() : buf(65536), out_pipe(), save() {}

  void SetUp() override {
    save = dup(STDOUT_FILENO);
    assert(pipe(out_pipe) == 0);
    long flags = fcntl(out_pipe[0], F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(out_pipe[0], F_SETFL, flags);
    dup2(out_pipe[1], STDOUT_FILENO);
    close(out_pipe[1]);
  }

  void TearDown() override { dup2(save, STDOUT_FILENO); }

  string result() {
    fflush(stdout);
    read(out_pipe[0], buf.data(), buf.size() - 1);
    return string(buf.data());
  }
};

static string
getTestNameFromParam(const testing::TestParamInfo<SeqTest::ParamType> &info) {
  const string basename = get<0>(info.param);
  const bool debug = get<1>(info.param);

  // normalize basename
  size_t found1 = basename.find('/');
  size_t found2 = basename.find('.');
  assert(found1 != string::npos);
  assert(found2 != string::npos);
  assert(found2 > found1);
  string normname = basename.substr(found1 + 1, found2 - found1 - 1);

  return normname + (debug ? "_debug" : "");
}

TEST_P(SeqTest, Run) {
  const string basename = get<0>(GetParam());
  const bool debug = get<1>(GetParam());
  string filename = string(TEST_DIR) + "/" + basename;
  SeqModule *module = parse("", filename);
  execute(module, {}, {}, debug);
  string output = result();
  const bool pass = output.find("TEST FAILED") == string::npos;
  EXPECT_TRUE(pass);
  if (!pass)
    std::cerr << output << std::endl;
}

INSTANTIATE_TEST_SUITE_P(
    CoreTests, SeqTest,
    testing::Combine(testing::Values("core/align.seq", "core/arguments.seq",
                                     "core/arithmetic.seq", "core/big.seq",
                                     "core/containers.seq", "core/empty.seq",
                                     "core/exceptions.seq", "core/formats.seq",
                                     "core/generators.seq", "core/generics.seq",
                                     "core/helloworld.seq", "core/kmers.seq",
                                     "core/match.seq", "core/proteins.seq",
                                     "core/pybridge.seq",
                                     "core/serialization.seq",
                                     "core/trees.seq",
                                     // Jordan's tests
                                     "stdlib/str.seq",
                                     "stdlib/math.seq",
                                     "stdlib/random.seq",
                                     "stdlib/itertools.seq",
                                     "stdlib/bisect.seq"),
                     testing::Values(true, false)),
    getTestNameFromParam);

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
