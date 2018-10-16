#include "seq/seq.h"

using namespace seq;

std::vector<Op> ops;

static void init()
{
	if (ops.empty()) {
		ops = {
			{"~", false, true, 100, "__invert__"},
			{"!", false, true, 100},
			{"-", false, true, 100, "__neg__"},
			{"+", false, true, 100, "__pos__"},
			{"**", true, true, 95,  "__pow__", "__rpow__"},
			{"*", true, false, 90,  "__mul__", "__rmul__"},
			{"/", true, false, 90,  "__div__", "__rdiv__"},
			{"%", true, false, 90,  "__mod__", "__rmod__"},
			{"+", true, false, 80,  "__add__", "__radd__"},
			{"-", true, false, 80,  "__sub__", "__rsub__"},
			{"<<", true, false, 70, "__lshift__", "__rlshift__"},
			{">>", true, false, 70, "__rshift__", "__rrshift__"},
			{"<", true, false, 60,  "__lt__"},
			{">", true, false, 60,  "__gt__"},
			{"<=", true, false, 60, "__le__"},
			{">=", true, false, 60, "__ge__"},
			{"==", true, false, 50, "__eq__"},
			{"!=", true, false, 50, "__ne__"},
			{"&", true, false, 40,  "__and__", "__rand__"},
			{"^", true, false, 30,  "__xor__", "__rxor__"},
			{"|", true, false, 20,  "__or__", "__ror__"},
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

	assert(0);
	return ops[0];
}

Op seq::bop(const std::string& symbol)
{
	init();

	for (auto& op : ops) {
		if (op.binary && op.symbol == symbol)
			return op;
	}

	assert(0);
	return ops[0];
}
