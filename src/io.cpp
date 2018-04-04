#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <cassert>
#include "seq/exc.h"
#include "seq/io.h"

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

DataCell::DataCell(char *buf, const size_t cap) :
    buf(buf), used(0), cap(cap)
{
}

DataCell::DataCell() :
    DataCell(nullptr, 0)
{
}

DataCell::~DataCell()
{
	free(buf);
}

char *DataCell::ensureSpace(const size_t idx, const size_t space)
{
	const size_t new_cap = used + space;
	if (new_cap > cap) {
		buf = (char *)realloc(buf, new_cap);
		assert(buf);
		cap = new_cap;

		// possibly new pointer, so re-do these:
		for (size_t i = 0; i < idx; i++)
			seqs[i] = {getLen(i, SeqData::SEQ), getData(i, SeqData::SEQ)};
	}
	return &buf[used];
}

void DataCell::clear()
{
	used = 0;
}

bool DataCell::readTXT(const size_t idx, ifstream& in)
{
	string seq;
	SAFE(getline(in, seq));
	const auto line_len = seq.length();
	const auto new_cap = (line_len + 1);
	char *bufx = ensureSpace(idx, new_cap);

	seq.copy(bufx, line_len);
	bufx[line_len] = '\0';

	data[idx][SeqData::SEQ] = &bufx[0] - buf;
	lens[idx][SeqData::SEQ] = (seq_int_t)line_len;

	seqs[idx] = {getLen(idx, SeqData::SEQ), getData(idx, SeqData::SEQ)};

	used += new_cap;

	return true;
}

bool DataCell::readFASTQ(const size_t idx, ifstream& in)
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
	char *bufx = ensureSpace(idx, new_cap);

	seq.copy(&bufx[0], seq_len);
	qual.copy(&bufx[seq_len + 1], qual_len);
	ident.copy(&bufx[seq_len + qual_len + 2], ident_len);

	bufx[seq_len] =
	  bufx[seq_len + qual_len + 1] =
	    bufx[seq_len + qual_len + ident_len + 2] = '\0';

	data[idx][SeqData::SEQ]   = &bufx[0] - buf;
	data[idx][SeqData::QUAL]  = &bufx[seq_len + 1] - buf;
	data[idx][SeqData::IDENT] = &bufx[seq_len + qual_len + 2] - buf;

	lens[idx][SeqData::SEQ]   = (seq_int_t)seq_len;
	lens[idx][SeqData::QUAL]  = (seq_int_t)qual_len;
	lens[idx][SeqData::IDENT] = (seq_int_t)ident_len;

	seqs[idx] = {getLen(idx, SeqData::SEQ), getData(idx, SeqData::SEQ)};

	used += new_cap;

	return true;
}

bool DataCell::readFASTA(const size_t idx, ifstream& in)
{
	string ident, line;

	do {
		SAFE(getline(in, ident));
	} while (ident[0] != '>');

	const auto start = in.tellg();
	unsigned seq_len = 0;

	do {
		if (!getline(in, line) || line.empty() || line[0] == '>') {
			in.clear();
			SAFE(in.seekg(start));
			break;
		} else {
			seq_len += line.size();
		}
	} while (true);

	const auto ident_len = ident.size();
	const auto new_cap = ident_len + (seq_len + 1);
	char *bufx = ensureSpace(idx, new_cap);

	ident.copy(&bufx[0], ident_len - 1, 1);
	bufx[ident_len] = '\0';
	char *next = &bufx[ident_len + 1];

	do {
		const auto here = in.tellg();
		if (!getline(in, line) || line.empty() || line[0] == '>') {
			if (in.good())  // don't read anything from the next sequence
				in.seekg(here);

			break;
		} else {
			const auto len = line.length();
			line.copy(next, len);
			next[len] = '\0';
			next += len;
		}
	} while (true);

	data[idx][SeqData::IDENT] = &bufx[0] - buf;
	data[idx][SeqData::SEQ]   = &bufx[ident_len + 1] - buf;

	lens[idx][SeqData::IDENT] = (seq_int_t)ident_len;
	lens[idx][SeqData::SEQ]   = (seq_int_t)seq_len;

	seqs[idx] = {getLen(idx, SeqData::SEQ), getData(idx, SeqData::SEQ)};

	used += new_cap;

	return true;
}

bool DataCell::read(const size_t idx, ifstream& in, const Format fmt)
{
	switch (fmt) {
		case Format::TXT:
			return readTXT(idx, in);
		case Format::FASTQ:
			return readFASTQ(idx, in);
		case Format::FASTA:
			return readFASTA(idx, in);
		case Format::SAM:
			// TODO
		case Format::BAM:
			// TODO
		default:
			break;
	}

	assert(false);
	return false;
}

bool DataCell::read(vector<ifstream *>& ins, const Format fmt)
{
	size_t idx = 0;
	clear();

	for (auto in : ins) {
		if (!read(idx++, *in, fmt))
			return false;
	}

	return true;
}

io::DataBlock::DataBlock(const size_t cap) : len(0), cap(cap), last(false)
{
}

io::DataBlock::DataBlock() : DataBlock(io::DEFAULT_BLOCK_SIZE)
{
}

void io::DataBlock::read(vector<ifstream *>& ins, Format fmt)
{
	for (len = 0; len < cap; len++) {
		if (!block[len].read(ins, fmt))
			break;
	}

	last = false;
	for (auto *in : ins) {
		if (in->eof()) {
			last = true;
			break;
		}
	}
}
