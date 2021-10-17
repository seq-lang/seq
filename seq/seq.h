#ifndef SEQ_H
#define SEQ_H

#include <cstdint>

#define SEQ_FUNC extern "C"

typedef int64_t seq_int_t;

struct seq_t {
  seq_int_t len;
  char *seq;
};

#endif /* SEQ_H */
