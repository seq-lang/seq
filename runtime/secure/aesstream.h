#ifndef __AESSTREAM_H_
#define __AESSTREAM_H_

#include "crypto.h"

class AESStream {

public:
  AESStream(const unsigned char *key);

  void get(unsigned char *res, long n);

private:
  AES_KEY key;
  uint64_t counter;

};

#endif
