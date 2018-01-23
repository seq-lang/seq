#include <cstdlib>
#include <iostream>
#include "util.h"

using namespace seq::util;

extern "C" bool seq_eq(char *seq1,
                       const uint32_t len1,
                       char *seq2,
                       const uint32_t len2)
{
	if (len1 != len2)
		return false;

	for (uint32_t i = 0; i < len1; i++) {
		if (seq1[i] != seq2[i])
			return false;
	}

	return true;
}

extern "C" char revcomp_base(const char base)
{
	switch (base) {
		case 'A':
			return 'T';
		case 'C':
			return 'G';
		case 'G':
			return 'C';
		case 'T':
			return 'A';
		case 'a':
			return 'a';
		case 'c':
			return 'c';
		case 'g':
			return 'g';
		case 't':
			return 't';
		default:
			return base;
	}
}

extern "C" void revcomp(char *seq, const uint32_t len)
{
	const uint32_t half = (len + 1)/2;
	for (uint32_t i = 0; i < half; i++) {
		const uint32_t j = len - i - 1;
		const char a = seq[i];
		const char b = seq[j];

		seq[i] = revcomp_base(b);
		seq[j] = revcomp_base(a);
	}
}

extern "C" void print(char *seq, const uint32_t len)
{
	for (uint32_t i = 0; i < len; i++)
		std::cout << seq[i];
	std::cout << std::endl;
}
