#ifndef SEQ_SEQGC_H
#define SEQ_SEQGC_H

#include <cstddef>
#include "common.h"

namespace seq {
	SEQ_FUNC void seqGCInit();
	SEQ_FUNC void *seqAlloc(size_t);
	SEQ_FUNC void *seqAllocAtomic(size_t);
	SEQ_FUNC void *seqRealloc(void *, size_t);
}

#endif /* SEQ_SEQGC_H */
