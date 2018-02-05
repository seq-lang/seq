#ifndef SEQ_TYPES_H
#define SEQ_TYPES_H

namespace seq {
	namespace types {
		struct Base {
		};

		struct Void : Base {
		};

		struct Seq : Base {
		};

		template<unsigned K>
		struct Mer : Seq {
		};

		struct Number : Base {
		};

		struct Int : Number {
		};

		struct Float : Number {
		};

		template<typename T>
		struct Array : Base {
		};

		typedef Base Type;
	}
}

#endif /* SEQ_TYPES_H */
