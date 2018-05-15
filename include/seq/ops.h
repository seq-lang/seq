#ifndef SEQ_OPS_H
#define SEQ_OPS_H

#include <vector>
#include "exc.h"

namespace seq {

	struct Op {
		std::string symbol;
		bool binary;
		bool rightAssoc;
		int prec;

		bool operator==(const Op& op)
		{
			return op.symbol == symbol && op.binary == binary;
		}
	};

	Op uop(const std::string& symbol);
	Op bop(const std::string& symbol);

}

#endif /* SEQ_OPS_H */
