#include <cstdlib>
#include "stageutil.h"

using namespace seq;

Copy &stageutil::copy()
{
	return Copy::make();
}

Filter& stageutil::filter(std::string name, SeqPred op)
{
	return Filter::make(std::move(name), op);
}

Op &stageutil::op(std::string name, SeqOp op)
{
	return Op::make(std::move(name), op);
}

Print &stageutil::print()
{
	return Print::make();
}

RevComp &stageutil::revcomp()
{
	return RevComp::make();
}

Split &stageutil::split(uint32_t k, uint32_t step)
{
	return Split::make(k, step);
}

Substr &stageutil::substr(uint32_t start, uint32_t len)
{
	return Substr::make(start, len);
}
