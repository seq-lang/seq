#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#include "aes.h"
#include "errors.h"

#include <cstdint>
#include <cstring>

typedef unsigned char byte;

static const int PRF_KEY_BYTES     = 16;
static const int PRF_BLOCK_LEN     = 16;
static const int PRF_OUTPUT_BYTES  = 16;

static const int GCM_IV_LEN = 12;
static const int GCM_AUTH_TAG_LEN = 16;

/**
 * Reads from /dev/urandom to sample a PRF key.
 *
 * @param dst Byte array which will store the PRF key
 * @param dstlen Length of the destination byte array. Must match PRF_KEY_BYTES.
 * @return ERROR_NONE on success, ERROR_PRF_KEYLEN_INVALID if the destination
 * length is invalid, and ERROR_RANDOMNESS if reading from /dev/urandom failed.
 */
int generate_prf_key(AES_KEY* key);

int setup_prf_key(AES_KEY* key, const byte* buf, uint32_t buflen);

static inline int prf_eval_in_place(block* dst, const AES_KEY* key) {
  AES_ecb_encrypt_blk(dst, key);

  return ERROR_NONE;
}

static inline int prf_eval(block* dst, const AES_KEY* key, const block src) {
  *dst = src;
  return prf_eval_in_place(dst, key);
}

#endif /* __CRYPTO_H__ */
