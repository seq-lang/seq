#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <unwind.h>

#if THREADED
#include <omp.h>
#define GC_THREADS
#endif

#include <sys/time.h>
#include <gc.h>
#include "lib.h"

using namespace std;


#include <malloc.h>



static void *my_malloc_hook(size_t size, const void *caller)
{
    return seq_alloc(size);
}

static void *my_realloc_hook(void *ptr, size_t size, const void *caller)
{
    return seq_realloc(ptr, size);
}

static void my_free_hook(void *ptr, const void *caller)
{
}

/*
 * General
 */

void seq_exc_init();

SEQ_FUNC void seq_init()
{
	GC_INIT();
	
#if THREADED
	GC_allow_register_threads();
	
	__malloc_hook = my_malloc_hook;
	__realloc_hook = my_realloc_hook;
	__free_hook = my_free_hook;



	#pragma omp parallel
	{
		GC_stack_base sb;
		GC_get_stack_base(&sb);
		GC_register_my_thread(&sb);
	}
#endif

	seq_exc_init();
}

SEQ_FUNC void seq_assert_failed(seq_str_t file, seq_int_t line)
{
	std::cerr << "assertion failed on line " << line << " (" << file.str << ")" << std::endl;
	exit(EXIT_FAILURE);
}


/*
 * GC
 */

SEQ_FUNC void *seq_alloc(size_t n)
{
	return GC_MALLOC(n);
}

SEQ_FUNC void *seq_alloc_atomic(size_t n)
{
	return GC_MALLOC_ATOMIC(n);
}

SEQ_FUNC void *seq_realloc(void *p, size_t n)
{
	return GC_REALLOC(p, n);
}

SEQ_FUNC void seq_register_finalizer(void *p, void (*f)(void *obj, void *data))
{
	GC_REGISTER_FINALIZER(p, f, nullptr, nullptr, nullptr);
}


/*
 * String conversion
 */

template<typename T>
static seq_str_t string_conv(const char *fmt, const size_t size, T t)
{
	auto *p = (char *)seq_alloc_atomic(size);
	int n = snprintf(p, size, fmt, t);
	if (n >= size) {
		auto n2 = (size_t)n + 1;
		p = (char *)seq_realloc((void *)p, n2);
		n = snprintf(p, n2, fmt, t);
	}
	return {(seq_int_t)n, p};
}

SEQ_FUNC seq_str_t seq_str_int(seq_int_t n)
{
	return string_conv("%ld", 22, n);
}

SEQ_FUNC seq_str_t seq_str_float(double f)
{
	return string_conv("%g", 16, f);
}

SEQ_FUNC seq_str_t seq_str_bool(bool b)
{
	return string_conv("%s", 6, b ? "True" : "False");
}

SEQ_FUNC seq_str_t seq_str_byte(char c)
{
	return string_conv("%c", 5, c);
}

SEQ_FUNC seq_str_t seq_str_ptr(void *p)
{
	return string_conv("%p", 19, p);
}

SEQ_FUNC seq_str_t seq_str_tuple(seq_str_t *strs, seq_int_t n)
{
	size_t total = 2;  // one for each of '(' and ')'
	for (seq_int_t i = 0; i < n; i++) {
		total += strs[i].len;
		if (i < n - 1)
			total += 2;  // ", "
	}

	auto *buf = (char *)seq_alloc_atomic(total);
	size_t where = 0;
	buf[where++] = '(';
	for (seq_int_t i = 0; i < n; i++) {
		seq_str_t str = strs[i];
		auto len = (size_t)str.len;
		memcpy(&buf[where], str.str, len);
		where += len;
		if (i < n - 1) {
			buf[where++] = ',';
			buf[where++] = ' ';
		}
	}
	buf[where] = ')';

	return {(seq_int_t)total, buf};
}


/*
 * General I/O
 */

SEQ_FUNC void seq_print(seq_str_t str)
{
	fwrite(str.str, 1, (size_t)str.len, stdout);
}

SEQ_FUNC void *seq_stdin()
{
	return stdin;
}

SEQ_FUNC void *seq_stdout()
{
	return stdout;
}

SEQ_FUNC void *seq_stderr()
{
	return stderr;
}

SEQ_FUNC int seq_time()
{
	timeval ts;
	gettimeofday(&ts, nullptr);
	auto time_ms = (int)((ts.tv_sec * 1000000 + ts.tv_usec) / 1000);
	return time_ms;
}
