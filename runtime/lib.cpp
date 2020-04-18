#include <array>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <unwind.h>
#include <vector>

#if THREADED
#include <omp.h>
#define GC_THREADS
#endif

#include "ksw2/ksw2.h"
#include "lib.h"
#include <gc.h>
#include <htslib/sam.h>

using namespace std;

/*
 * General
 */

void seq_exc_init();

SEQ_FUNC void seq_init() {
  GC_INIT();
  GC_set_warn_proc(GC_ignore_warn_proc);

#if THREADED
  GC_allow_register_threads();

#pragma omp parallel
  {
    GC_stack_base sb;
    GC_get_stack_base(&sb);
    GC_register_my_thread(&sb);
  }
#endif

  seq_exc_init();
}

SEQ_FUNC seq_int_t seq_pid() { return (seq_int_t)getpid(); }

SEQ_FUNC seq_int_t seq_time() {
  auto duration = chrono::system_clock::now().time_since_epoch();
  seq_int_t nanos =
      chrono::duration_cast<chrono::nanoseconds>(duration).count();
  return nanos;
}

SEQ_FUNC seq_int_t seq_time_monotonic() {
  auto duration = chrono::steady_clock::now().time_since_epoch();
  seq_int_t nanos =
      chrono::duration_cast<chrono::nanoseconds>(duration).count();
  return nanos;
}

extern char **environ;
SEQ_FUNC char **seq_env() { return environ; }

/*
 * GC
 */
#define USE_STANDARD_MALLOC 0

SEQ_FUNC void *seq_alloc(size_t n) {
#if USE_STANDARD_MALLOC
  return malloc(n);
#else
  return GC_MALLOC(n);
#endif
}

SEQ_FUNC void *seq_alloc_atomic(size_t n) {
#if USE_STANDARD_MALLOC
  return malloc(n);
#else
  return GC_MALLOC_ATOMIC(n);
#endif
}

SEQ_FUNC void *seq_realloc(void *p, size_t n) {
#if USE_STANDARD_MALLOC
  return realloc(p, n);
#else
  return GC_REALLOC(p, n);
#endif
}

SEQ_FUNC void seq_free(void *p) {
#if USE_STANDARD_MALLOC
  free(p);
#else
  GC_FREE(p);
#endif
}

SEQ_FUNC void seq_register_finalizer(void *p,
                                     void (*f)(void *obj, void *data)) {
#if !USE_STANDARD_MALLOC
  GC_REGISTER_FINALIZER(p, f, nullptr, nullptr, nullptr);
#endif
}

SEQ_FUNC void seq_gc_add_roots(void *start, void *end) {
#if !USE_STANDARD_MALLOC
  GC_add_roots(start, end);
#endif
}

SEQ_FUNC void seq_gc_remove_roots(void *start, void *end) {
#if !USE_STANDARD_MALLOC
  GC_remove_roots(start, end);
#endif
}

SEQ_FUNC void seq_gc_clear_roots() {
#if !USE_STANDARD_MALLOC
  GC_clear_roots();
#endif
}

SEQ_FUNC void seq_gc_exclude_static_roots(void *start, void *end) {
#if !USE_STANDARD_MALLOC
  GC_exclude_static_roots(start, end);
#endif
}

/*
 * String conversion
 */

template <typename T>
static seq_str_t string_conv(const char *fmt, const size_t size, T t) {
  auto *p = (char *)seq_alloc_atomic(size);
  int n = snprintf(p, size, fmt, t);
  if (n >= size) {
    auto n2 = (size_t)n + 1;
    p = (char *)seq_realloc((void *)p, n2);
    n = snprintf(p, n2, fmt, t);
  }
  return {(seq_int_t)n, p};
}

SEQ_FUNC seq_str_t seq_str_int(seq_int_t n) {
  return string_conv("%ld", 22, n);
}

SEQ_FUNC seq_str_t seq_str_float(double f) { return string_conv("%g", 16, f); }

SEQ_FUNC seq_str_t seq_str_bool(bool b) {
  return string_conv("%s", 6, b ? "True" : "False");
}

