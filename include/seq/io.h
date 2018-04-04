#ifndef SEQ_IO_H
#define SEQ_IO_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <fstream>
#include <array>
#include <vector>
#include <map>
#include "stage.h"
#include "common.h"

namespace seq {
	namespace io {
		static const size_t DEFAULT_BLOCK_SIZE = 1000;
		static const size_t MAX_INPUTS = 5;

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
			size_t used;
			size_t cap;
			std::array<std::array<ptrdiff_t, SeqData::SEQ_DATA_COUNT>, MAX_INPUTS> data;
			std::array<std::array<seq_int_t, SeqData::SEQ_DATA_COUNT>, MAX_INPUTS> lens;
			std::array<seq_t, MAX_INPUTS> seqs;

			DataCell();
			~DataCell();

			bool read(std::vector<std::ifstream *>& ins, Format fmt);
			void clear();

			inline char *getData(const size_t idx, const SeqData key)
			{
				return buf + data[idx][key];
			}

			inline seq_int_t getLen(const size_t idx, const SeqData key)
			{
				return lens[idx][key];
			}
		private:
			DataCell(char *data, size_t len);
			char *ensureSpace(size_t idx, size_t space);

			bool read(size_t idx, std::ifstream& in, Format fmt);
			bool readTXT(size_t idx, std::ifstream& in);
			bool readFASTQ(size_t idx, std::ifstream& in);
			bool readFASTA(size_t idx, std::ifstream& in);
		};

		struct DataBlock {
			DataCell block[DEFAULT_BLOCK_SIZE];
			size_t len;
			const size_t cap;
			bool last;

			explicit DataBlock(size_t cap);
			DataBlock();

			void read(std::vector<std::ifstream *>& ins, Format fmt);
		};
	}
}

#endif /* SEQ_IO_H */
