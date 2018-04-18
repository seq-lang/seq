#ifndef SEQ_UTIL_H
#define SEQ_UTIL_H

#include <cstdint>
#include "common.h"

namespace seq {
	namespace util {
		inline int ord(const char base)
		{
			switch (base) {
				case 'A':
				case 'a':
					return 0;
				case 'C':
				case 'c':
					return 1;
				case 'G':
				case 'g':
					return 2;
				case 'T':
				case 't':
					return 3;
				default:
					return 4;
			}
		}

		inline char revcomp_base(const char base)
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

		SEQ_FUNC void revcomp(char *seq, seq_int_t len);

		SEQ_FUNC void append(void **array,
		                     void *elem,
		                     seq_int_t elem_size,
		                     seq_int_t len,
		                     seq_int_t *cap);

		SEQ_FUNC bool eq(const char *seq1,
		                 seq_int_t len1,
		                 const char *seq2,
		                 seq_int_t len2);
	}
}

#endif /* SEQ_UTIL_H */