SEQ_FUNC seq_str_t seq_str_byte(char c) { return string_conv("%c", 5, c); }

SEQ_FUNC seq_str_t seq_str_ptr(void *p) { return string_conv("%p", 19, p); }

/*
 * General I/O
 */

SEQ_FUNC seq_str_t seq_check_errno() {
  if (errno) {
    string msg = strerror(errno);
    auto *buf = (char *)seq_alloc_atomic(msg.size());
    memcpy(buf, msg.data(), msg.size());
    return {(seq_int_t)msg.size(), buf};
  }
  return {0, nullptr};
}

SEQ_FUNC void seq_print(seq_str_t str) {
  fwrite(str.str, 1, (size_t)str.len, stdout);
}

SEQ_FUNC void *seq_stdin() { return stdin; }

SEQ_FUNC void *seq_stdout() { return stdout; }

SEQ_FUNC void *seq_stderr() { return stderr; }

/*
 * dlopen
 */

static std::unordered_map<std::string, void *> dlopen_handles;

SEQ_FUNC void *seq_get_handle(const char *c) {
  auto it = dlopen_handles.find(std::string(c));
  if (it != dlopen_handles.end())
    return it->second;
  else
    return nullptr;
}

SEQ_FUNC void seq_set_handle(const char *c, void *h) {
  dlopen_handles[std::string(c)] = h;
}

/*
 * Threading
 */

SEQ_FUNC void *seq_lock_new() {
  return (void *)new (seq_alloc_atomic(sizeof(timed_mutex))) timed_mutex();
}

SEQ_FUNC bool seq_lock_acquire(void *lock, bool block, double timeout) {
  auto *m = (timed_mutex *)lock;
  if (timeout < 0.0) {
    if (block) {
      m->lock();
      return true;
    } else {
      return m->try_lock();
    }
  } else {
    return m->try_lock_for(chrono::duration<double>(timeout));
  }
}

SEQ_FUNC void seq_lock_release(void *lock) {
  auto *m = (timed_mutex *)lock;
  m->unlock();
}

SEQ_FUNC void *seq_rlock_new() {
  return (void *)new (seq_alloc_atomic(sizeof(recursive_timed_mutex)))
      recursive_timed_mutex();
}

SEQ_FUNC bool seq_rlock_acquire(void *lock, bool block, double timeout) {
  auto *m = (recursive_timed_mutex *)lock;
  if (timeout < 0.0) {
    if (block) {
      m->lock();
      return true;
    } else {
      return m->try_lock();
    }
  } else {
    return m->try_lock_for(chrono::duration<double>(timeout));
  }
}

SEQ_FUNC void seq_rlock_release(void *lock) {
  auto *m = (recursive_timed_mutex *)lock;
  m->unlock();
}

/*
 * Alignment
 *
 * Adapted from ksw2
 * seq_nt4_table is consistent with k-mer encoding
 */

unsigned char seq_nt4_table[256] = {
    0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 1, 4, 4, 4, 2,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 0, 4, 1, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};

unsigned char seq_nt4_rc_table[5] = {3, 2, 1, 0, 4};

unsigned char seq_aa20_table[256] = {
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 0,  1,  2,  3,  4,  5,  6,  7,  8,  20, 9,
    10, 11, 12, 20, 13, 14, 15, 16, 17, 20, 18, 19, 20, 21, 22, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20};

static void encode(seq_t s, uint8_t *buf) {
  if (s.len >= 0) {
    for (seq_int_t i = 0; i < s.len; i++)
      buf[i] = seq_nt4_table[(int)s.seq[i]];
  } else {
    uint8_t *p1 = &buf[0];
    uint8_t *p2 = &buf[-s.len - 1];
    while (p1 <= p2) {
      uint8_t c1 = seq_nt4_rc_table[seq_nt4_table[(int)*p1]];
      uint8_t c2 = seq_nt4_rc_table[seq_nt4_table[(int)*p2]];
      *p1 = c2;
      *p2 = c1;
      p1 += 1;
      p2 -= 1;
    }
  }
}

