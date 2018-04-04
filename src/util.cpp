#include <cstdlib>
#include <cstdint>
#include <cassert>
#include "seq/util.h"

using namespace seq;
using namespace seq::util;

SEQ_FUNC void revcomp(char *seq, const seq_int_t len)
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

SEQ_FUNC char *copy(char *seq, const seq_int_t len)
{
	auto *seq2 = (char *)std::malloc(len);
	std::memcpy(seq2, seq, len);
	return seq2;
}

SEQ_FUNC void append(void **array,
                     void *elem,
                     const seq_int_t elem_size,
                     const seq_int_t len,
                     seq_int_t *cap)
{
	if (len >= *cap) {
		*cap = (*cap * 3)/2 + 1;
		*array = std::realloc(*array, (size_t)(*cap * elem_size));
		assert(*array);
	}

	auto *bytes = (char *)(*array);
	std::memcpy(&bytes[len * elem_size], elem, (size_t)elem_size);
}
