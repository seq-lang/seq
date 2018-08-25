#include "seq/seq.h"

using namespace seq;

std::vector<Op> ops;

static void init()
{
	if (ops.empty()) {
		ops = {
			{"~", false, true, 100},
			{"!", false, true, 100},
			{"-", false, true, 100},
			{"+", false, true, 100},
			{"*", true, false, 90},
			{"/", true, false, 90},
			{"%", true, false, 90},
			{"+", true, false, 80},
			{"-", true, false, 80},
			{"<<", true, false, 70},
			{">>", true, false, 70},
			{"<", true, false, 60},
			{">", true, false, 60},
			{"<=", true, false, 60},
			{">=", true, false, 60},
			{"==", true, false, 50},
			{"!=", true, false, 50},
			{"&", true, false, 40},
			{"^", true, false, 30},
			{"|", true, false, 20},
			{"&&", true, false, 10},
			{"||", true, false, 0},
		};
	}
}

Op seq::uop(const std::string& symbol)
{
	init();

	for (auto& op : ops) {
		if (!op.binary && op.symbol == symbol)
			return op;
	}

	throw exc::SeqException("invalid unary operator: '" + symbol + "'");
}

Op seq::bop(const std::string& symbol)
{
	init();

	for (auto& op : ops) {
		if (op.binary && op.symbol == symbol)
			return op;
	}

	throw exc::SeqException("invalid binary operator: '" + symbol + "'");
}
