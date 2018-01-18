#ifndef SEQ_REVCOMP_H
#define SEQ_REVCOMP_H

#include "op.h"

namespace seq {
	class RevComp : public Op {
	public:
		RevComp();
		static RevComp& make();
	};
}

#endif /* SEQ_REVCOMP_H */
