#include "parser/parser.h"
#include "sir/llvm/llvisitor.h"
#include "sir/transform/manager.h"
#include "sir/transform/pipeline.h"
#include "util/common.h"
#include "llvm/Support/CommandLine.h"
#include <chrono>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
void versMsg(llvm::raw_ostream &out) {
  out << "Seq " << SEQ_VERSION_MAJOR << "." << SEQ_VERSION_MINOR << "."
      << SEQ_VERSION_PATCH << "\n";
}

void registerStandardPasses(seq::ir::transform::PassManager &pm, bool debug) {
  if (debug)
    return;
  pm.registerPass(
      "bio-pipeline-opts",
      std::make_unique<seq::ir::transform::pipeline::PipelineOptimizations>());
}

bool hasExtension(const std::string &filename, const std::string &extension) {
  return filename.size() >= extension.size() &&
         filename.compare(filename.size() - extension.size(), extension.size(),
                          extension) == 0;
}

std::string trimExtension(const std::string &filename, const std::string &extension) {
  if (hasExtension(filename, extension)) {
    return filename.substr(0, filename.size() - extension.size());
  } else {
    return filename;
  }
}

std::string makeOutputFilename(const std::string &filename,
                               const std::string &extension) {
  return trimExtension(filename, ".seq") + extension;
}

enum BuildMode { LLVM, Bitcode, Object, Executable, Detect };
enum OptMode { Debug, Release };

} // namespace

int docMode(const vector<char *> &args) {
  llvm::cl::SetVersionPrinter(versMsg);
  llvm::cl::ParseCommandLineOptions(args.size(), args.data());
  seq::generateDocstr(args[0]);
  return EXIT_SUCCESS;
}

int runMode(const vector<char *> &args) {
  llvm::cl::OptionCategory generalCat("Options");
  llvm::cl::opt<std::string> input(llvm::cl::Positional, llvm::cl::Required,
                                   llvm::cl::desc("<input file>"));
  llvm::cl::opt<OptMode> optMode(
      llvm::cl::desc("optimization mode"),
      llvm::cl::values(
          clEnumValN(Debug, "debug",
                     "Turn off compiler optimizations and show backtraces"),
          clEnumValN(Release, "release",
                     "Turn on compiler optimizations and disable debug info")),
      llvm::cl::init(Debug), llvm::cl::cat(generalCat));
  llvm::cl::list<std::string> defines(
      "D", llvm::cl::Prefix,
      llvm::cl::desc("Add static variable definitions. The syntax is <name>=<value>"),
      llvm::cl::cat(generalCat));
  llvm::cl::list<std::string> seqArgs(llvm::cl::ConsumeAfter,
                                      llvm::cl::desc("<program arguments>..."),
                                      llvm::cl::cat(generalCat));
  llvm::cl::list<std::string> libs(
      "L", llvm::cl::desc("Load and link the specified library"),
      llvm::cl::cat(generalCat));
  llvm::cl::SetVersionPrinter(versMsg);
  llvm::cl::ParseCommandLineOptions(args.size(), args.data());
  std::vector<std::string> libsVec(libs);
  std::vector<std::string> argsVec(seqArgs);

  if (input != "-" && !hasExtension(input, ".seq"))
    seq::compilationError("input file is expected to be a .seq file, or '-' for stdin");

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

  auto *module = seq::parse(args[0], input.c_str(), /*code=*/"", /*isCode=*/false,
                            /*isTest=*/false, /*startLine=*/0, defmap);
  if (!module)
    return EXIT_FAILURE;

  const bool isDebug = (optMode == OptMode::Debug);
  auto t = std::chrono::high_resolution_clock::now();
  seq::ir::transform::PassManager pm;
  registerStandardPasses(pm, isDebug);
  pm.run(module);
  seq::ir::LLVMVisitor visitor(isDebug);
  visitor.visit(module);
  LOG_TIME("[T] ir-visitor = {:.1f}",
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::high_resolution_clock::now() - t)
                   .count() /
               1000.0);

  argsVec.insert(argsVec.begin(), input);
  visitor.run(argsVec, libsVec);
  return EXIT_SUCCESS;
}

