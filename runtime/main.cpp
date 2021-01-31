#include "lang/seq.h"
#include "parser/parser.h"
#include "sir/llvm/llvisitor.h"
#include "util/jit.h"
#include "llvm/Support/CommandLine.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
void versMsg(llvm::raw_ostream &out) {
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
  llvm::cl::opt<std::string> input(llvm::cl::Positional, llvm::cl::desc("<input file>"),
                                   llvm::cl::init("-"));
  llvm::cl::opt<bool> debug("d", llvm::cl::desc("Compile in debug mode"));
  llvm::cl::opt<bool> docstr("docstr", llvm::cl::desc("Generate docstrings"));
  llvm::cl::opt<std::string> output(
      "o", llvm::cl::desc(
               "Write LLVM bitcode to specified file instead of running with JIT"));
  llvm::cl::list<std::string> defines(
      "D", llvm::cl::Prefix,
      llvm::cl::desc("Add static variable definitions. The syntax is <name>=<value>"));
  llvm::cl::list<std::string> libs(
      "L", llvm::cl::desc("Load and link the specified library"));
  llvm::cl::list<std::string> args(llvm::cl::ConsumeAfter,
                                   llvm::cl::desc("<program arguments>..."));

  llvm::cl::SetVersionPrinter(versMsg);
  llvm::cl::ParseCommandLineOptions(argc, argv);
  std::vector<std::string> libsVec(libs);
  std::vector<std::string> argsVec(args);

  std::unordered_map<std::string, std::string> defmap;
  for (const auto &define : defines) {
    auto eq = define.find('=');
    if (eq == std::string::npos || !eq) {
      seq::compilationWarning("ignoring malformed definition: " + define);
      continue;
    }

    auto name = define.substr(0, eq);
    auto value = define.substr(eq + 1);

    if (defmap.find(name) != defmap.end()) {
      seq::compilationWarning("ignoring duplicate definition: " + define);
      continue;
    }

    defmap.emplace(name, value);
  }

  if (docstr.getValue()) {
    seq::generateDocstr(argv[0]);
    return EXIT_SUCCESS;
  }

  auto module = seq::parse(argv[0], input.c_str(), /*code=*/"", /*isCode=*/false,
                           /*isTest=*/false, /*startLine=*/0, defmap);
  if (!module)
    return EXIT_FAILURE;
  
  seq::ir::LLVMVisitor visitor(debug.getValue());
  visitor.visit(module);

  if (output.getValue().empty()) {
    argsVec.insert(argsVec.begin(), input);
    visitor.run(argsVec, libsVec);
  } else {
    if (!libsVec.empty())
      seq::compilationWarning("ignoring libraries during compilation");

    if (!argsVec.empty())
      seq::compilationWarning("ignoring arguments during compilation");

    const std::string filename = output.getValue();
    if (isLLVMFilename(filename)) {
      visitor.dump(filename);
    } else {
      visitor.compile(filename);
    }
  }

  return EXIT_SUCCESS;
}
