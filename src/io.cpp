#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <cassert>
#include "exc.h"
#include "io.h"

using namespace seq;
using namespace seq::io;
using namespace std;

#define SAFE(x) if (!(x)) return false;

const map<string, Format> io::EXT_CONV = {{"txt",   Format::TXT},
                                          {"fastq", Format::FASTQ},
                                          {"fq",    Format::FASTQ},
                                          {"fasta", Format::FASTA},
                                          {"fa",    Format::FASTA},
                                          {"sam",   Format::SAM},
                                          {"bam",   Format::FASTQ}};

io::DataCell::DataCell(char *buf, const uint32_t cap) :
    buf(buf), cap(cap)
{
}

io::DataCell::DataCell() : DataCell(nullptr, 0)
{
}

io::DataCell::~DataCell()
{
	free(buf);
}

bool DataCell::readTXT(std::ifstream& in)
{
	string seq;
	SAFE(getline(in, seq));
	const auto line_len = seq.length();
	const auto new_cap = (line_len + 1);

	if (new_cap > cap) {
		buf = (char *)realloc(buf, new_cap);
		assert(buf);
		cap = new_cap;
	}

	seq.copy(buf, line_len);
	buf[line_len] = '\0';

	data[SeqData::SEQ] = &buf[0];
	lens[SeqData::SEQ] = (uint32_t)line_len;

	return true;
}

bool DataCell::readFASTQ(std::ifstream& in)
{
	string ident, seq, sep, qual;
	SAFE(getline(in, ident));
	SAFE(getline(in, seq));
	SAFE(getline(in, sep));
	SAFE(getline(in, qual));

	const auto seq_len = seq.length();
	const auto qual_len = qual.length();
	const auto ident_len = ident.length();
	const auto new_cap = (seq_len + 1) + (qual_len + 1) + (ident_len + 1);

	if (new_cap > cap) {
		buf = (char *)realloc(buf, new_cap);
		assert(buf);
		cap = new_cap;
	}

	seq.copy(&buf[0], seq_len);
	qual.copy(&buf[seq_len + 1], qual_len);
	ident.copy(&buf[seq_len + qual_len + 2], ident_len);

	buf[seq_len] =
	  buf[seq_len + qual_len + 1] =
	    buf[seq_len + qual_len + ident_len + 2] = '\0';

	data[SeqData::SEQ] = &buf[0];
	data[SeqData::QUAL] = &buf[seq_len + 1];
	data[SeqData::IDENT] = &buf[seq_len + qual_len + 2];

	lens[SeqData::SEQ] = (uint32_t)seq_len;
	lens[SeqData::QUAL] = (uint32_t)qual_len;
	lens[SeqData::IDENT] = (uint32_t)ident_len;

	return true;
}

bool DataCell::read(std::ifstream& in, Format fmt)
{
	switch (fmt) {
		case Format::TXT:
			return readTXT(in);
		case Format::FASTQ:
			return readFASTQ(in);
		case Format::FASTA:
			// TODO
		case Format::SAM:
			// TODO
		case Format::BAM:
			// TODO
		default:
			break;
	}

	assert(false);
}

io::DataBlock::DataBlock(const size_t cap) : len(0), cap(cap)
{
}

io::DataBlock::DataBlock() : DataBlock(io::DEFAULT_BLOCK_SIZE)
{
}

void io::DataBlock::read(std::ifstream& in, Format fmt)
{
	for (len = 0; len < cap; len++) {
		if (!block[len].read(in, fmt))
			break;
	}
}
