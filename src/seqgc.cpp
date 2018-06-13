#include "seq/seqgc.h"
#include <gc.h>

SEQ_FUNC void seq::seqGCInit()
{
	GC_INIT();
}

SEQ_FUNC void *seq::seqAlloc(size_t n)
{
	return GC_MALLOC(n);
}

SEQ_FUNC void *seq::seqAllocAtomic(size_t n)
{
	return GC_MALLOC_ATOMIC(n);
}

SEQ_FUNC void *seq::seqRealloc(void *p, size_t n)
{
	return GC_REALLOC(p, n);
}
