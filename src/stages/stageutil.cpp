#include <cstdlib>
#include "seq/stageutil.h"

using namespace seq;

Nop& stageutil::nop()
{
	return Nop::make();
}

Copy& stageutil::copy()
{
	return Copy::make();
}

Filter& stageutil::filter(Expr *key)
{
	return Filter::make(key);
}

Filter& stageutil::filter(Func& key)
{
	return Filter::make(key);
}

OpStage& stageutil::op(std::string name, SeqOp op)
{
	return OpStage::make(std::move(name), op);
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

Split& stageutil::split(Expr *k, Expr *step)
{
	return Split::make(k, step);
}

Split& stageutil::split(seq_int_t k, seq_int_t step)
{
	return Split::make(k, step);
}

Substr& stageutil::substr(Expr *start, Expr *len)
{
	return Substr::make(start, len);
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

Range& stageutil::range(Expr *from, Expr *to, Expr *step)
{
	return Range::make(from, to, step);
}

Range& stageutil::range(Expr *from, Expr *to)
{
	return Range::make(from, to);
}

Range& stageutil::range(Expr *to)
{
	return Range::make(to);
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

Chunk& stageutil::chunk(Expr *key)
{
	return Chunk::make(key);
}

Chunk& stageutil::chunk(Func& key)
{
	return Chunk::make(key);
}

Chunk& stageutil::chunk()
{
	return Chunk::make();
}

GetItem& stageutil::get(seq_int_t idx)
{
	return GetItem::make(idx);
}

Serialize& stageutil::ser(std::string filename)
{
	return Serialize::make(std::move(filename));
}

Deserialize& stageutil::deser(types::Type& type, std::string filename)
{
	return Deserialize::make(&type, std::move(filename));
}

ExprStage& stageutil::expr(Expr *expr)
{
	return ExprStage::make(expr);
}

CellStage& stageutil::cell(Cell *cell)
{
	return CellStage::make(cell);
}

AssignStage& stageutil::assign(Cell *cell, Expr *value)
{
	return AssignStage::make(cell, value);
}

AssignIndexStage& stageutil::assignindex(Expr *array, Expr *idx, Expr *value)
{
	return AssignIndexStage::make(array, idx, value);
}

AssignMemberStage& stageutil::assignmemb(Expr *expr, seq_int_t idx, Expr *value)
{
	return AssignMemberStage::make(expr, idx, value);
}

AssignMemberStage& stageutil::assignmemb(Expr *expr, std::string memb, Expr *value)
{
	return AssignMemberStage::make(expr, memb, value);
}

Capture& stageutil::capture(void *addr)
{
	return Capture::make(addr);
}

Source& stageutil::source(std::vector<Expr *> sources)
{
	return Source::make(std::move(sources));
}

If& stageutil::ifstage()
{
	return If::make();
}

Match& stageutil::matchstage()
{
	return Match::make();
}

While& stageutil::whilestage(Expr *cond)
{
	return While::make(cond);
}

For& stageutil::forstage(Expr *gen)
{
	return For::make(gen);
}

Return& stageutil::ret(Expr *expr)
{
	return Return::make(expr);
}

Yield& stageutil::yield(Expr *expr)
{
	return Yield::make(expr);
}

Break& stageutil::brk()
{
	return Break::make();
}

Continue& stageutil::cnt()
{
	return Continue::make();
}