static void pencode(seq_t s, unsigned char *buf) {
  for (seq_int_t i = 0; i < s.len; i++)
    buf[i] = seq_aa20_table[(int)s.seq[i]];
}

struct CIGAR {
  uint32_t *value;
  seq_int_t len;
};

struct Alignment {
  CIGAR cigar;
  seq_int_t score;
};

#define ALIGN_ENCODE(enc_func)                                                 \
  uint8_t static_qbuf[128];                                                    \
  uint8_t static_tbuf[128];                                                    \
  const int qlen = abs(query.len);                                             \
  const int tlen = abs(target.len);                                            \
  uint8_t *qbuf = qlen <= sizeof(static_qbuf)                                  \
                      ? &static_qbuf[0]                                        \
                      : (uint8_t *)seq_alloc_atomic(qlen);                     \
  uint8_t *tbuf = tlen <= sizeof(static_tbuf)                                  \
                      ? &static_tbuf[0]                                        \
                      : (uint8_t *)seq_alloc_atomic(tlen);                     \
  (enc_func)(query, qbuf);                                                     \
  (enc_func)(target, tbuf)

#define ALIGN_RELEASE()                                                        \
  if (qbuf != &static_qbuf[0])                                                 \
    seq_free(qbuf);                                                            \
  if (tbuf != &static_tbuf[0])                                                 \
  seq_free(tbuf)

SEQ_FUNC void seq_align(seq_t query, seq_t target, int8_t *mat, int8_t gapo,
                        int8_t gape, seq_int_t bandwidth, seq_int_t zdrop,
                        seq_int_t end_bonus, seq_int_t flags, Alignment *out) {
  ksw_extz_t ez;
  ALIGN_ENCODE(encode);
  ksw_extz2_sse(nullptr, qlen, qbuf, tlen, tbuf, 5, mat, gapo, gape,
                (int)bandwidth, (int)zdrop, end_bonus, (int)flags, &ez);
  ALIGN_RELEASE();
  *out = {{ez.cigar, ez.n_cigar}, ez.score};
}

SEQ_FUNC void seq_align_default(seq_t query, seq_t target, Alignment *out) {
  static const int8_t mat[] = {2,  -4, -4, -4, 0,  -4, 2, -4, -4, 0, -4, -4, 2,
                               -4, 0,  -4, -4, -4, 2,  0, 0,  0,  0, 0,  0};
  ksw_extz_t ez;
  ALIGN_ENCODE(encode);
  ksw_extd2_sse(nullptr, qlen, qbuf, tlen, tbuf, 5, mat, 4, 2, 13, 1, -1, -1,
                /* end_bonus */ 0, 0, &ez);
  ALIGN_RELEASE();
  *out = {{ez.cigar, ez.n_cigar}, ez.score};
}

SEQ_FUNC void seq_align_dual(seq_t query, seq_t target, int8_t *mat,
                             int8_t gapo1, int8_t gape1, int8_t gapo2,
                             int8_t gape2, seq_int_t bandwidth, seq_int_t zdrop,
                             seq_int_t end_bonus, seq_int_t flags,
                             Alignment *out) {
  ksw_extz_t ez;
  ALIGN_ENCODE(encode);
  ksw_extd2_sse(nullptr, qlen, qbuf, tlen, tbuf, 5, mat, gapo1, gape1, gapo2,
                gape2, (int)bandwidth, (int)zdrop, end_bonus, (int)flags, &ez);
  ALIGN_RELEASE();
  *out = {{ez.cigar, ez.n_cigar}, ez.score};
}

SEQ_FUNC void seq_align_splice(seq_t query, seq_t target, int8_t *mat,
                               int8_t gapo1, int8_t gape1, int8_t gapo2,
                               int8_t noncan, seq_int_t zdrop, seq_int_t flags,
                               Alignment *out) {
  ksw_extz_t ez;
  ALIGN_ENCODE(encode);
  ksw_exts2_sse(nullptr, qlen, qbuf, tlen, tbuf, 5, mat, gapo1, gape1, gapo2,
                noncan, (int)zdrop, (int)flags, &ez);
  ALIGN_RELEASE();
  *out = {{ez.cigar, ez.n_cigar}, ez.score};
}

