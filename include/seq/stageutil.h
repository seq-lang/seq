#ifndef SEQ_STAGEUTIL_H
#define SEQ_STAGEUTIL_H

#include "stage.h"
#include "copy.h"
#include "filter.h"
#include "op.h"
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
#include "serialize.h"
#include "capture.h"

namespace seq {
	namespace stageutil {
		Copy& copy();
		Filter& filter(Func& func);
		Op& op(std::string name, SeqOp op);
		Hash& hash(std::string name, SeqHash hash);
		Print& print();
		RevComp& revcomp();
		Split& split(seq_int_t k, seq_int_t step);
		Substr& substr(seq_int_t start, seq_int_t len);
		Len& len();
		Count& count();
		Range& range(seq_int_t from, seq_int_t to, seq_int_t step);
		Range& range(seq_int_t from, seq_int_t to);
		Range& range(seq_int_t to);
		LambdaStage& lambda(LambdaContext& lambdaContext);
		ForEach& foreach();
		Collect& collect();
		Chunk& chunk(Func& key);
		Chunk& chunk();
		Serialize& ser(std::string filename);
		Deserialize& deser(types::Type& type, std::string filename);
		Capture& capture(void *addr);
	}
}

#endif /* SEQ_STAGEUTIL_H */
