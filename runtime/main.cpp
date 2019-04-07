#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include "llvm/Support/CommandLine.h"
#include "seq/seq.h"
#include "seq/parser.h"

#define SEQ_PATH_ENV_VAR "SEQ_PATH"

using namespace std;
using namespace seq;
using namespace llvm;
using namespace llvm::cl;

static void errMsg(const string& msg)
{
	cerr << "\033[1;31merror:\033[0m " << msg << endl;
}

static void warnMsg(const string& msg)
{
	cerr << "\033[1;33mwarning:\033[0m " << msg << endl;
}

static void versMsg(raw_ostream& out)
{
	out << "Seq " << SEQ_VERSION_MAJOR << "."
	              << SEQ_VERSION_MINOR << "."
	              << SEQ_VERSION_PATCH << "\n";
}

int main(int argc, char **argv)
{
	opt<string> input(Positional, desc("<input file>"), NumOccurrencesFlag::Optional);
	opt<bool> debug("d", desc("Compile in debug mode (disable optimizations; print LLVM IR to stderr)"));
	opt<string> output("o", desc("Write LLVM bitcode to specified file instead of running with JIT"));
	cl::list<string> libs("L", desc("Load and link the specified library"));
	cl::list<string> args(ConsumeAfter, desc("<program arguments>..."));

	SetVersionPrinter(versMsg);
	ParseCommandLineOptions(argc, argv);
	vector<string> libsVec(libs);
	vector<string> argsVec(args);

	if (input.empty()) {
#if LLVM_VERSION_MAJOR >= 7
		repl();
		return EXIT_SUCCESS;
#else
		errMsg("Seq REPL requires LLVM 7+");
		return EXIT_FAILURE;
#endif
	} else if (FILE *file = fopen(input.c_str(), "r")) {
		fclose(file);
	} else {
		errMsg("could not open '" + input + "' for reading");
		return EXIT_FAILURE;
	}

	// make sure path is set
	if (!getenv(SEQ_PATH_ENV_VAR)) {
		errMsg(SEQ_PATH_ENV_VAR " environment variable is not set");
		return EXIT_FAILURE;
	}

	SeqModule *s = parse(input.c_str());

	if (output.getValue().empty()) {
		execute(s, argsVec, libsVec, debug.getValue());
	} else {
		if (!libsVec.empty())
			warnMsg("ignoring libraries during compilation");

		if (!argsVec.empty())
			warnMsg("ignoring libraries during compilation");

		compile(s, output.getValue(), debug.getValue());
	}

	return EXIT_SUCCESS;
}
