#include <algorithm>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <tuple>
#include <unistd.h>
#include <vector>

#include "lang/seq.h"
#include "parser/common.h"
#include "parser/parser.h"
#include "gtest/gtest.h"

using namespace seq;
using namespace std;

vector<string> splitLines(const string &output) {
  vector<string> result;
  string line;
  istringstream stream(output);
  const char delim = '\n';

  while (getline(stream, line, delim))
    result.push_back(line);

  return result;
}

static pair<bool, string> findExpectOnLine(const string &line) {
  for (auto EXPECT_STR : vector<pair<bool, string>>{
           {false, "# EXPECT: "}, {false, "#: "}, {true, "#! "}}) {
    size_t pos = line.find(EXPECT_STR.second);
    if (pos != string::npos)
      return {EXPECT_STR.first, line.substr(pos + EXPECT_STR.second.length())};
  }
  return {false, ""};
}

static pair<vector<string>, bool> findExpects(const string &filename, bool isCode) {
  vector<string> result;
  bool isError = false;
  string line;
  if (!isCode) {
    ifstream file(filename);
    if (!file.good()) {
      cerr << "error: could not open " << filename << endl;
      exit(EXIT_FAILURE);
    }

    while (getline(file, line)) {
      auto expect = findExpectOnLine(line);
      if (!expect.second.empty()) {
        result.push_back(expect.second);
        isError |= expect.first;
      }
    }
    file.close();
  } else {
    istringstream file(filename);
    while (getline(file, line)) {
      auto expect = findExpectOnLine(line);
      if (!expect.second.empty()) {
        result.push_back(expect.second);
        isError |= expect.first;
      }
    }
  }
  return {result, isError};
}

class SeqTest : public testing::TestWithParam<
                    tuple<string /*filename*/, bool /*debug*/, string /* case name */,
                          string /* case code */, int /* case line */>> {
  vector<char> buf;
  int out_pipe[2];
  pid_t pid;

public:
  SeqTest() : buf(65536), out_pipe(), pid() {}
  string getFilename(const string &basename) {
    return string(TEST_DIR) + "/" + basename;
  }
  int runInChildProcess() {
    assert(pipe(out_pipe) != -1);
    pid = fork();
    assert(pid != -1);

    if (pid == 0) {
      dup2(out_pipe[1], STDOUT_FILENO);
      close(out_pipe[0]);
      close(out_pipe[1]);

      auto file = getFilename(get<0>(GetParam()));
      auto code = get<3>(GetParam());
      auto startLine = get<4>(GetParam());
      SeqModule *module = parse(ast::executable_path(""), file, code, code.size() > 0,
                                /* isTest */ true, startLine);
      execute(module, {file}, {}, get<1>(GetParam()));
      fflush(stdout);
      exit(EXIT_SUCCESS);
    } else {
      int status = -1;
      close(out_pipe[1]);
      assert(waitpid(pid, &status, 0) == pid);
      read(out_pipe[0], buf.data(), buf.size() - 1);
      close(out_pipe[0]);
      return status;
    }
    return -1;
  }
  string result() { return string(buf.data()); }
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
static string
getTypeTestNameFromParam(const testing::TestParamInfo<SeqTest::ParamType> &info) {
  return getTestNameFromParam(info) + "_" + get<2>(info.param);
}
TEST_P(SeqTest, Run) {
  const string file = get<0>(GetParam());
  int status;
  bool isCase = !get<2>(GetParam()).empty();
  if (!isCase)
    status = runInChildProcess();
  else
    status = runInChildProcess();
  ASSERT_TRUE(WIFEXITED(status));

  string output = result();
  fprintf(stderr, "%s\n", output.c_str());

  auto expects = findExpects(!isCase ? getFilename(file) : get<3>(GetParam()), isCase);
  ASSERT_EQ(WEXITSTATUS(status), int(expects.second));
  const bool assertsFailed = output.find("TEST FAILED") != string::npos;
  EXPECT_FALSE(assertsFailed);
  if (assertsFailed)
    std::cerr << output << std::endl;

  if (!expects.first.empty()) {
    vector<string> results = splitLines(output);
    EXPECT_EQ(results.size(), expects.first.size());
    if (expects.first.size() == results.size()) {
      for (unsigned i = 0; i < expects.first.size(); i++)
        EXPECT_EQ(results[i], expects.first[i]);
    }
  }
}
auto getTypeTests(const vector<string> &files) {
  vector<tuple<string, bool, string, string, int>> cases;
  for (auto &f : files) {
    string l;
    ifstream fin(string(TEST_DIR) + "/" + f);
    string code, testName;
    int test = 0;
    int codeLine = 0;
    int line = 0;
    while (getline(fin, l)) {
      if (l.substr(0, 3) == "#%%") {
        if (line)
          cases.push_back({f, true, to_string(line) + "_" + testName, code, codeLine});
        testName = l.substr(4);
        code = l + "\n";
        codeLine = line;
        test++;
      } else {
        code += l + "\n";
      }
      line++;
    }
    if (line)
      cases.push_back({f, true, to_string(line) + "_" + testName, code, codeLine});
  }
  return cases;
}
INSTANTIATE_TEST_SUITE_P(TypeTests, SeqTest,
                         testing::ValuesIn(getTypeTests({"parser/expressions.seq",
                                                         "parser/statements.seq"})),
                         getTypeTestNameFromParam);

// INSTANTIATE_TEST_SUITE_P(
//     CoreTests, SeqTest,
//     testing::Combine(testing::Values("core/parser.seq", "core/align.seq",
//                                      "core/arguments.seq",
//                                      "core/arithmetic.seq", "core/big.seq",
//                                      "core/bltin.seq", "core/bwtsa.seq",
//                                      "core/containers.seq", "core/empty.seq",
//                                      "core/exceptions.seq", "core/formats.seq",
//                                      "core/generators.seq", "core/generics.seq",
//                                      "core/helloworld.seq", "core/kmers.seq",
//                                      "core/match.seq", "core/proteins.seq",
//                                      "core/range.seq", "core/serialization.seq",
//                                      "core/trees.seq"),
//                      testing::Values(true, false), testing::Values(false)),
//     getTestNameFromParam);

// INSTANTIATE_TEST_SUITE_P(
//     PipelineTests, SeqTest,
//     testing::Combine(testing::Values("pipeline/parallel.seq",
//                                      "pipeline/prefetch.seq",
//                                      "pipeline/revcomp_opt.seq",
//                                      "pipeline/canonical_opt.seq",
//                                      "pipeline/interalign.seq"),
//                      testing::Values(true, false)),
//     getTestNameFromParam);

// INSTANTIATE_TEST_SUITE_P(
//     StdlibTests, SeqTest,
//     testing::Combine(
//         testing::Values("stdlib/str_test.seq", "stdlib/math_test.seq",
//                         "stdlib/itertools_test.seq", "stdlib/bisect_test.seq",
//                         "stdlib/sort_test.seq", "stdlib/random_test.seq",
//                         "stdlib/heapq_test.seq", "stdlib/statistics_test.seq"),
//         testing::Values(true, false)),
//     getTestNameFromParam);

// INSTANTIATE_TEST_SUITE_P(
//     PythonTests, SeqTest,
//     testing::Combine(testing::Values("python/pybridge.seq"),
//                      testing::Values(true, false)),
//     getTestNameFromParam);

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
