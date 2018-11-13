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

struct seq_str_t {
	seq_int_t len;
	char *str;
};

template<typename T = void>
struct seq_arr_t {
	seq_int_t len;
	T *arr;
};

#define SEQ_FLAG_NONE   (0UL)
#define SEQ_FLAG_FASTIO (1UL << 0)

SEQ_FUNC void seq_init(seq_int_t flags);
SEQ_FUNC void seq_assert(bool check, seq_str_t file, seq_int_t line);

SEQ_FUNC void *seq_alloc(size_t n);
SEQ_FUNC void *seq_alloc_atomic(size_t n);
SEQ_FUNC void *seq_realloc(void *p, size_t n);
SEQ_FUNC void seq_register_finalizer(void *p, void (*f)(void *obj, void *data));

SEQ_FUNC void seq_print_int(seq_int_t n);
SEQ_FUNC void seq_print_float(double f);
SEQ_FUNC void seq_print_bool(bool b);
SEQ_FUNC void seq_print_byte(char c);
SEQ_FUNC void seq_print_str(seq_str_t str);
SEQ_FUNC void seq_print_seq(seq_t seq);
SEQ_FUNC void seq_print_ptr(void *p);

SEQ_FUNC void *seq_io_openr(const char *filename);
SEQ_FUNC void *seq_io_openw(const char *filename);
SEQ_FUNC void seq_io_close(void *fp);
SEQ_FUNC void seq_io_read(void *ptr,
                          seq_int_t size,
                          seq_int_t nmemb,
                          void *fp);
SEQ_FUNC void seq_io_write(const void *ptr,
                           seq_int_t size,
                           seq_int_t nmemb,
                           void *fp);

SEQ_FUNC void *seq_source_new();
SEQ_FUNC void seq_source_init(void *state, seq_str_t source);
SEQ_FUNC seq_int_t seq_source_read(void *state);
SEQ_FUNC seq_arr_t<seq_t> seq_source_get(void *state, seq_int_t idx);
SEQ_FUNC seq_t seq_source_get_single(void *state, seq_int_t idx);
SEQ_FUNC void seq_source_dealloc(void *state);

SEQ_FUNC void *seq_raw_new();
SEQ_FUNC void seq_raw_init(void *st, seq_str_t source);
SEQ_FUNC seq_t seq_raw_read(void *st);
SEQ_FUNC void seq_raw_dealloc(void *st);

#endif /* SEQ_LIB_H */
