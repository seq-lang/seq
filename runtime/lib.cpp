#include <array>
#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
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
#include <sys/time.h>

using namespace std;

/*
 * General
 */

void seq_exc_init();
void seq_py_init();

SEQ_FUNC void seq_init() {
  GC_INIT();

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
  seq_py_init();
}

SEQ_FUNC void seq_assert_failed(seq_str_t file, seq_int_t line) {
  fprintf(stderr, "assertion failed on line %d (%s)\n", (int)line, file.str);
  exit(EXIT_FAILURE);
}

/*
 * GC
 */

SEQ_FUNC void *seq_alloc(size_t n) { return GC_MALLOC(n); }

SEQ_FUNC void *seq_alloc_atomic(size_t n) { return GC_MALLOC_ATOMIC(n); }

SEQ_FUNC void *seq_realloc(void *p, size_t n) { return GC_REALLOC(p, n); }

SEQ_FUNC void seq_free(void *p) { GC_FREE(p); }

SEQ_FUNC void seq_register_finalizer(void *p,
                                     void (*f)(void *obj, void *data)) {
  GC_REGISTER_FINALIZER(p, f, nullptr, nullptr, nullptr);
}

SEQ_FUNC void seq_gc_add_roots(void *start, void *end) {
  GC_add_roots(start, end);
}

SEQ_FUNC void seq_gc_remove_roots(void *start, void *end) {
  GC_remove_roots(start, end);
}

SEQ_FUNC void seq_gc_clear_roots() { GC_clear_roots(); }

