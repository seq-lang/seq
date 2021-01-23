#include "lang/seq.h"
#include "parser/parser.h"
#include "sir/llvm/llvisitor.h"
#include "util/jit.h"
#include "llvm/Support/CommandLine.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#define SEQ_PATH_ENV_VAR "SEQ_PATH"

using namespace std;
using namespace seq;
using namespace llvm;
using namespace llvm::cl;

namespace {
void versMsg(raw_ostream &out) {
  out << "Seq " << SEQ_VERSION_MAJOR << "." << SEQ_VERSION_MINOR << "."
      << SEQ_VERSION_PATCH << "\n";
}

bool endsWith(std::string const &query, std::string const &ending) {
  if (query.length() >= ending.length()) {
    return (query.compare(query.length() - ending.length(), ending.length(), ending) ==
            0);
  } else {
    return false;
  }
}

bool isLLVMFilename(const std::string &filename) { return endsWith(filename, ".ll"); }
} // namespace

int main(int argc, char **argv) {
  opt<string> input(Positional, desc("<input file>"), init("-"));
  opt<bool> debug("d", desc("Compile in debug mode"));
  opt<bool> docstr("docstr", desc("Generate docstrings"));
  opt<string> output(
      "o", desc("Write LLVM bitcode to specified file instead of running with JIT"));
  cl::list<string> libs("L", desc("Load and link the specified library"));
  cl::list<string> args(ConsumeAfter, desc("<program arguments>..."));

  SetVersionPrinter(versMsg);
  ParseCommandLineOptions(argc, argv);
  vector<string> libsVec(libs);
  vector<string> argsVec(args);

  if (docstr.getValue()) {
    generateDocstr(argv[0]);
    return EXIT_SUCCESS;
  }

  auto module = parse(argv[0], input.c_str(), "", false, false);
  if (!module)
    return EXIT_FAILURE;

  seq::ir::LLVMVisitor visitor(debug.getValue());
  visitor.visit(module);

  if (output.getValue().empty()) {
    argsVec.insert(argsVec.begin(), input);
    visitor.run(argsVec, libsVec);
  } else {
    if (!libsVec.empty())
      compilationWarning("ignoring libraries during compilation");

    if (!argsVec.empty())
      compilationWarning("ignoring arguments during compilation");

    const std::string filename = output.getValue();
    if (isLLVMFilename(filename)) {
      visitor.dump(filename);
    } else {
      visitor.compile(filename);
    }
  }

  return EXIT_SUCCESS;
}
