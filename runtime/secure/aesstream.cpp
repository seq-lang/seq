#include <iostream>
#include "aesstream.h"
#include "crypto.h"
#include "util.h"

using namespace std;

AESStream::AESStream(const unsigned char *key) {
  setup_prf_key(&(this->key), key, PRF_KEY_BYTES);
  counter = 0;
}

void AESStream::get(unsigned char *res, long n) {
  if (n < 0) {
    cout << "AESStream::get: bad args" << endl;
  } else if (n == 0) {
    return;
  } else {

    long nblock = 1 + ((n - 1) / (long) PRF_BLOCK_LEN);
    long cur = 0;
    for (long i = 0; i < nblock; i++) {
      block b = MAKE_BLOCK(0, counter);
      prf_eval_in_place(&b, &key);

      long copylen;
      if (i == nblock - 1) {
        copylen = n - cur;
      } else {
        copylen = (long) PRF_BLOCK_LEN;
      }

      memcpy(res + cur, &b, copylen);

      cur += copylen;
      counter++;
    }

  }
}
