#include "seq.h"
#include "sw/ksw2.h"

/*
 * Alignment
 *
 * Adapted from ksw2
 * seq_nt4_table is consistent with k-mer encoding
 */

unsigned char seq_nt4_table[256] = {
    0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 1, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 1, 4, 4, 4, 2,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};

unsigned char seq_aa20_table[256] = {
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 0,  1,  2,  3,  4,  5,  6,  7,  8,  20, 9,  10, 11, 12, 20,
    13, 14, 15, 16, 17, 20, 18, 19, 20, 21, 22, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20};

static void encode(seq_t s, uint8_t *buf) {
  if (s.len >= 0) {
    for (seq_int_t i = 0; i < s.len; i++)
      buf[i] = seq_nt4_table[(int)s.seq[i]];
  } else {
    seq_int_t n = -s.len;
    for (seq_int_t i = 0; i < n; i++) {
      int c = seq_nt4_table[(int)s.seq[n - 1 - i]];
      buf[i] = (c < 4) ? (3 - c) : c;
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

#define ALIGN_ENCODE(enc_func)                                                         \
  uint8_t static_qbuf[128];                                                            \
  uint8_t static_tbuf[128];                                                            \
  const int qlen = query.len > 0 ? query.len : -query.len;                             \
  const int tlen = target.len > 0 ? target.len : -target.len;                          \
  uint8_t *qbuf = qlen <= sizeof(static_qbuf) ? &static_qbuf[0]                        \
                                              : (uint8_t *)seq_alloc_atomic(qlen);     \
  uint8_t *tbuf = tlen <= sizeof(static_tbuf) ? &static_tbuf[0]                        \
                                              : (uint8_t *)seq_alloc_atomic(tlen);     \
  (enc_func)(query, qbuf);                                                             \
  (enc_func)(target, tbuf)

#define ALIGN_RELEASE()                                                                \
  if (qbuf != &static_qbuf[0])                                                         \
    seq_free(qbuf);                                                                    \
  if (tbuf != &static_tbuf[0])                                                         \
  seq_free(tbuf)

SEQ_FUNC void seq_align(seq_t query, seq_t target, int8_t *mat, int8_t gapo,
                        int8_t gape, seq_int_t bandwidth, seq_int_t zdrop,
                        seq_int_t end_bonus, seq_int_t flags, Alignment *out) {
  ksw_extz_t ez;
  ALIGN_ENCODE(encode);
  ksw_extz2_sse(nullptr, qlen, qbuf, tlen, tbuf, 5, mat, gapo, gape, (int)bandwidth,
                (int)zdrop, end_bonus, (int)flags, &ez);
  ALIGN_RELEASE();
  *out = {{ez.cigar, ez.n_cigar}, flags & KSW_EZ_EXTZ_ONLY ? ez.max : ez.score};
}

SEQ_FUNC void seq_align_default(seq_t query, seq_t target, Alignment *out) {
  static const int8_t mat[] = {0,  -1, -1, -1, -1, -1, 0,  -1, -1, -1, -1, -1, 0,
                               -1, -1, -1, -1, -1, 0,  -1, -1, -1, -1, -1, -1};
  int m_cigar = 0;
  int n_cigar = 0;
  uint32_t *cigar = nullptr;
  ALIGN_ENCODE(encode);
  int score = ksw_gg2_sse(nullptr, qlen, qbuf, tlen, tbuf, 5, mat, 0, 1, -1, &m_cigar,
                          &n_cigar, &cigar);
  ALIGN_RELEASE();
  *out = {{cigar, n_cigar}, score};
}

SEQ_FUNC void seq_align_dual(seq_t query, seq_t target, int8_t *mat, int8_t gapo1,
                             int8_t gape1, int8_t gapo2, int8_t gape2,
                             seq_int_t bandwidth, seq_int_t zdrop, seq_int_t end_bonus,
                             seq_int_t flags, Alignment *out) {
  ksw_extz_t ez;
  ALIGN_ENCODE(encode);
  ksw_extd2_sse(nullptr, qlen, qbuf, tlen, tbuf, 5, mat, gapo1, gape1, gapo2, gape2,
                (int)bandwidth, (int)zdrop, end_bonus, (int)flags, &ez);
  ALIGN_RELEASE();
  *out = {{ez.cigar, ez.n_cigar}, flags & KSW_EZ_EXTZ_ONLY ? ez.max : ez.score};
}

SEQ_FUNC void seq_align_splice(seq_t query, seq_t target, int8_t *mat, int8_t gapo1,
                               int8_t gape1, int8_t gapo2, int8_t noncan,
                               seq_int_t zdrop, seq_int_t flags, Alignment *out) {
  ksw_extz_t ez;
  ALIGN_ENCODE(encode);
  ksw_exts2_sse(nullptr, qlen, qbuf, tlen, tbuf, 5, mat, gapo1, gape1, gapo2, noncan,
                (int)zdrop, (int)flags, &ez);
  ALIGN_RELEASE();
  *out = {{ez.cigar, ez.n_cigar}, flags & KSW_EZ_EXTZ_ONLY ? ez.max : ez.score};
}

SEQ_FUNC void seq_align_global(seq_t query, seq_t target, int8_t *mat, int8_t gapo,
                               int8_t gape, seq_int_t bandwidth, bool backtrace,
                               Alignment *out) {
  int m_cigar = 0;
  int n_cigar = 0;
  uint32_t *cigar = nullptr;
  ALIGN_ENCODE(encode);
  int score = ksw_gg2_sse(nullptr, qlen, qbuf, tlen, tbuf, 5, mat, gapo, gape,
                          (int)bandwidth, &m_cigar, &n_cigar, &cigar);
  ALIGN_RELEASE();
  *out = {{backtrace ? cigar : nullptr, backtrace ? n_cigar : 0}, score};
}

SEQ_FUNC void seq_palign(seq_t query, seq_t target, int8_t *mat, int8_t gapo,
                         int8_t gape, seq_int_t bandwidth, seq_int_t zdrop,
                         seq_int_t end_bonus, seq_int_t flags, Alignment *out) {
  ksw_extz_t ez;
  ALIGN_ENCODE(pencode);
  ksw_extz2_sse(nullptr, qlen, qbuf, tlen, tbuf, 23, mat, gapo, gape, (int)bandwidth,
                (int)zdrop, end_bonus, (int)flags, &ez);
  ALIGN_RELEASE();
  *out = {{ez.cigar, ez.n_cigar}, flags & KSW_EZ_EXTZ_ONLY ? ez.max : ez.score};
}

SEQ_FUNC void seq_palign_default(seq_t query, seq_t target, Alignment *out) {
  // Blosum-62
  static const int8_t mat[] = {
      4,  -2, 0,  -2, -1, -2, 0,  -2, -1, -1, -1, -1, -2, -1, -1, -1, 1,  0,  0,  -3,
      0,  -2, -1, -2, 4,  -3, 4,  1,  -3, -1, 0,  -3, 0,  -4, -3, 3,  -2, 0,  -1, 0,
      -1, -3, -4, -1, -3, 1,  0,  -3, 9,  -3, -4, -2, -3, -3, -1, -3, -1, -1, -3, -3,
      -3, -3, -1, -1, -1, -2, -2, -2, -3, -2, 4,  -3, 6,  2,  -3, -1, -1, -3, -1, -4,
      -3, 1,  -1, 0,  -2, 0,  -1, -3, -4, -1, -3, 1,  -1, 1,  -4, 2,  5,  -3, -2, 0,
      -3, 1,  -3, -2, 0,  -1, 2,  0,  0,  -1, -2, -3, -1, -2, 4,  -2, -3, -2, -3, -3,
      6,  -3, -1, 0,  -3, 0,  0,  -3, -4, -3, -3, -2, -2, -1, 1,  -1, 3,  -3, 0,  -1,
      -3, -1, -2, -3, 6,  -2, -4, -2, -4, -3, 0,  -2, -2, -2, 0,  -2, -3, -2, -1, -3,
      -2, -2, 0,  -3, -1, 0,  -1, -2, 8,  -3, -1, -3, -2, 1,  -2, 0,  0,  -1, -2, -3,
      -2, -1, 2,  0,  -1, -3, -1, -3, -3, 0,  -4, -3, 4,  -3, 2,  1,  -3, -3, -3, -3,
      -2, -1, 3,  -3, -1, -1, -3, -1, 0,  -3, -1, 1,  -3, -2, -1, -3, 5,  -2, -1, 0,
      -1, 1,  2,  0,  -1, -2, -3, -1, -2, 1,  -1, -4, -1, -4, -3, 0,  -4, -3, 2,  -2,
      4,  2,  -3, -3, -2, -2, -2, -1, 1,  -2, -1, -1, -3, -1, -3, -1, -3, -2, 0,  -3,
      -2, 1,  -1, 2,  5,  -2, -2, 0,  -1, -1, -1, 1,  -1, -1, -1, -1, -2, 3,  -3, 1,
      0,  -3, 0,  1,  -3, 0,  -3, -2, 6,  -2, 0,  0,  1,  0,  -3, -4, -1, -2, 0,  -1,
      -2, -3, -1, -1, -4, -2, -2, -3, -1, -3, -2, -2, 7,  -1, -2, -1, -1, -2, -4, -2,
      -3, -1, -1, 0,  -3, 0,  2,  -3, -2, 0,  -3, 1,  -2, 0,  0,  -1, 5,  1,  0,  -1,
      -2, -2, -1, -1, 3,  -1, -1, -3, -2, 0,  -3, -2, 0,  -3, 2,  -2, -1, 0,  -2, 1,
      5,  -1, -1, -3, -3, -1, -2, 0,  1,  0,  -1, 0,  0,  -2, 0,  -1, -2, 0,  -2, -1,
      1,  -1, 0,  -1, 4,  1,  -2, -3, 0,  -2, 0,  0,  -1, -1, -1, -1, -2, -2, -2, -1,
      -1, -1, -1, 0,  -1, -1, -1, 1,  5,  0,  -2, 0,  -2, -1, 0,  -3, -1, -3, -2, -1,
      -3, -3, 3,  -2, 1,  1,  -3, -2, -2, -3, -2, 0,  4,  -3, -1, -1, -2, -3, -4, -2,
      -4, -3, 1,  -2, -2, -3, -3, -2, -1, -4, -4, -2, -3, -3, -2, -3, 11, -2, 2,  -3,
      0,  -1, -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -1, -1, 0,  0,  -1, -2,
      -1, -1, -1, -2, -3, -2, -3, -2, 3,  -3, 2,  -1, -2, -1, -1, -2, -3, -1, -2, -2,
      -2, -1, 2,  -1, 7,  -2, -1, 1,  -3, 1,  4,  -3, -2, 0,  -3, 1,  -3, -1, 0,  -1,
      3,  0,  0,  -1, -2, -3, -1, -2, 4};
  ksw_extz_t ez;
  ALIGN_ENCODE(pencode);
  ksw_extz2_sse(nullptr, qlen, qbuf, tlen, tbuf, 23, mat, 11, 1, -1, -1,
                /* end_bonus */ 0, 0, &ez);
  ALIGN_RELEASE();
  *out = {{ez.cigar, ez.n_cigar}, ez.score};
}

SEQ_FUNC void seq_palign_dual(seq_t query, seq_t target, int8_t *mat, int8_t gapo1,
                              int8_t gape1, int8_t gapo2, int8_t gape2,
                              seq_int_t bandwidth, seq_int_t zdrop, seq_int_t end_bonus,
                              seq_int_t flags, Alignment *out) {
  ksw_extz_t ez;
  ALIGN_ENCODE(pencode);
  ksw_extd2_sse(nullptr, qlen, qbuf, tlen, tbuf, 23, mat, gapo1, gape1, gapo2, gape2,
                (int)bandwidth, (int)zdrop, end_bonus, (int)flags, &ez);
  ALIGN_RELEASE();
  *out = {{ez.cigar, ez.n_cigar}, flags & KSW_EZ_EXTZ_ONLY ? ez.max : ez.score};
}

SEQ_FUNC void seq_palign_global(seq_t query, seq_t target, int8_t *mat, int8_t gapo,
                                int8_t gape, seq_int_t bandwidth, Alignment *out) {
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

typedef struct __kstring_t {
  size_t l, m;
  char *s;
} kstring_t;

typedef struct htsFormat {
  int32_t category;
  int32_t format;
  struct {
    short major, minor;
  } version;
  int32_t compression;
  short compression_level;
  void *specific;
} htsFormat;

typedef struct {
  uint32_t is_bin : 1, is_write : 1, is_be : 1, is_cram : 1, is_bgzf : 1, dummy : 27;
  int64_t lineno;
  kstring_t line;
  char *fn, *fn_aux;
  void *fp;
  void *state; // format specific state information
  htsFormat format;
  void *idx;
  const char *fnidx;
  void *bam_header;
} htsFile;

SEQ_FUNC bool seq_is_htsfile_cram(htsFile *f) { return f->is_cram; }
SEQ_FUNC bool seq_is_htsfile_bgzf(htsFile *f) { return f->is_bgzf; }
SEQ_FUNC void *seq_get_htsfile_fp(htsFile *f) { return f->fp; }
SEQ_FUNC double seq_i32_to_float(int32_t x) { return (double)(*(float *)&x); }
