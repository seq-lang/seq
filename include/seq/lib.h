#ifndef SEQ_LIB_H
#define SEQ_LIB_H

#include <cstdlib>
#include <cstddef>
#include <cstdint>

#define SEQ_FUNC extern "C"

typedef int64_t seq_int_t;

struct seq_t {
	seq_int_t len;
	char *seq;
};

template<typename T = void>
struct seq_arr_t {
	seq_int_t len;
	T *arr;
};

SEQ_FUNC void seq_init();

SEQ_FUNC void *seq_alloc(size_t n);
SEQ_FUNC void *seq_alloc_atomic(size_t n);
SEQ_FUNC void *seq_realloc(void *p, size_t n);

SEQ_FUNC char *seq_copy_seq(char *seq, seq_int_t len);
SEQ_FUNC void *seq_copy_array(void *arr, seq_int_t len, seq_int_t elem_size, bool atomic);

SEQ_FUNC void seq_print_int(seq_int_t n);
SEQ_FUNC void seq_print_float(double f);
SEQ_FUNC void seq_print_bool(bool b);
SEQ_FUNC void seq_print_seq(char *seq, seq_int_t len);

SEQ_FUNC void *seq_io_openr(const char *filename);
SEQ_FUNC void *seq_io_openw(const char *filename);
SEQ_FUNC void seq_io_close(void *fp);
SEQ_FUNC void seq_io_read(void *ptr,
                          size_t size,
                          size_t nmemb,
                          void *fp);
SEQ_FUNC void seq_io_write(const void *ptr,
                           seq_int_t size,
                           seq_int_t nmemb,
                           void *fp);

SEQ_FUNC void *seq_source_init(char **sources, seq_int_t numSources);
SEQ_FUNC seq_int_t seq_source_read(void *state);
SEQ_FUNC seq_arr_t<seq_t> seq_source_get(void *state, seq_int_t idx);
SEQ_FUNC seq_t seq_source_get_single(void *state, seq_int_t idx);
SEQ_FUNC void seq_source_dealloc(void *state);

#endif /* SEQ_LIB_H */
