#include <iostream>
#include <string>
#include <vector>
#include "llvm/Support/CommandLine.h"
#include "seq/seq.h"

using namespace std;
using namespace seq;
using namespace llvm;
using namespace llvm::cl;

int main(int argc, char **argv)
{
	opt<string> input(Positional, desc("<input file>"), Required);
	cl::list<string> args(ConsumeAfter, desc("<program arguments>..."));
	ParseCommandLineOptions(argc, argv);
	vector<string> argsVec(args);

	SeqModule& s = parse(input.c_str());
	s.execute(argsVec);
}
