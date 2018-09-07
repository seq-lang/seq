#ifndef SEQ_BASE_H
#define SEQ_BASE_H

#include "types.h"

namespace seq {
	namespace types {

		class BaseType : public Type {
		private:
			BaseType();
		public:
			BaseType(BaseType const&)=delete;
			void operator=(BaseType const&)=delete;
			static BaseType *get() noexcept;
		};

	}
}

#endif /* SEQ_BASE_H */
