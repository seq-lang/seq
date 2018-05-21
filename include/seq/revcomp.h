#ifndef SEQ_REVCOMP_H
#define SEQ_REVCOMP_H

#include "opstage.h"

namespace seq {
	class RevComp : public OpStage {
	public:
		RevComp();
		static RevComp& make();
	};
}

#endif /* SEQ_REVCOMP_H */
