#include <sstream>
#include <NTL/ZZ_p.h>
#include <NTL/ZZ.h>
#include <NTL/vec_ZZ.h>
#include "lib.h"

using namespace NTL;

/****************** ZZ ******************/

SEQ_FUNC ZZ*  ZZ_add (ZZ* a, ZZ* b) { return s_alloc<ZZ>((*a) + (*b)); }
SEQ_FUNC ZZ*  ZZ_sub (ZZ* a, ZZ* b) { return s_alloc<ZZ>((*a) - (*b)); }
SEQ_FUNC ZZ*  ZZ_mul (ZZ* a, ZZ* b) { return s_alloc<ZZ>((*a) * (*b)); }
SEQ_FUNC ZZ*  ZZ_div (ZZ* a, ZZ* b) { return s_alloc<ZZ>((*a) / (*b)); }
SEQ_FUNC ZZ*  ZZ_mod (ZZ* a, ZZ* b) { return s_alloc<ZZ>((*a) % (*b)); }
SEQ_FUNC ZZ*  ZZ_lsh (ZZ* a, seq_int_t b) { return s_alloc<ZZ>(LeftShift (*a, b)); }
SEQ_FUNC ZZ*  ZZ_rsh (ZZ* a, seq_int_t b) { return s_alloc<ZZ>(RightShift(*a, b)); }
SEQ_FUNC bool ZZ_eq  (ZZ* a, ZZ* b) { return ((*a) == (*b)); }
SEQ_FUNC bool ZZ_ne  (ZZ* a, ZZ* b) { return ((*a) != (*b)); }
SEQ_FUNC bool ZZ_lt  (ZZ* a, ZZ* b) { return ((*a) <  (*b)); }
SEQ_FUNC bool ZZ_le  (ZZ* a, ZZ* b) { return ((*a) <= (*b)); }
SEQ_FUNC bool ZZ_gt  (ZZ* a, ZZ* b) { return ((*a) >  (*b)); }
SEQ_FUNC bool ZZ_ge  (ZZ* a, ZZ* b) { return ((*a) >= (*b)); }

SEQ_FUNC ZZ* ZZ_init(seq_int_t val) 
	{ return s_alloc<ZZ>(val); }
SEQ_FUNC ZZ* ZZ_copy(ZZ* a) 
	{ return s_alloc<ZZ>(*a); }
SEQ_FUNC seq_str_t ZZ_str(ZZ* a) 
	{ return to_string(*a); }
SEQ_FUNC ZZ* ZZ_frombytes(const unsigned char *c, seq_int_t n) 
	{ return s_alloc<ZZ>(ZZFromBytes(c, n)); }
SEQ_FUNC void ZZ_tobytes(ZZ* a, unsigned char *c, seq_int_t n) 
	{ BytesFromZZ(c, *a, n); }
SEQ_FUNC ZZ_p* ZZ_to_ZP(ZZ* a) 
	{ return s_alloc<ZZ_p>(conv<ZZ_p>(*a)); }
SEQ_FUNC seq_int_t ZZ_bit(ZZ *a, seq_int_t n)
	{ return bit(*a, n); }
SEQ_FUNC ZZ* ZZ_setbit(ZZ *a, seq_int_t n)
	{ ZZ b = *a; SetBit(b, n); return s_alloc<ZZ>(b); }

/****************** ZZ_p ******************/

SEQ_FUNC ZZ_p* ZP_add (ZZ_p* a, ZZ_p* b) { return s_alloc<ZZ_p>((*a) + (*b)); }
SEQ_FUNC ZZ_p* ZP_sub (ZZ_p* a, ZZ_p* b) { return s_alloc<ZZ_p>((*a) - (*b)); }
SEQ_FUNC ZZ_p* ZP_mul (ZZ_p* a, ZZ_p* b) { return s_alloc<ZZ_p>((*a) * (*b)); }
SEQ_FUNC ZZ_p* ZP_div (ZZ_p* a, ZZ_p* b) { return s_alloc<ZZ_p>((*a) / (*b)); }
SEQ_FUNC bool  ZP_eq  (ZZ_p* a, ZZ_p* b) { return ((*a) == (*b)); }
SEQ_FUNC bool  ZP_ne  (ZZ_p* a, ZZ_p* b) { return ((*a) != (*b)); }

SEQ_FUNC void ZP_setup()
	{ ZZ_p::init(ZZ(127)); }
SEQ_FUNC ZZ_p* ZP_init(seq_int_t val) 
	{ return s_alloc<ZZ_p>(val);  } //s_alloc<ZZ_p>(val); }
SEQ_FUNC ZZ_p* ZP_copy(ZZ_p* a) 
	{ return s_alloc<ZZ_p>(*a); }
SEQ_FUNC ZZ* ZP_modulus(void)  
	{ return s_alloc<ZZ>(ZZ_p::modulus()); }
SEQ_FUNC seq_str_t ZP_str(ZZ_p* a) 
	{ return to_string(*a); }
SEQ_FUNC ZZ_p* ZP_rand() 
	{ return s_alloc<ZZ_p>(random_ZZ_p()); }
SEQ_FUNC ZZ* ZP_to_ZZ(ZZ_p* a) 
	{ return s_alloc<ZZ>(rep(*a)); }
SEQ_FUNC ZZ* ZP_trunc(ZZ_p *a, int n)
	{ return s_alloc<ZZ>(trunc_ZZ(rep(*a), n)); }

