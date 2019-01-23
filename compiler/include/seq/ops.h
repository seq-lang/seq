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
		std::string magicInPlace;

		Op(std::string symbol,
		   bool binary,
		   std::string magic="",
		   std::string magicReflected="",
		   std::string magicInPlace="") :
		    symbol(std::move(symbol)), binary(binary), magic(std::move(magic)),
		    magicReflected(std::move(magicReflected)), magicInPlace(std::move(magicInPlace))
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
