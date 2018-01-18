#ifndef SEQ_PRINT_H
#define SEQ_PRINT_H

#include "op.h"

namespace seq {
	class Print : public Op {
	public:
		Print();
		static Print& make();
	};
}

#endif /* SEQ_PRINT_H */
