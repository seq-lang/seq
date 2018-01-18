#include <iostream>
#include "../src/seq.h"

using namespace seq;

extern "C" bool is_cpg(char *seq, uint32_t len)
{
	return len >= 2 && seq[0] == 'C' && seq[1] == 'G';
}

int main()
{
	Seq s = Seq();

	auto pipeline = s | Split::make(10, 1)
	                  | Filter::make("is_cpg", is_cpg)
	                  | Print::make()
	                  | Substr::make(6, 5)
	                  | Copy::make()
	                  | RevComp::make()
	                  | Split::make(1, 1)
	                  | Print::make();

	s.source("test/seqs.txt");
	s.execute(true);
}
