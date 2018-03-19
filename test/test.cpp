#include <iostream>
#include "seq.h"

using namespace seq;
using namespace seq::types;
using namespace seq::stageutil;

extern "C" bool is_cpg(char *seq, seq_int_t len)
{
	return len >= 2 && seq[0] == 'C' && seq[1] == 'G';
}

extern "C" seq_int_t my_hash_func(char *seq, seq_int_t len)
{
	seq_int_t h = 0;

	for (seq_int_t i = 0; i < len; i++) {
		h <<= 2;
		switch (seq[i]) {
			case 'A':
			case 'a':
				h += 0;
				break;
			case 'C':
			case 'c':
				h += 1;
				break;
			case 'G':
			case 'g':
				h += 2;
				break;
			case 'T':
			case 't':
				h += 3;
				break;
			default:
				break;
		}
	}

	return h;
}

static Filter& filt_cpg()
{
	return filter("is_cpg", is_cpg);
}

static Hash& my_hash()
{
	return hash("my_hash_func", my_hash_func);
}

int main()
{
	seq::Seq s;

	Var mem = s.once | Int[10];
	s.once | range(10) | mem[_];
	s.once | range(10) | mem[_] | print();

	s | print();

	s.source("test/data/seqs.fastq");
	s.execute(true);  // debug=true
}
