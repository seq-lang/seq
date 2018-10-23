#include "seq/seq.h"

using namespace seq;

static std::vector<Op> ops;

static void init()
{
	if (ops.empty()) {
		ops = {
			{"~",  false, "__invert__"},
			{"!",  false},
			{"-",  false, "__neg__"},
			{"+",  false, "__pos__"},
			{"**", true,  "__pow__", "__rpow__"},
			{"*",  true,  "__mul__", "__rmul__"},
			{"/",  true,  "__div__", "__rdiv__"},
			{"%",  true,  "__mod__", "__rmod__"},
			{"+",  true,  "__add__", "__radd__"},
			{"-",  true,  "__sub__", "__rsub__"},
			{"<<", true,  "__lshift__", "__rlshift__"},
			{">>", true,  "__rshift__", "__rrshift__"},
			{"<",  true,  "__lt__"},
			{">",  true,  "__gt__"},
			{"<=", true,  "__le__"},
			{">=", true,  "__ge__"},
			{"==", true,  "__eq__"},
			{"!=", true,  "__ne__"},
			{"&",  true,  "__and__", "__rand__"},
			{"^",  true,  "__xor__", "__rxor__"},
			{"|",  true,  "__or__", "__ror__"},
			{"&&", true},
			{"||", true},
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
