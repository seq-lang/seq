#include <sstream>
#include <NTL/ZZ_p.h>
#include <NTL/ZZ.h>
#include <NTL/vec_ZZ.h>
using namespace NTL;

#define EX extern "C"

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

EX ZZ*  ZZ_init (int val) { return new ZZ(val); }
EX void ZZ_free (ZZ* x)   { delete x; }

EX ZZ* ZZ_O_add     (ZZ* a, ZZ* b) { return new ZZ((*a) +  (*b)); }
EX ZZ* ZZ_O_sub     (ZZ* a, ZZ* b) { return new ZZ((*a) -  (*b)); }
EX ZZ* ZZ_O_mul     (ZZ* a, ZZ* b) { return new ZZ((*a) *  (*b)); }
EX ZZ* ZZ_O_truediv (ZZ* a, ZZ* b) { return new ZZ((*a) /  (*b)); }
EX ZZ* ZZ_O_mod     (ZZ* a, ZZ* b) { return new ZZ((*a) %  (*b)); }

EX bool ZZ_O_eq (ZZ* a, ZZ* b) { return ((*a) == (*b)); }
EX bool ZZ_O_ne (ZZ* a, ZZ* b) { return ((*a) != (*b)); }
EX bool ZZ_O_lt (ZZ* a, ZZ* b) { return ((*a) <  (*b)); }
EX bool ZZ_O_le (ZZ* a, ZZ* b) { return ((*a) <= (*b)); }
EX bool ZZ_O_gt (ZZ* a, ZZ* b) { return ((*a) >  (*b)); }
EX bool ZZ_O_ge (ZZ* a, ZZ* b) { return ((*a) >= (*b)); }

EX char* ZZ_O_str (ZZ* a) { return to_string(*a); }

/****************** ZZ_p ******************/

EX ZZ_p* ZZ_p_init (int val) { return new ZZ_p(val); }
EX void  ZZ_p_free (ZZ_p* x) { delete x; }

EX ZZ_p* ZZ_p_O_add     (ZZ_p* a, ZZ_p* b) { return new ZZ_p((*a) +  (*b)); }
EX ZZ_p* ZZ_p_O_sub     (ZZ_p* a, ZZ_p* b) { return new ZZ_p((*a) -  (*b)); }
EX ZZ_p* ZZ_p_O_mul     (ZZ_p* a, ZZ_p* b) { return new ZZ_p((*a) *  (*b)); }
EX ZZ_p* ZZ_p_O_truediv (ZZ_p* a, ZZ_p* b) { return new ZZ_p((*a) /  (*b)); }

EX bool ZZ_p_O_eq (ZZ_p* a, ZZ_p* b) { return ((*a) == (*b)); }
EX bool ZZ_p_O_ne (ZZ_p* a, ZZ_p* b) { return ((*a) != (*b)); }

EX void ZZ_p_init_p  (ZZ* a) { ZZ_p::init(*a); }
EX ZZ*  ZZ_p_modulus (void)  { return new ZZ(ZZ_p::modulus()); }

EX char* ZZ_p_O_str (ZZ_p* a) { return to_string(*a); }

/****************** VecZZ ******************/

EX VecZZ* VecZZ_init (void)     { return new VecZZ(); }
EX void   VecZZ_free (VecZZ* x) { delete x; }

EX bool VecZZ_O_eq (VecZZ* a, VecZZ* b) { return ((*a) == (*b)); }
EX bool VecZZ_O_ne (VecZZ* a, VecZZ* b) { return ((*a) != (*b)); }

EX ZZ*  VecZZ_O_getitem (VecZZ* a, int b)        { return new ZZ((*a)[b]); }
EX void VecZZ_O_setitem (VecZZ* a, int b, ZZ* z) { (*a)[b] = *z; }

EX int  VecZZ_length    (VecZZ* a)        { return a->length(); }
EX void VecZZ_SetLength (VecZZ* a, int b) { a->SetLength(b); }


/****************** VecZZ_p ******************/

EX VecZZ_p* VecZZ_p_init (void)       { return new VecZZ_p(); }
EX void     VecZZ_p_free (VecZZ_p* x) { delete x; }

EX bool VecZZ_p_O_eq (VecZZ_p* a, VecZZ_p* b) { return ((*a) == (*b)); }
EX bool VecZZ_p_O_ne (VecZZ_p* a, VecZZ_p* b) { return ((*a) != (*b)); }

EX ZZ_p* VecZZ_p_O_getitem (VecZZ_p* a, int b)          { return new ZZ_p((*a)[b]); }
EX void  VecZZ_p_O_setitem (VecZZ_p* a, int b, ZZ_p* z) { (*a)[b] = *z; }

EX int  VecZZ_p_length    (VecZZ_p* a)        { return a->length(); }
EX void VecZZ_p_SetLength (VecZZ_p* a, int b) { a->SetLength(b); }
