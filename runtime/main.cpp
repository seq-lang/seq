#include <iostream>
#include <string>
#include <vector>
#include "llvm/Support/CommandLine.h"
#include "seq/seq.h"
#include "seq/parser.h"

using namespace std;
using namespace seq;
using namespace llvm;
using namespace llvm::cl;

int main(int argc, char **argv)
{
	opt<string> input(Positional, desc("<input file>"), NumOccurrencesFlag::Optional);
	opt<bool> debug("d", desc("Compile in debug mode (disable optimizations; print LLVM IR)"));
	opt<string> output("o", desc("Write LLVM bitcode to specified file instead of running with JIT"));
	cl::list<string> libs("L", desc("Load and link the specified library"));
	cl::list<string> args(ConsumeAfter, desc("<program arguments>..."));

	ParseCommandLineOptions(argc, argv);
	vector<string> libsVec(libs);
	vector<string> argsVec(args);

	if (input.empty()) {
		repl();
		return EXIT_SUCCESS;
	}

	SeqModule *s = parse(input.c_str());

	if (output.getValue().empty()) {
		execute(s, argsVec, libsVec, debug.getValue());
	} else {
		if (!libsVec.empty())
			std::cerr << "warning: ignoring libraries during compilation" << std::endl;

		if (!argsVec.empty())
			std::cerr << "warning: ignoring arguments during compilation" << std::endl;

		compile(s, output.getValue(), debug.getValue());
	}

	return EXIT_SUCCESS;
}
