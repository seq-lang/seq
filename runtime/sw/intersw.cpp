#include "intersw.h"
#include "ksw2.h"
#include "lib.h"
#include <cstdint>
#include <cstdlib>

struct InterAlignParams { // must be consistent with bio/align.seq
  int8_t a;
  int8_t b;
  int8_t ambig;
  int8_t gapo;
  int8_t gape;
  int8_t score_only;
  int32_t bandwidth;
  int32_t zdrop;
  int32_t end_bonus;
};

SEQ_FUNC void seq_inter_align128(InterAlignParams *paramsx,
                                 SeqPair *seqPairArray, uint8_t *seqBufRef,
                                 uint8_t *seqBufQer, int numPairs) {
  InterAlignParams params = *paramsx;
  const int8_t bandwidth = (0 <= params.bandwidth && params.bandwidth < 0xff)
                               ? params.bandwidth
                               : 0x7f;
  const int8_t zdrop =
      (0 <= params.zdrop && params.zdrop < 0xff) ? params.zdrop : 0x7f;
  if (params.score_only) {
    SW8 bsw(params.gapo, params.gape, params.gapo, params.gape, zdrop,
            params.end_bonus, params.a, params.b, params.ambig);
    bsw.SW(seqPairArray, seqBufRef, seqBufQer, numPairs, bandwidth);
  } else {
    SWbt8 bsw(params.gapo, params.gape, params.gapo, params.gape, zdrop,
              params.end_bonus, params.a, params.b, params.ambig);
    bsw.SW(seqPairArray, seqBufRef, seqBufQer, numPairs, bandwidth);
  }
}

SEQ_FUNC void seq_inter_align16(InterAlignParams *paramsx,
                                SeqPair *seqPairArray, uint8_t *seqBufRef,
                                uint8_t *seqBufQer, int numPairs) {
  InterAlignParams params = *paramsx;
  const int16_t bandwidth = (0 <= params.bandwidth && params.bandwidth < 0xffff)
                                ? params.bandwidth
                                : 0x7fff;
  const int16_t zdrop =
      (0 <= params.zdrop && params.zdrop < 0xffff) ? params.zdrop : 0x7fff;
  if (params.score_only) {
    SW16 bsw(params.gapo, params.gape, params.gapo, params.gape, zdrop,
             params.end_bonus, params.a, params.b, params.ambig);
    bsw.SW(seqPairArray, seqBufRef, seqBufQer, numPairs, bandwidth);
  } else {
    SWbt16 bsw(params.gapo, params.gape, params.gapo, params.gape, zdrop,
               params.end_bonus, params.a, params.b, params.ambig);
    bsw.SW(seqPairArray, seqBufRef, seqBufQer, numPairs, bandwidth);
  }
}

SEQ_FUNC void seq_inter_align1(InterAlignParams *paramsx, SeqPair *seqPairArray,
                               uint8_t *seqBufRef, uint8_t *seqBufQer,
                               int numPairs) {
  InterAlignParams params = *paramsx;
  int8_t a = params.a > 0 ? params.a : -params.a;
  int8_t b = params.b > 0 ? -params.b : params.b;
  int8_t ambig = params.ambig > 0 ? -params.ambig : params.ambig;
  int8_t mat[] = {a,     b,     b,     b,     ambig, b,     a,    b, b,
                  ambig, b,     b,     a,     b,     ambig, b,    b, b,
                  a,     ambig, ambig, ambig, ambig, ambig, ambig};
  ksw_extz_t ez;
  int flags = params.score_only ? KSW_EZ_SCORE_ONLY : 0;
  for (int i = 0; i < numPairs; i++) {
    SeqPair *sp = &seqPairArray[i];
    int myflags = flags | sp->flags;
    ksw_reset_extz(&ez);
    ksw_extz2_sse(nullptr, sp->len2, seqBufQer + SW8::LEN_LIMIT * sp->id,
                  sp->len1, seqBufRef + SW8::LEN_LIMIT * sp->id, /*m=*/5, mat,
                  params.gapo, params.gape, params.bandwidth, params.zdrop,
                  params.end_bonus, myflags, &ez);
    sp->score = (myflags & KSW_EZ_EXTZ_ONLY) ? ez.max : ez.score;
    sp->cigar = ez.cigar;
    sp->n_cigar = ez.n_cigar;
  }
}
