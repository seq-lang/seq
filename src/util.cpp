#include <cstdlib>
#include <cstdint>
#include <cstring>
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

SEQ_FUNC bool eq(const char *seq1,
                 const seq_int_t len1,
                 const char *seq2,
                 const seq_int_t len2)
{
	return len1 == len2 && std::memcmp(seq1, seq2, (size_t)len1) == 0;
}