SEQ_FUNC void seq_gc_exclude_static_roots(void *start, void *end) {
  GC_exclude_static_roots(start, end);
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

SEQ_FUNC seq_str_t seq_str_tuple(seq_str_t *strs, seq_int_t n) {
  size_t total = 2; // one for each of '(' and ')'
  for (seq_int_t i = 0; i < n; i++) {
    total += strs[i].len;
    if (i < n - 1)
      total += 2; // ", "
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

SEQ_FUNC int seq_time() {
  timeval ts;
  gettimeofday(&ts, nullptr);
  auto time_ms = (int)((ts.tv_sec * 1000000 + ts.tv_usec) / 1000);
  return time_ms;
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
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 2, 4, 4, 4, 1,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 0, 4, 2, 4, 4, 4, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};

unsigned char seq_nt4_rc_table[5] = {3, 2, 1, 0, 4};

static int encode(seq_t s) {
  if (s.len >= 0) {
    for (seq_int_t i = 0; i < s.len; i++)
      s.seq[i] = seq_nt4_table[(int)s.seq[i]];
    return (int)s.len;
  } else {
    char *p1 = &s.seq[0];
    char *p2 = &s.seq[-s.len - 1];
    while (p1 <= p2) {
      char c1 = seq_nt4_rc_table[seq_nt4_table[(int)*p1]];
      char c2 = seq_nt4_rc_table[seq_nt4_table[(int)*p2]];
      *p1 = c2;
      *p2 = c1;
      p1 += 1;
      p2 -= 1;
    }
    return (int)(-s.len);
  }
}

static void decode(seq_t s) {
#define BASES "AGCTN"
  if (s.len >= 0) {
    for (seq_int_t i = 0; i < s.len; i++)
      s.seq[i] = BASES[(int)s.seq[i]];
  } else {
    char *p1 = &s.seq[0];
    char *p2 = &s.seq[-s.len - 1];
    while (p1 <= p2) {
      char c1 = BASES[(int)*p1];
      char c2 = BASES[(int)*p2];
      *p1 = c2;
      *p2 = c1;
      p1 += 1;
      p2 -= 1;
    };
  }
#undef BASES
}

struct CIGAR {
  uint32_t *value;
  seq_int_t len;
};

struct Alignment {
  CIGAR cigar;
  seq_int_t score;
};

SEQ_FUNC void seq_align(seq_t query, seq_t target, int8_t *mat, int8_t gapo,
                        int8_t gape, seq_int_t bandwidth, seq_int_t zdrop,
                        seq_int_t flags, Alignment *out) {
  ksw_extz_t ez;
  const int qlen = encode(query);
  const int tlen = encode(target);
  ksw_extz2_sse(nullptr, qlen, (uint8_t *)query.seq, tlen,
                (uint8_t *)target.seq, 5, mat, gapo, gape, (int)bandwidth,
                (int)zdrop,
                /* end_bonus */ 0, (int)flags, &ez);
  decode(query);
  decode(target);
  *out = {{ez.cigar, ez.n_cigar}, ez.score};
}

SEQ_FUNC void seq_align_default(seq_t query, seq_t target, Alignment *out) {
  static int8_t mat[] = {2,  -4, -4, -4, 0,  -4, 2, -4, -4, 0, -4, -4, 2,
                         -4, 0,  -4, -4, -4, 2,  0, 0,  0,  0, 0,  0};
  ksw_extz_t ez;
  const int qlen = encode(query);
  const int tlen = encode(target);
  ksw_extd2_sse(nullptr, qlen, (uint8_t *)query.seq, tlen,
                (uint8_t *)target.seq, 5, mat, 4, 2, 13, 1, -1, -1,
                /* end_bonus */ 0, 0, &ez);
  decode(query);
  decode(target);
  *out = {{ez.cigar, ez.n_cigar}, ez.score};
}

SEQ_FUNC void seq_align_dual(seq_t query, seq_t target, int8_t *mat,
                             int8_t gapo1, int8_t gape1, int8_t gapo2,
                             int8_t gape2, seq_int_t bandwidth, seq_int_t zdrop,
                             seq_int_t flags, Alignment *out) {
  ksw_extz_t ez;
  const int qlen = encode(query);
  const int tlen = encode(target);
  ksw_extd2_sse(nullptr, qlen, (uint8_t *)query.seq, tlen,
                (uint8_t *)target.seq, 5, mat, gapo1, gape1, gapo2, gape2,
                (int)bandwidth, (int)zdrop,
                /* end_bonus */ 0, (int)flags, &ez);
  decode(query);
  decode(target);
  *out = {{ez.cigar, ez.n_cigar}, ez.score};
}

SEQ_FUNC void seq_align_splice(seq_t query, seq_t target, int8_t *mat,
                               int8_t gapo1, int8_t gape1, int8_t gapo2,
                               int8_t noncan, seq_int_t zdrop, seq_int_t flags,
                               Alignment *out) {
  ksw_extz_t ez;
  const int qlen = encode(query);
  const int tlen = encode(target);
  ksw_exts2_sse(nullptr, qlen, (uint8_t *)query.seq, tlen,
                (uint8_t *)target.seq, 5, mat, gapo1, gape1, gapo2, noncan,
                (int)zdrop, (int)flags, &ez);
  decode(query);
  decode(target);
  *out = {{ez.cigar, ez.n_cigar}, ez.score};
}

SEQ_FUNC void seq_align_global(seq_t query, seq_t target, int8_t *mat,
                               int8_t gapo, int8_t gape, seq_int_t bandwidth,
                               Alignment *out) {
  int m_cigar = 0;
  int n_cigar = 0;
  uint32_t *cigar = nullptr;
  const int qlen = encode(query);
  const int tlen = encode(target);
  int score = ksw_gg2_sse(nullptr, qlen, (uint8_t *)query.seq, tlen,
                          (uint8_t *)target.seq, 5, mat, gapo, gape,
                          (int)bandwidth, &m_cigar, &n_cigar, &cigar);
  decode(query);
  decode(target);
  *out = {{cigar, n_cigar}, score};
}

/*
 * htslib
 */

struct seq_cigar_t {
  uint32_t *value;
  seq_int_t len;
};

struct seq_sam_hdr_target_t {
  seq_str_t name;
  seq_int_t len;
};

SEQ_FUNC seq_int_t seq_hts_sam_itr_next(htsFile *htsfp, hts_itr_t *itr,
                                        bam1_t *r) {
  return sam_itr_next(htsfp, itr, r);
}

SEQ_FUNC seq_str_t seq_hts_get_name(bam1_t *aln) {
  char *name = bam_get_qname(aln);
  const int len = aln->core.l_qname - aln->core.l_extranul - 1;
  auto *buf = (char *)seq_alloc_atomic(len);
  memcpy(buf, name, len);
  return {len, buf};
}

SEQ_FUNC seq_t seq_hts_get_seq(bam1_t *aln) {
  uint8_t *seqi = bam_get_seq(aln);
  const int len = aln->core.l_qseq;
  auto *buf = (char *)seq_alloc_atomic(len);
  for (int i = 0; i < len; i++) {
    buf[i] = seq_nt16_str[bam_seqi(seqi, i)];
  }
  return {len, buf};
}

SEQ_FUNC seq_str_t seq_hts_get_qual(bam1_t *aln) {
  uint8_t *quali = bam_get_qual(aln);
  const int len = aln->core.l_qseq;
  auto *buf = (char *)seq_alloc_atomic(len);
  for (int i = 0; i < len; i++) {
    buf[i] = 33 + quali[i];
  }
  return {len, buf};
}

SEQ_FUNC seq_cigar_t seq_hts_get_cigar(bam1_t *aln) {
  uint32_t *cigar = bam_get_cigar(aln);
  const int len = aln->core.n_cigar;
  auto *buf = (uint32_t *)seq_alloc_atomic(len * sizeof(uint32_t));
  memcpy(buf, cigar, len * sizeof(uint32_t));
  return {buf, len};
}

SEQ_FUNC seq_str_t seq_hts_get_aux(bam1_t *aln) {
  auto *aux = (char *)bam_get_aux(aln);
  const int len = bam_get_l_aux(aln);
  auto *buf = (char *)seq_alloc_atomic(len);
  memcpy(buf, aux, len);
  return {len, buf};
}

SEQ_FUNC uint8_t *seq_hts_aux_get(seq_str_t aux, seq_str_t tag) {
  bam1_t aln = {};
  aln.data = (uint8_t *)aux.str;
  aln.l_data = aln.m_data = aux.len;
  char tag_arr[] = {tag.str[0], tag.str[1]};
  return bam_aux_get(&aln, tag_arr);
}

SEQ_FUNC seq_arr_t<seq_sam_hdr_target_t> seq_hts_get_targets(bam_hdr_t *hdr) {
  const int len = hdr->n_targets;
  auto *arr =
      (seq_sam_hdr_target_t *)seq_alloc(len * sizeof(seq_sam_hdr_target_t));
  for (int i = 0; i < len; i++) {
    const int name_len = strlen(hdr->target_name[i]);
    auto *buf = (char *)seq_alloc_atomic(name_len);
    memcpy(buf, hdr->target_name[i], name_len);
    const int target_len = hdr->target_len[i];
    arr[i] = {{name_len, buf}, target_len};
  }
  return {len, arr};
}
