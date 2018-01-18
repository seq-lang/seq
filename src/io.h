#ifndef SEQ_IO_H
#define SEQ_IO_H

#include <cstdint>
#include <string>
#include <fstream>

namespace seq {
	namespace io {
		static const size_t DEFAULT_BLOCK_SIZE = 1000;

		struct SeqData {
			char *data;
			uint32_t len;

			SeqData();
			~SeqData();

			bool read(std::ifstream &in);
		private:
			SeqData(char *data, uint32_t len);
		};

		struct DataBlock {
			SeqData block[DEFAULT_BLOCK_SIZE];
			size_t len;
			const size_t cap;

			explicit DataBlock(size_t cap);
			DataBlock();

			void read(std::ifstream &in);
		};

		enum Format {
			FASTQ,
			FASTA,
			SAM,
			BAM
		};
	}
}

#endif /* SEQ_IO_H */
