#include "lang/seq.h"
#include "parser/parser.h"
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

static void versMsg(raw_ostream &out) {
  out << "Seq " << SEQ_VERSION_MAJOR << "." << SEQ_VERSION_MINOR << "."
      << SEQ_VERSION_PATCH << "\n";
}

int main(int argc, char **argv) {
  opt<string> input(Positional, desc("<input file>"), init("-"));
  opt<bool> debug("d", desc("Compile in debug mode"));
  opt<bool> profile("prof", desc("Profile LLVM IR using XRay"));
  opt<bool> docstr("docstr", desc("Generate docstrings"));
  opt<string> output(
      "o", desc("Write LLVM bitcode to specified file instead of running with JIT"));
  cl::list<string> libs("L", desc("Load and link the specified library"));
  cl::list<string> args(ConsumeAfter, desc("<program arguments>..."));

  SetVersionPrinter(versMsg);
  ParseCommandLineOptions(argc, argv);
  vector<string> libsVec(libs);
  vector<string> argsVec(args);

  config::config().debug = debug.getValue();
  config::config().profile = profile.getValue();

  if (docstr.getValue()) {
    generateDocstr(input);
    return EXIT_SUCCESS;
  }

  SeqModule *s = parse(argv[0], input.c_str(), "", false, false);
  if (output.getValue().empty()) {
    argsVec.insert(argsVec.begin(), input);
    execute(s, argsVec, libsVec, debug.getValue());
  } else {
    if (!libsVec.empty())
      compilationWarning("ignoring libraries during compilation");

    if (!argsVec.empty())
      compilationWarning("ignoring arguments during compilation");

    compile(s, output.getValue(), debug.getValue());
  }

  return EXIT_SUCCESS;
}
