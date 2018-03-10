#ifndef SEQ_IO_H
#define SEQ_IO_H

#include <cstdint>
#include <string>
#include <fstream>
#include <map>
#include "stage.h"

namespace seq {
	namespace io {
		static const size_t DEFAULT_BLOCK_SIZE = 1000;

		enum Format {
			TXT,
			FASTQ,
			FASTA,
			SAM,
			BAM
		};

		extern const std::map<std::string, Format> EXT_CONV;

		struct DataCell {
			char *buf;
			size_t cap;
			char *data[SeqData::SEQ_DATA_COUNT];
			uint32_t lens[SeqData::SEQ_DATA_COUNT];

			DataCell();
			~DataCell();

			bool read(std::ifstream& in, Format fmt);
		private:
			DataCell(char *data, uint32_t len);
			void ensureCap(const size_t new_cap);
			bool readTXT(std::ifstream& in);
			bool readFASTQ(std::ifstream& in);
			bool readFASTA(std::ifstream& in);
		};

		struct DataBlock {
			DataCell block[DEFAULT_BLOCK_SIZE];
			size_t len;
			const size_t cap;

			explicit DataBlock(size_t cap);
			DataBlock();

			void read(std::ifstream& in, Format fmt);
		};
	}
}

#endif /* SEQ_IO_H */
