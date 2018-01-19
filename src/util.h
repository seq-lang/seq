#ifndef SEQ_UTIL_H
#define SEQ_UTIL_H

#include <cstdint>

namespace seq {
	namespace util {
		extern "C" bool seq_eq(char *seq1,
		                       uint32_t len1,
		                       char *seq2,
		                       uint32_t len2);
		extern "C" char revcomp_base(char base);
		extern "C" void revcomp(char *seq, uint32_t len);
		extern "C" void dump(char *seq, uint32_t len);
	}
}

#endif /* SEQ_UTIL_H */
