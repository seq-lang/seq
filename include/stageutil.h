#ifndef SEQ_STAGEUTIL_H
#define SEQ_STAGEUTIL_H

#include "stage.h"
#include "copy.h"
#include "filter.h"
#include "op.h"
#include "print.h"
#include "revcomp.h"
#include "split.h"
#include "substr.h"

namespace seq {
	namespace stageutil {
		Copy& copy();
		Filter& filter(std::string name, SeqPred op);
		Op& op(std::string name, SeqOp op);
		Print& print();
		RevComp& revcomp();
		Split& split(uint32_t k, uint32_t step);
		Substr& substr(uint32_t start, uint32_t len);
	}
}

#endif /* SEQ_STAGEUTIL_H */