int buildMode(const vector<char *> &args) {
  llvm::cl::OptionCategory generalCat("General Options");
  llvm::cl::opt<std::string> input(llvm::cl::Positional, llvm::cl::Required,
                                   llvm::cl::desc("<input file>"));
  llvm::cl::opt<OptMode> optMode(
      llvm::cl::desc("optimization mode"),
      llvm::cl::values(
          clEnumValN(Debug, "debug",
                     "Turn off compiler optimizations and show backtraces"),
          clEnumValN(Release, "release",
                     "Turn on compiler optimizations and disable debug info")),
      llvm::cl::init(Debug), llvm::cl::cat(generalCat));
  llvm::cl::list<std::string> defines(
      "D", llvm::cl::Prefix,
      llvm::cl::desc("Add static variable definitions. The syntax is <name>=<value>"),
      llvm::cl::cat(generalCat));
  llvm::cl::opt<BuildMode> buildMode(
      llvm::cl::desc("output type"),
      llvm::cl::values(clEnumValN(LLVM, "llvm", "Generate LLVM IR"),
                       clEnumValN(Bitcode, "bc", "Generate LLVM bitcode"),
                       clEnumValN(Object, "obj", "Generate native object file"),
                       clEnumValN(Executable, "exe", "Generate executable"),
                       clEnumValN(Detect, "detect",
                                  "Detect output type based on output file extension")),
      llvm::cl::init(Detect), llvm::cl::cat(generalCat));
  llvm::cl::opt<std::string> output(
      "o",
      llvm::cl::desc("Write compiled output to specified file. Supported extensions: "
                     ".ll (LLVM IR), .bc (LLVM bitcode), .o (object file)"),
      llvm::cl::cat(generalCat));
  llvm::cl::SetVersionPrinter(versMsg);
  llvm::cl::ParseCommandLineOptions(args.size(), args.data());

  if (input != "-" && !hasExtension(input, ".seq"))
    seq::compilationError("input file is expected to be a .seq file, or '-' for stdin");

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

  auto *module = seq::parse(args[0], input.c_str(), /*code=*/"", /*isCode=*/false,
                            /*isTest=*/false, /*startLine=*/0, defmap);
  if (!module)
    return EXIT_FAILURE;

  const bool isDebug = (optMode == OptMode::Debug);
  auto t = std::chrono::high_resolution_clock::now();
  seq::ir::transform::PassManager pm;
  registerStandardPasses(pm, isDebug);
  pm.run(module);
  seq::ir::LLVMVisitor visitor(isDebug);
  visitor.visit(module);
  LOG_TIME("[T] ir-visitor = {:.1f}",
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::high_resolution_clock::now() - t)
                   .count() /
               1000.0);

  if (output.empty() && input == "-")
    seq::compilationError("output file must be specified when reading from stdin");
  std::string extension;
  switch (buildMode) {
  case BuildMode::LLVM:
    extension = ".ll";
    break;
  case BuildMode::Bitcode:
    extension = ".bc";
    break;
  case BuildMode::Object:
    extension = ".o";
    break;
  case BuildMode::Executable:
  case BuildMode::Detect:
    extension = "";
    break;
  default:
    assert(0);
  }
  const std::string filename =
      output.empty() ? makeOutputFilename(input, extension) : output;
  switch (buildMode) {
  case BuildMode::LLVM:
    visitor.writeToLLFile(filename);
    break;
  case BuildMode::Bitcode:
    visitor.writeToBitcodeFile(filename);
    break;
  case BuildMode::Object:
    visitor.writeToObjectFile(filename);
    break;
  case BuildMode::Executable:
    visitor.writeToExecutable(filename);
    break;
  case BuildMode::Detect:
    visitor.compile(filename);
    break;
  default:
    assert(0);
  }

  return EXIT_SUCCESS;
}

int otherMode(const vector<char *> &args) {
  llvm::cl::opt<std::string> input(llvm::cl::Positional, llvm::cl::desc("<mode>"));
  llvm::cl::extrahelp("\nMODES:\n\n"
                      "  run   - run a program interactively\n"
                      "  build - build a program\n"
                      "  doc   - generate program documentation\n");
  llvm::cl::SetVersionPrinter(versMsg);
  llvm::cl::ParseCommandLineOptions(args.size(), args.data());

  if (!input.empty())
    seq::compilationError("Available commands: seqc [build|run|doc]");
  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  vector<char *> args{argv[0]};
  for (int i = 2; i < argc; i++)
    args.emplace_back(argv[i]);
  if (argc < 2)
    seq::compilationError("Available commands: seqc [build|run|doc]");
  if (string(argv[1]) == "run")
    return runMode(args);
  if (string(argv[1]) == "build")
    return buildMode(args);
  if (string(argv[1]) == "doc")
    return docMode(args);
  return otherMode(vector<char *>(argv, argv + argc));
}
