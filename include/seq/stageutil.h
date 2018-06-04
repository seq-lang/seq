#ifndef SEQ_STAGEUTIL_H
#define SEQ_STAGEUTIL_H

#include "stage.h"
#include "copy.h"
#include "filter.h"
#include "opstage.h"
#include "hash.h"
#include "print.h"
#include "revcomp.h"
#include "split.h"
#include "substr.h"
#include "len.h"
#include "count.h"
#include "range.h"
#include "lambda.h"
#include "foreach.h"
#include "collect.h"
#include "chunk.h"
#include "call.h"
#include "getitem.h"
#include "makerec.h"
#include "serialize.h"
#include "exprstage.h"
#include "capture.h"
#include "source.h"

namespace seq {
	namespace stageutil {
		Nop& nop();
		Copy& copy();
		Filter& filter(Expr *key);
		Filter& filter(Func& key);
		OpStage& op(std::string name, SeqOp op);
		Hash& hash(std::string name, SeqHash hash);
		Print& print();
		RevComp& revcomp();
		Split& split(Expr *k, Expr *step);
		Split& split(seq_int_t k, seq_int_t step);
		Substr& substr(Expr *start, Expr *len);
		Substr& substr(seq_int_t start, seq_int_t len);
		Len& len();
		Count& count();
		Range& range(Expr *from, Expr *to, Expr *step);
		Range& range(Expr *from, Expr *to);
		Range& range(Expr *to);
		Range& range(seq_int_t from, seq_int_t to, seq_int_t step);
		Range& range(seq_int_t from, seq_int_t to);
		Range& range(seq_int_t to);
		LambdaStage& lambda(LambdaContext& lambdaContext);
		ForEach& foreach();
		Collect& collect();
		Chunk& chunk(Expr* key);
		Chunk& chunk(Func& key);
		Chunk& chunk();
		GetItem& get(seq_int_t idx);
		Serialize& ser(std::string filename);
		Deserialize& deser(types::Type& type, std::string filename);
		ExprStage& expr(Expr *expr);
		CellStage& cell(Cell *cell);
		AssignStage& assign(Cell *cell, Expr *value);
		AssignIndexStage& assignindex(Expr *array, Expr *idx, Expr *value);
		AssignMemberStage& assignmemb(Cell *cell, seq_int_t idx, Expr *value);
		AssignMemberStage& assignmemb(Cell *cell, std::string name, Expr *value);
		Capture& capture(void *addr);
		Source& source(std::vector<Expr *> sources);
		If& ifstage();
		While& whilestage(Expr *cond);
		Return& ret(Expr *expr);
		Break& brk();
		Continue& cnt();
	}
}

#endif /* SEQ_STAGEUTIL_H */
