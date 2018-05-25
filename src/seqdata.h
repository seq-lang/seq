#ifndef SEQ_SEQDATA_H
#define SEQ_SEQDATA_H

namespace seq {
	enum SeqData {
		NONE,

		SEQ,
		LEN,
		QUAL,
		IDENT,

		INT,
		FLOAT,
		BOOL,
		STR,
		ARRAY,
		RECORD,
		FUNC,

		SEQ_DATA_COUNT
	};
}

#endif /* SEQ_SEQDATA_H */
