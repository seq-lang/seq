#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <cassert>
#include "seq/seqgc.h"
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
		*array = seqRealloc(*array, (size_t)(*cap * elem_size));
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

static inline void *seqio_openx(const char *filename, const char *mode)
{
	void *fp = fopen(filename, mode);

	if (!fp) {
		std::cerr << "error: unable to open file '" << filename << "'" << std::endl;
		exit(-1);
	}

	return fp;
}

SEQ_FUNC void *io::io_openr(const char *filename)
{
	return seqio_openx(filename, "rb");
}

SEQ_FUNC void *io::io_openw(const char *filename)
{
	return seqio_openx(filename, "wb");
}

SEQ_FUNC void io::io_close(void *fp)
{
	if (fclose((FILE *)fp) != 0) {
		std::cerr << "error: unable to close file" << std::endl;
		exit(-1);
	}
}


SEQ_FUNC void io::io_read(void *ptr,
                          size_t size,
                          size_t nmemb,
                          void *fp)
{
	if (fread(ptr, (size_t)size, (size_t)nmemb, (FILE *)fp) != nmemb) {
		std::cerr << "error: unable to read from file" << std::endl;
		exit(-1);
	}
}

SEQ_FUNC void io::io_write(const void *ptr,
                           seq_int_t size,
                           seq_int_t nmemb,
                           void *fp)
{
	if (fwrite(ptr, (size_t)size, (size_t)nmemb, (FILE *)fp) != nmemb) {
		std::cerr << "error: unable to write to file" << std::endl;
		exit(-1);
	}
}
