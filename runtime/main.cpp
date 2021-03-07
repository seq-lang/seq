#include "parser/parser.h"
#include "sir/llvm/llvisitor.h"
#include "sir/transform/manager.h"
#include "sir/transform/pipeline.h"
#include "util/common.h"
#include "llvm/Support/CommandLine.h"
#include <chrono>
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

enum Mode { Run, Build, Doc };
enum BuildMode { LLVM, Bitcode, Object, Executable, Detect };
enum OptMode { Debug, Release };

llvm::cl::OptionCategory runCat("Run Options", "Applicable in 'run' mode");
llvm::cl::OptionCategory buildCat("Build Options", "Applicable in 'build' mode");
llvm::cl::OptionCategory generalCat("General Options");

llvm::cl::opt<Mode>
    mode(llvm::cl::desc("mode"),
         llvm::cl::values(clEnumValN(Run, "run", "Run with JIT"),
                          clEnumValN(Build, "build",
                                     "Compile to executable, object file or LLVM IR"),
                          clEnumValN(Doc, "doc", "Generate documentation")),
         llvm::cl::init(Run), llvm::cl::cat(generalCat));

llvm::cl::opt<std::string> input(llvm::cl::Positional, llvm::cl::Required,
                                 llvm::cl::desc("<input file>"));

llvm::cl::list<std::string> args(llvm::cl::ConsumeAfter,
                                 llvm::cl::desc("<program arguments>..."),
                                 llvm::cl::cat(runCat));

llvm::cl::opt<BuildMode> buildMode(
    llvm::cl::desc("output type"),
    llvm::cl::values(clEnumValN(LLVM, "llvm", "Generate LLVM IR"),
                     clEnumValN(Bitcode, "bc", "Generate LLVM bitcode"),
                     clEnumValN(Object, "obj", "Generate native object file"),
                     clEnumValN(Executable, "exe", "Generate executable"),
                     clEnumValN(Detect, "detect",
                                "Detect output type based on output file extension")),
    llvm::cl::init(Detect), llvm::cl::cat(buildCat));

llvm::cl::opt<OptMode>
    optMode(llvm::cl::desc("optimization mode"),
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

llvm::cl::opt<std::string> output(
    "o",
    llvm::cl::desc("Write compiled output to specified file. Supported extensions: "
                   ".ll (LLVM IR), .bc (LLVM bitcode), .o (object file)"),
    llvm::cl::cat(buildCat));

llvm::cl::list<std::string> libs("L",
                                 llvm::cl::desc("Load and link the specified library"),
                                 llvm::cl::cat(runCat));
} // namespace

int main(int argc, char **argv) {
  llvm::cl::SetVersionPrinter(versMsg);
  llvm::cl::ParseCommandLineOptions(argc, argv);
  std::vector<std::string> libsVec(libs);
  std::vector<std::string> argsVec(args);

  if (input != "-" && !hasExtension(input, ".seq")) {
    seq::compilationError("input file is expected to be a .seq file, or '-' for stdin");
  }

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

  if (mode == Mode::Doc) {
    // TODO: generate docs for user code
    seq::generateDocstr(argv[0]);
    return EXIT_SUCCESS;
  }

  auto *module = seq::parse(argv[0], input.c_str(), /*code=*/"", /*isCode=*/false,
                            /*isTest=*/false, /*startLine=*/0, defmap);
  if (!module)
    return EXIT_FAILURE;

  const bool debug = (optMode == OptMode::Debug);
  auto t = std::chrono::high_resolution_clock::now();
  seq::ir::transform::PassManager pm;
  registerStandardPasses(pm, debug);
  pm.run(module);
  seq::ir::LLVMVisitor visitor(debug);
  visitor.visit(module);
  LOG_TIME("[T] ir-visitor = {:.1f}",
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::high_resolution_clock::now() - t)
                   .count() /
               1000.0);

  switch (mode.getValue()) {
  case Mode::Run:
    argsVec.insert(argsVec.begin(), input);
    visitor.run(argsVec, libsVec);
    break;
  case Mode::Build: {
    if (output.empty() && input == "-") {
      seq::compilationError("output file must be specified when reading from stdin");
    }

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
    break;
  }
  default:
    assert(0);
  }

  return EXIT_SUCCESS;
}
