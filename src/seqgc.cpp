#include "seq/seqgc.h"
#include <gc.h>

SEQ_FUNC void seq::seqGCInit()
{
	GC_init();
}

SEQ_FUNC void *seq::seqAlloc(size_t n)
{
	return GC_malloc(n);
}

SEQ_FUNC void *seq::seqAllocAtomic(size_t n)
{
	return GC_malloc_atomic(n);
}

SEQ_FUNC void *seq::seqRealloc(void *p, size_t n)
{
	return GC_realloc(p, n);
}