SEQ_FUNC void seq_align_global(seq_t query, seq_t target, int8_t *mat,
                               int8_t gapo, int8_t gape, seq_int_t bandwidth,
                               Alignment *out) {
  int m_cigar = 0;
  int n_cigar = 0;
  uint32_t *cigar = nullptr;
  ALIGN_ENCODE(encode);
  int score = ksw_gg2_sse(nullptr, qlen, qbuf, tlen, tbuf, 5, mat, gapo, gape,
                          (int)bandwidth, &m_cigar, &n_cigar, &cigar);
  ALIGN_RELEASE();
  *out = {{cigar, n_cigar}, score};
}

SEQ_FUNC void seq_palign(seq_t query, seq_t target, int8_t *mat, int8_t gapo,
                         int8_t gape, seq_int_t bandwidth, seq_int_t zdrop,
                         seq_int_t end_bonus, seq_int_t flags, Alignment *out) {
  ksw_extz_t ez;
  ALIGN_ENCODE(pencode);
  ksw_extz2_sse(nullptr, qlen, qbuf, tlen, tbuf, 23, mat, gapo, gape,
                (int)bandwidth, (int)zdrop, end_bonus, (int)flags, &ez);
  ALIGN_RELEASE();
  *out = {{ez.cigar, ez.n_cigar}, ez.score};
}

SEQ_FUNC void seq_palign_default(seq_t query, seq_t target, Alignment *out) {
  // Blosum-62
  static const int8_t mat[] = {
      4,  -2, 0,  -2, -1, -2, 0,  -2, -1, -1, -1, -1, -2, -1, -1, -1, 1,  0,
      0,  -3, 0,  -2, -1, -2, 4,  -3, 4,  1,  -3, -1, 0,  -3, 0,  -4, -3, 3,
      -2, 0,  -1, 0,  -1, -3, -4, -1, -3, 1,  0,  -3, 9,  -3, -4, -2, -3, -3,
      -1, -3, -1, -1, -3, -3, -3, -3, -1, -1, -1, -2, -2, -2, -3, -2, 4,  -3,
      6,  2,  -3, -1, -1, -3, -1, -4, -3, 1,  -1, 0,  -2, 0,  -1, -3, -4, -1,
      -3, 1,  -1, 1,  -4, 2,  5,  -3, -2, 0,  -3, 1,  -3, -2, 0,  -1, 2,  0,
      0,  -1, -2, -3, -1, -2, 4,  -2, -3, -2, -3, -3, 6,  -3, -1, 0,  -3, 0,
      0,  -3, -4, -3, -3, -2, -2, -1, 1,  -1, 3,  -3, 0,  -1, -3, -1, -2, -3,
      6,  -2, -4, -2, -4, -3, 0,  -2, -2, -2, 0,  -2, -3, -2, -1, -3, -2, -2,
      0,  -3, -1, 0,  -1, -2, 8,  -3, -1, -3, -2, 1,  -2, 0,  0,  -1, -2, -3,
      -2, -1, 2,  0,  -1, -3, -1, -3, -3, 0,  -4, -3, 4,  -3, 2,  1,  -3, -3,
      -3, -3, -2, -1, 3,  -3, -1, -1, -3, -1, 0,  -3, -1, 1,  -3, -2, -1, -3,
      5,  -2, -1, 0,  -1, 1,  2,  0,  -1, -2, -3, -1, -2, 1,  -1, -4, -1, -4,
      -3, 0,  -4, -3, 2,  -2, 4,  2,  -3, -3, -2, -2, -2, -1, 1,  -2, -1, -1,
      -3, -1, -3, -1, -3, -2, 0,  -3, -2, 1,  -1, 2,  5,  -2, -2, 0,  -1, -1,
      -1, 1,  -1, -1, -1, -1, -2, 3,  -3, 1,  0,  -3, 0,  1,  -3, 0,  -3, -2,
      6,  -2, 0,  0,  1,  0,  -3, -4, -1, -2, 0,  -1, -2, -3, -1, -1, -4, -2,
      -2, -3, -1, -3, -2, -2, 7,  -1, -2, -1, -1, -2, -4, -2, -3, -1, -1, 0,
      -3, 0,  2,  -3, -2, 0,  -3, 1,  -2, 0,  0,  -1, 5,  1,  0,  -1, -2, -2,
      -1, -1, 3,  -1, -1, -3, -2, 0,  -3, -2, 0,  -3, 2,  -2, -1, 0,  -2, 1,
      5,  -1, -1, -3, -3, -1, -2, 0,  1,  0,  -1, 0,  0,  -2, 0,  -1, -2, 0,
      -2, -1, 1,  -1, 0,  -1, 4,  1,  -2, -3, 0,  -2, 0,  0,  -1, -1, -1, -1,
      -2, -2, -2, -1, -1, -1, -1, 0,  -1, -1, -1, 1,  5,  0,  -2, 0,  -2, -1,
      0,  -3, -1, -3, -2, -1, -3, -3, 3,  -2, 1,  1,  -3, -2, -2, -3, -2, 0,
      4,  -3, -1, -1, -2, -3, -4, -2, -4, -3, 1,  -2, -2, -3, -3, -2, -1, -4,
      -4, -2, -3, -3, -2, -3, 11, -2, 2,  -3, 0,  -1, -2, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -2, -1, -1, 0,  0,  -1, -2, -1, -1, -1, -2, -3, -2,
      -3, -2, 3,  -3, 2,  -1, -2, -1, -1, -2, -3, -1, -2, -2, -2, -1, 2,  -1,
      7,  -2, -1, 1,  -3, 1,  4,  -3, -2, 0,  -3, 1,  -3, -1, 0,  -1, 3,  0,
      0,  -1, -2, -3, -1, -2, 4};
  ksw_extz_t ez;
  ALIGN_ENCODE(pencode);
  ksw_extz2_sse(nullptr, qlen, qbuf, tlen, tbuf, 23, mat, 11, 1, -1, -1,
                /* end_bonus */ 0, 0, &ez);
  ALIGN_RELEASE();
  *out = {{ez.cigar, ez.n_cigar}, ez.score};
}

