#ifndef SEQ_EXC_H
#define SEQ_EXC_H

#include <string>
#include <exception>
#include <stdexcept>

namespace seq {
	namespace exc {
		class SeqException : public std::runtime_error {
		public:
			explicit SeqException(std::string msg);
		};

		class IOException : public std::runtime_error {
		public:
			explicit IOException(std::string msg);
		};
	}
}

#endif /* SEQ_EXC_H */
