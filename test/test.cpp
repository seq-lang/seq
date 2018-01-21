#include <iostream>
#include "../src/seq.h"

using namespace seq;
using namespace seq::stageutil;

extern "C" bool is_cpg(char *seq, uint32_t len)
{
	return len >= 2 && seq[0] == 'C' && seq[1] == 'G';
}

int main()
{
	Seq s = Seq();

	auto p1 = split(10, 1) |
	          filter("is_cpg", is_cpg) |
	          print() |
	          substr(6, 5) |
	          copy() |
	          revcomp() |
	          split(1, 1) |
	          print();

	auto p2 = split(32, 32) | print();

	auto p3 = print() | copy() | revcomp() | print();

	s.add(&p1);
	s.add(&p2);
	s.add(&p3);
	s.source("test/seqs.txt");
	s.execute(true);  // debug=true
}
