#include <algorithm>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <parser/parser.h>
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

vector<string> splitLines(const string &output) {
  vector<string> result;
  string line;
  istringstream stream(output);
  const char delim = '\n';

  while (getline(stream, line, delim))
    result.push_back(line);

  return result;
}

static string findExpectOnLine(const string &line) {
  static const string EXPECT_STR = "# EXPECT: ";
  size_t pos = line.find(EXPECT_STR);
  return pos == string::npos ? "" : line.substr(pos + EXPECT_STR.length());
}

static vector<string> findExpects(const string &filename) {
  ifstream file(filename);

  if (!file.good()) {
    cerr << "error: could not open " << filename << endl;
    exit(EXIT_FAILURE);
  }

  string line;
  vector<string> result;

  while (getline(file, line)) {
    string expect = findExpectOnLine(line);
    if (!expect.empty())
      result.push_back(expect);
  }

  file.close();
  return result;
}

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
  SeqModule *module = parse("", filename.c_str());
  execute(module, {filename}, {}, debug);
  string output = result();
  const bool assertsFailed = output.find("TEST FAILED") != string::npos;
  EXPECT_FALSE(assertsFailed);
  if (assertsFailed)
    std::cerr << output << std::endl;
  vector<string> expects = findExpects(filename);
  if (!expects.empty()) {
    vector<string> results = splitLines(output);
    EXPECT_EQ(results.size(), expects.size());
    if (expects.size() == results.size()) {
      for (unsigned i = 0; i < expects.size(); i++) {
        EXPECT_EQ(results[i], expects[i]);
      }
    }
  }
}

class ParserTestFixture
    : public testing::TestWithParam<tuple<
          const char * /*code*/, bool /*success*/, const char * /*output*/>> {
protected:
  vector<char> buf;
  int out_pipe[2];
  int save;

  ParserTestFixture() : buf(65536), out_pipe(), save() {}

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

TEST_P(ParserTestFixture, Run) {
  string code = get<0>(GetParam());
  bool success = get<1>(GetParam());
  string output = get<2>(GetParam());
  string filename = "<test>";
  try {
    SeqModule *module = parse(filename.c_str(), code.c_str(), true, true);
    execute(module, {filename}, {}, false);
    string seqOutput = result();
    EXPECT_TRUE(success);
    EXPECT_EQ(output, seqOutput);
  } catch (seq::exc::SeqException &e) {
    EXPECT_FALSE(success);
  }
}

vector<tuple<const char *, bool, const char *>> cases{
    {"1", true, "1"},
    {"0xFFFFFFFFFFFFFFFFu", true, ""},
    {"-45.353", true, "-45.353"},
    {"245.e12", true, "245.e12"},
    {"'hai'", true, "hai"},
    {"\"\"\"\nEEE\"\"\"", true, "\nEEE"},
    // {"f'{1} + {2} = {1+2}'", true, "1 + 2 = 3"},
    {"k'ACGT'", true, "ACGT"},
    {"s'ACGT'", true, "ACGT"},
    {"p'ACGT'", true, "ACGT"}};
// INSTANTIATE_TEST_SUITE_P(
//   CppParserTests, ParserTestFixture,
//   testing::ValuesIn(cases)
// );

INSTANTIATE_TEST_SUITE_P(
    CoreTests, SeqTest,
    testing::Combine(testing::Values("core/align.seq", "core/arguments.seq",
                                     "core/arithmetic.seq", "core/big.seq",
                                     "core/bwtsa.seq", "core/containers.seq",
                                     "core/empty.seq", "core/exceptions.seq",
                                     "core/formats.seq",
                                     "core/generators.seq", "core/generics.seq",
                                     "core/helloworld.seq", "core/kmers.seq",
                                     "core/match.seq", "core/proteins.seq",
                                     "core/serialization.seq",
                                     "core/trees.seq"),
                     testing::Values(true, false)),
    getTestNameFromParam);

INSTANTIATE_TEST_SUITE_P(
    PipelineTests, SeqTest,
    testing::Combine(testing::Values("pipeline/parallel.seq",
                                     "pipeline/prefetch.seq",
                                     "pipeline/revcomp_opt.seq",
                                     "pipeline/interalign.seq"),
                     testing::Values(true, false)),
    getTestNameFromParam);

INSTANTIATE_TEST_SUITE_P(
    StdlibTests, SeqTest,
    testing::Combine(
        testing::Values("stdlib/str_test.seq", "stdlib/math_test.seq",
                        "stdlib/itertools_test.seq", "stdlib/bisect_test.seq",
                        "stdlib/sort_test.seq", "stdlib/random_test.seq",
                        "stdlib/heapq_test.seq", "stdlib/statistics_test.seq"),
        testing::Values(true, false)),
    getTestNameFromParam);

// INSTANTIATE_TEST_SUITE_P(
//     PythonTests, SeqTest,
//     testing::Combine(testing::Values("python/pybridge.seq"),
//                      testing::Values(true, false)),
//     getTestNameFromParam);

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
