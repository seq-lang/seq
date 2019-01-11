#include <sstream>
#include <NTL/ZZ_p.h>
#include <NTL/ZZ.h>
#include <NTL/vec_ZZ.h>
#include "lib.h"

using namespace NTL;

#define VecZZ   Vec<ZZ>
#define VecZZ_p Vec<ZZ_p>

template<typename T>
char *to_string(T x)
{
	std::ostringstream sout;
	sout << x;
	return strdup(sout.str().c_str());
}

/****************** ZZ ******************/

SEQ_FUNC ZZ*  ZZ_init (seq_int_t val) { return new ZZ(val); }
SEQ_FUNC void ZZ_free (ZZ* x)   { delete x; }

SEQ_FUNC ZZ* ZZ_O_add     (ZZ* a, ZZ* b) { return new ZZ((*a) +  (*b)); }
SEQ_FUNC ZZ* ZZ_O_sub     (ZZ* a, ZZ* b) { return new ZZ((*a) -  (*b)); }
SEQ_FUNC ZZ* ZZ_O_mul     (ZZ* a, ZZ* b) { return new ZZ((*a) *  (*b)); }
SEQ_FUNC ZZ* ZZ_O_truediv (ZZ* a, ZZ* b) { return new ZZ((*a) /  (*b)); }
SEQ_FUNC ZZ* ZZ_O_mod     (ZZ* a, ZZ* b) { return new ZZ((*a) %  (*b)); }

SEQ_FUNC bool ZZ_O_eq (ZZ* a, ZZ* b) { return ((*a) == (*b)); }
SEQ_FUNC bool ZZ_O_ne (ZZ* a, ZZ* b) { return ((*a) != (*b)); }
SEQ_FUNC bool ZZ_O_lt (ZZ* a, ZZ* b) { return ((*a) <  (*b)); }
SEQ_FUNC bool ZZ_O_le (ZZ* a, ZZ* b) { return ((*a) <= (*b)); }
SEQ_FUNC bool ZZ_O_gt (ZZ* a, ZZ* b) { return ((*a) >  (*b)); }
SEQ_FUNC bool ZZ_O_ge (ZZ* a, ZZ* b) { return ((*a) >= (*b)); }

SEQ_FUNC char* ZZ_O_str (ZZ* a) { return to_string(*a); }

SEQ_FUNC ZZ* ZZ_FromBytes (const unsigned char *c, seq_int_t n) { return new ZZ(ZZFromBytes(c, n)); }
SEQ_FUNC void ZZ_ToBytes  (ZZ *z, unsigned char *c, seq_int_t n) { BytesFromZZ(c, *z, n); }

/****************** ZZ_p ******************/

SEQ_FUNC ZZ_p* ZZ_p_init (seq_int_t val) { return new ZZ_p(val); }
SEQ_FUNC void  ZZ_p_free (ZZ_p* x) { delete x; }

SEQ_FUNC ZZ_p* ZZ_p_O_add     (ZZ_p* a, ZZ_p* b) { return new ZZ_p((*a) +  (*b)); }
SEQ_FUNC ZZ_p* ZZ_p_O_sub     (ZZ_p* a, ZZ_p* b) { return new ZZ_p((*a) -  (*b)); }
SEQ_FUNC ZZ_p* ZZ_p_O_mul     (ZZ_p* a, ZZ_p* b) { return new ZZ_p((*a) *  (*b)); }
SEQ_FUNC ZZ_p* ZZ_p_O_truediv (ZZ_p* a, ZZ_p* b) { return new ZZ_p((*a) /  (*b)); }

SEQ_FUNC bool ZZ_p_O_eq (ZZ_p* a, ZZ_p* b) { return ((*a) == (*b)); }
SEQ_FUNC bool ZZ_p_O_ne (ZZ_p* a, ZZ_p* b) { return ((*a) != (*b)); }

SEQ_FUNC void ZZ_p_init_p  (ZZ* a) { ZZ_p::init(*a); }
SEQ_FUNC ZZ*  ZZ_p_modulus (void)  { return new ZZ(ZZ_p::modulus()); }

SEQ_FUNC char* ZZ_p_O_str (ZZ_p* a) { return to_string(*a); }

/****************** VecZZ ******************/

SEQ_FUNC VecZZ* VecZZ_init (void)     { return new VecZZ(); }
SEQ_FUNC void   VecZZ_free (VecZZ* x) { delete x; }

SEQ_FUNC bool VecZZ_O_eq (VecZZ* a, VecZZ* b) { return ((*a) == (*b)); }
SEQ_FUNC bool VecZZ_O_ne (VecZZ* a, VecZZ* b) { return ((*a) != (*b)); }

SEQ_FUNC ZZ*  VecZZ_O_getitem (VecZZ* a, int b)        { return new ZZ((*a)[b]); }
SEQ_FUNC void VecZZ_O_setitem (VecZZ* a, int b, ZZ* z) { (*a)[b] = *z; }

SEQ_FUNC int  VecZZ_length    (VecZZ* a)        { return a->length(); }
SEQ_FUNC void VecZZ_SetLength (VecZZ* a, int b) { a->SetLength(b); }


/****************** VecZZ_p ******************/

SEQ_FUNC VecZZ_p* VecZZ_p_init (void)       { return new VecZZ_p(); }
SEQ_FUNC void     VecZZ_p_free (VecZZ_p* x) { delete x; }

SEQ_FUNC bool VecZZ_p_O_eq (VecZZ_p* a, VecZZ_p* b) { return ((*a) == (*b)); }
SEQ_FUNC bool VecZZ_p_O_ne (VecZZ_p* a, VecZZ_p* b) { return ((*a) != (*b)); }

SEQ_FUNC ZZ_p* VecZZ_p_O_getitem (VecZZ_p* a, int b)          { return new ZZ_p((*a)[b]); }
SEQ_FUNC void  VecZZ_p_O_setitem (VecZZ_p* a, int b, ZZ_p* z) { (*a)[b] = *z; }

SEQ_FUNC int  VecZZ_p_length    (VecZZ_p* a)        { return a->length(); }
SEQ_FUNC void VecZZ_p_SetLength (VecZZ_p* a, int b) { a->SetLength(b); }
