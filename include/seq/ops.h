#ifndef SEQ_OPS_H
#define SEQ_OPS_H

#include <vector>

namespace seq {

	struct Op {
		std::string symbol;
		bool binary;
		bool rightAssoc;
		int prec;
		std::string magic;

		bool operator==(const Op& op) const
		{
			return op.symbol == symbol && op.binary == binary;
		}
	};

	Op uop(const std::string& symbol);
	Op bop(const std::string& symbol);

}

#endif /* SEQ_OPS_H */
