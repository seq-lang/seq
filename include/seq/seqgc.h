#ifndef SEQ_SEQGC_H
#define SEQ_SEQGC_H

#include <cstddef>
#include "common.h"

namespace seq {
	SEQ_FUNC void seqGCInit();
	SEQ_FUNC void *seqAlloc(size_t);
	SEQ_FUNC void *seqAllocAtomic(size_t);
	SEQ_FUNC void *seqRealloc(void *, size_t);

	const std::string GC_INIT_FUNC_NAME = "seqGCInit";
	const std::string ALLOC_FUNC_NAME = "seqAlloc";
	const std::string ALLOC_ATOMIC_FUNC_NAME = "seqAllocAtomic";
	const std::string REALLOC_FUNC_NAME = "seqRealloc";
}

#endif /* SEQ_SEQGC_H */
