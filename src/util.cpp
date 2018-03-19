#include <cstdlib>
#include "util.h"

using namespace seq;
using namespace seq::util;

extern "C" void revcomp(char *seq, const seq_int_t len)
{
	const seq_int_t half = (len + 1)/2;
	for (seq_int_t i = 0; i < half; i++) {
		const seq_int_t j = len - i - 1;
		const char a = seq[i];
		const char b = seq[j];

		seq[i] = revcomp_base(b);
		seq[j] = revcomp_base(a);
	}
}
