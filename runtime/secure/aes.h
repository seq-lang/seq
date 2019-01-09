/* crypto/aes/aes.h -*- mode:C; c-file-style: "eay" -*- */
/* ====================================================================
 * Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 */

#ifndef __AES_H__
#define __AES_H__

#include <xmmintrin.h>              /* SSE instructions and _mm_malloc */
#include <emmintrin.h>              /* SSE2 instructions               */
#include <wmmintrin.h>

typedef __m128i block;

typedef struct { block rd_key[15]; int rounds; } AES_KEY;

#define ROUNDS(ctx) ((ctx)->rounds)

#define MAKE_BLOCK(X,Y) _mm_set_epi64((__m64)((uint64_t) X), (__m64)((uint64_t) Y))

#define EXPAND_ASSIST(v1,v2,v3,v4,shuff_const,aes_const)                    \
    v2 = _mm_aeskeygenassist_si128(v4,aes_const);                           \
    v3 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(v3),              \
                                         _mm_castsi128_ps(v1), 16));        \
    v1 = _mm_xor_si128(v1,v3);                                              \
    v3 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(v3),              \
                                         _mm_castsi128_ps(v1), 140));       \
    v1 = _mm_xor_si128(v1,v3);                                              \
    v2 = _mm_shuffle_epi32(v2,shuff_const);                                 \
    v1 = _mm_xor_si128(v1,v2)

static inline void AES_128_Key_Expansion(const unsigned char *userkey, AES_KEY *key) {
    __m128i x0, x1, x2;
    __m128i *kp = (__m128i *) key;
    kp[0] = x0 = _mm_loadu_si128((__m128i *) userkey);
    x2 = _mm_setzero_si128();
    EXPAND_ASSIST(x0, x1, x2, x0, 255, 1);
    kp[1] = x0;
    EXPAND_ASSIST(x0, x1, x2, x0, 255, 2);
    kp[2] = x0;
    EXPAND_ASSIST(x0, x1, x2, x0, 255, 4);
    kp[3] = x0;
    EXPAND_ASSIST(x0, x1, x2, x0, 255, 8);
    kp[4] = x0;
    EXPAND_ASSIST(x0, x1, x2, x0, 255, 16);
    kp[5] = x0;
    EXPAND_ASSIST(x0, x1, x2, x0, 255, 32);
    kp[6] = x0;
    EXPAND_ASSIST(x0, x1, x2, x0, 255, 64);
    kp[7] = x0;
    EXPAND_ASSIST(x0, x1, x2, x0, 255, 128);
    kp[8] = x0;
    EXPAND_ASSIST(x0, x1, x2, x0, 255, 27);
    kp[9] = x0;
    EXPAND_ASSIST(x0, x1, x2, x0, 255, 54);
    kp[10] = x0;

    key->rounds = 10;
}

static inline void AES_ecb_encrypt_blk(block *blk, const AES_KEY *key) {
    unsigned j, rnds = ROUNDS(key);
    const __m128i *sched = ((__m128i *) (key->rd_key));
    *blk = _mm_xor_si128(*blk, sched[0]);
    for (j = 1; j < rnds; ++j) {
        *blk = _mm_aesenc_si128(*blk, sched[j]);
    }
    *blk = _mm_aesenclast_si128(*blk, sched[j]);
}

static inline void AES_ecb_encrypt_blks(block *blks, int nblks, const AES_KEY *key) {
    unsigned j, rnds = ROUNDS(key);
    const __m128i *sched = ((__m128i *) (key->rd_key));
    for (int i = 0; i < nblks; ++i)
        blks[i] = _mm_xor_si128(blks[i], sched[0]);
    for (j = 1; j < rnds; ++j)
        for (int i = 0; i < nblks; ++i)
            blks[i] = _mm_aesenc_si128(blks[i], sched[j]);
    for (int i = 0; i < nblks; ++i)
        blks[i] = _mm_aesenclast_si128(blks[i], sched[j]);
}

static inline void AES_ecb_encrypt_blks_3(block *blks, const AES_KEY *key) {
    unsigned j, rnds = ROUNDS(key);
    const __m128i *sched = ((__m128i *) (key->rd_key));
    blks[0] = _mm_xor_si128(blks[0], sched[0]);
    blks[1] = _mm_xor_si128(blks[1], sched[0]);
    blks[2] = _mm_xor_si128(blks[2], sched[0]);

    for (j = 1; j < rnds; ++j) {
        blks[0] = _mm_aesenc_si128(blks[0], sched[j]);
        blks[1] = _mm_aesenc_si128(blks[1], sched[j]);
        blks[2] = _mm_aesenc_si128(blks[2], sched[j]);
    }
    blks[0] = _mm_aesenclast_si128(blks[0], sched[j]);
    blks[1] = _mm_aesenclast_si128(blks[1], sched[j]);
    blks[2] = _mm_aesenclast_si128(blks[2], sched[j]);
}

#endif
