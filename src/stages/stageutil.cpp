#include <cstdlib>
#include "stageutil.h"

using namespace seq;

Copy& stageutil::copy()
{
	return Copy::make();
}

Filter& stageutil::filter(std::string name, SeqPred op)
{
	return Filter::make(std::move(name), op);
}

Op& stageutil::op(std::string name, SeqOp op)
{
	return Op::make(std::move(name), op);
}

Hash& stageutil::hash(std::string name, SeqHash hash)
{
	return Hash::make(std::move(name), hash);
}

Print& stageutil::print()
{
	return Print::make();
}

RevComp& stageutil::revcomp()
{
	return RevComp::make();
}

Split& stageutil::split(seq_int_t k, seq_int_t step)
{
	return Split::make(k, step);
}

Substr& stageutil::substr(seq_int_t start, seq_int_t len)
{
	return Substr::make(start, len);
}

Len& stageutil::len()
{
	return Len::make();
}

Count& stageutil::count()
{
	return Count::make();
}

Range& stageutil::range(seq_int_t from, seq_int_t to, seq_int_t step)
{
	return Range::make(from, to, step);
}

Range& stageutil::range(seq_int_t from, seq_int_t to)
{
	return Range::make(from, to);
}

Range& stageutil::range(seq_int_t to)
{
	return Range::make(to);
}

LambdaStage& stageutil::lambda(LambdaContext& lambdaContext)
{
	return LambdaStage::make(lambdaContext);
}

ForEach& stageutil::foreach()
{
	return ForEach::make();
}

Collect& stageutil::collect()
{
	return Collect::make();
}

Serialize& stageutil::ser(std::string filename)
{
	return Serialize::make(std::move(filename));
}

Deserialize& stageutil::deser(types::Type *type, std::string filename)
{
	return Deserialize::make(type, std::move(filename));
}
