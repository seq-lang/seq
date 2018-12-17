#ifndef SEQ_OPS_H
#define SEQ_OPS_H

#include <string>
#include <vector>

namespace seq {

	struct Op {
		std::string symbol;
		bool binary;
		std::string magic;
		std::string magicReflected;

		Op(std::string symbol, bool binary, std::string magic="", std::string magicReflected="") :
		    symbol(std::move(symbol)), binary(std::move(binary)), magic(std::move(magic)),
		    magicReflected(std::move(magicReflected))
		{
		}

		bool operator==(const Op& op) const
		{
			return op.symbol == symbol && op.binary == binary;
		}
	};

	Op uop(const std::string& symbol);
	Op bop(const std::string& symbol);

}

#endif /* SEQ_OPS_H */