SEQ_FUNC void seq_palign_dual(seq_t query, seq_t target, int8_t *mat,
                              int8_t gapo1, int8_t gape1, int8_t gapo2,
                              int8_t gape2, seq_int_t bandwidth,
                              seq_int_t zdrop, seq_int_t end_bonus,
                              seq_int_t flags, Alignment *out) {
  ksw_extz_t ez;
  ALIGN_ENCODE(pencode);
  ksw_extd2_sse(nullptr, qlen, qbuf, tlen, tbuf, 23, mat, gapo1, gape1, gapo2,
                gape2, (int)bandwidth, (int)zdrop, end_bonus, (int)flags, &ez);
  ALIGN_RELEASE();
  *out = {{ez.cigar, ez.n_cigar}, ez.score};
}

SEQ_FUNC void seq_palign_global(seq_t query, seq_t target, int8_t *mat,
                                int8_t gapo, int8_t gape, seq_int_t bandwidth,
                                Alignment *out) {
  int m_cigar = 0;
  int n_cigar = 0;
  uint32_t *cigar = nullptr;
  ALIGN_ENCODE(pencode);
  int score = ksw_gg2_sse(nullptr, qlen, qbuf, tlen, tbuf, 23, mat, gapo, gape,
                          (int)bandwidth, &m_cigar, &n_cigar, &cigar);
  ALIGN_RELEASE();
  *out = {{cigar, n_cigar}, score};
}

/*
 * htslib
 */

SEQ_FUNC seq_int_t seq_hts_sam_itr_next(htsFile *htsfp, hts_itr_t *itr,
                                        bam1_t *r) {
  return sam_itr_next(htsfp, itr, r);
}
