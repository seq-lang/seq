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

#include "codon/compiler/compiler.h"
#include "codon/compiler/error.h"
#include "codon/sir/llvm/llvisitor.h"
#include "codon/sir/transform/manager.h"
#include "codon/sir/transform/pass.h"
#include "codon/sir/util/inlining.h"
#include "codon/sir/util/irtools.h"
#include "codon/sir/util/outlining.h"
#include "codon/util/common.h"

#include "gtest/gtest.h"

using namespace codon;
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

string argv0;
extern "C" void GC_atfork_prepare();
extern "C" void GC_atfork_parent();
extern "C" void GC_atfork_child();

class SeqTest
    : public testing::TestWithParam<tuple<
          string /*filename*/, bool /*debug*/, string /* case name */,
          string /* case code */, int /* case line */, bool /* barebones stdlib */>> {
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
    GC_atfork_prepare();
    assert(pid != -1);

    if (pid == 0) {
      GC_atfork_child();
      dup2(out_pipe[1], STDOUT_FILENO);
      close(out_pipe[0]);
      close(out_pipe[1]);

      auto file = getFilename(get<0>(GetParam()));
      bool debug = get<1>(GetParam());
      auto code = get<3>(GetParam());
      auto startLine = get<4>(GetParam());
      int testFlags = 1 + get<5>(GetParam());

      auto compiler = std::make_unique<Compiler>(
          argv0, debug, /*disabledPasses=*/std::vector<std::string>{}, /*isTest=*/true);
      llvm::handleAllErrors(code.empty()
                                ? compiler->parseFile(file, testFlags)
                                : compiler->parseCode(file, code, startLine, testFlags),
                            [](const error::ParserErrorInfo &e) {
                              for (auto &msg : e) {
                                getLogger().level = 0;
                                printf("%s\n", msg.getMessage().c_str());
                              }
                              fflush(stdout);
                              exit(EXIT_FAILURE);
                            });

      llvm::cantFail(compiler->compile());
      compiler->getLLVMVisitor()->run({file});
      fflush(stdout);
      exit(EXIT_SUCCESS);
    } else {
      GC_atfork_parent();
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
  string normname = basename;
  replace(normname.begin(), normname.end(), '/', '_');
  replace(normname.begin(), normname.end(), '.', '_');
  return normname + (debug ? "_debug" : "");
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

  auto expects = findExpects(!isCase ? getFilename(file) : get<3>(GetParam()), isCase);
  if (WEXITSTATUS(status) != int(expects.second))
    fprintf(stderr, "%s\n", output.c_str());
  ASSERT_EQ(WEXITSTATUS(status), int(expects.second));
  const bool assertsFailed = output.find("TEST FAILED") != string::npos;
  EXPECT_FALSE(assertsFailed);
  if (assertsFailed)
    std::cerr << output << std::endl;

  if (!expects.first.empty()) {
    vector<string> results = splitLines(output);
    for (unsigned i = 0; i < min(results.size(), expects.first.size()); i++)
      if (expects.second)
        EXPECT_EQ(results[i].substr(0, expects.first[i].size()), expects.first[i]);
      else
        EXPECT_EQ(results[i], expects.first[i]);
    EXPECT_EQ(results.size(), expects.first.size());
  }
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(
    CoreTests, SeqTest,
    testing::Combine(
      testing::Values(
        "core/align.codon",
        "core/big.codon",
        "core/bwtsa.codon",
        "core/containers.codon",
        "core/formats.codon",
        "core/kmers.codon",
        "core/match.codon",
        "core/proteins.codon",
        "core/serialization.codon"
      ),
      testing::Values(true, false),
      testing::Values(""),
      testing::Values(""),
      testing::Values(0),
      testing::Values(false)
    ),
    getTestNameFromParam);

INSTANTIATE_TEST_SUITE_P(
    PipelineTests, SeqTest,
    testing::Combine(
      testing::Values(
        "pipeline/canonical_opt.codon",
        "pipeline/interalign.codon",
        "pipeline/prefetch.codon",
        "pipeline/revcomp_opt.codon"
      ),
      testing::Values(false),
      testing::Values(""),
      testing::Values(""),
      testing::Values(0),
      testing::Values(false)
    ),
    getTestNameFromParam);
// clang-format on

int main(int argc, char *argv[]) {
  argv0 = ast::executable_path(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
