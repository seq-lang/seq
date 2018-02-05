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

	s |
	split(10, 1) |
	filter("is_cpg", is_cpg) |
	print() |
	substr(6, 5) |
	copy() |
	revcomp() |
	split(1, 1) |
	print();

	s | split(32, 32) | print();

	s | print() | copy() | revcomp() | print();

	Var a, b, c, d;
	a = s | print();
	b = a | substr(1,5);
	c = b | split(1,1) | print();
	d = b | copy() | revcomp() | print();

	s.source("test/data/seqs.fastq");
	s.execute(true);  // debug=true
}
