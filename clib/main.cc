/// 786

#include <map>
#include <array>
#include <vector>
#include <fstream>
#include <iostream>
using namespace std;

#define SEQ_FUNC extern "C"
#define O stdout

#define SAFE(x) if (!(x)) return false;
typedef int64_t seq_int_t;

static const size_t DEFAULT_BLOCK_SIZE = 1000;
static const size_t MAX_INPUTS = 5;

struct seq_t {
    seq_int_t len;
    char *seq;
};

enum Format {
    TXT, FASTQ, FASTA, SAM, BAM
};

enum SeqData {
    NONE,
    SEQ, LEN, QUAL, IDENT,
    INT, FLOAT, BOOL, STR, ARRAY, RECORD,
    SEQ_DATA_COUNT
};

const map<string, Format> EXT_CONV = {
    {"txt",   Format::TXT},
    {"fastq", Format::FASTQ},
    {"fq",    Format::FASTQ},
    {"fasta", Format::FASTA},
    {"fa",    Format::FASTA},
    {"sam",   Format::SAM},
    {"bam",   Format::FASTQ}
};

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

struct DataBlock {
    DataCell block[DEFAULT_BLOCK_SIZE];
    size_t len;
    const size_t cap;
    bool last;

    explicit DataBlock(size_t cap);
    DataBlock();

    void read(std::vector<std::ifstream *>& ins, Format fmt);
};

DataBlock::DataBlock(const size_t cap) : len(0), cap(cap), last(false)
{
}

DataBlock::DataBlock() : DataBlock(DEFAULT_BLOCK_SIZE)
{
}

void DataBlock::read(vector<ifstream *>& ins, Format fmt)
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

Format extractExt(const std::string& source)
{
	auto fmtIter = EXT_CONV.find(source.substr(source.find_last_of('.') + 1));

	if (fmtIter == EXT_CONV.end())
		throw string("unknown file extension in '" + source + "'");

	return fmtIter->second;
}


static inline void ioError(const std::string &msg)
{
    fprintf(stderr, "IO error: %s", msg.c_str());
    abort();
}

struct IOState
{
    DataBlock data;
    std::vector<std::ifstream *> ins;
    Format fmt;

    IOState(char **sources, const seq_int_t numSources): 
        data(), ins()
    {
        if (numSources == 0)
            ioError("sequence source not specified");

        if (numSources > MAX_INPUTS)
            ioError("too many inputs (max: " + std::to_string(MAX_INPUTS) + ")");

        fmt = extractExt(sources[0]);

        for (seq_int_t i = 1; i < numSources; i++)
        {
            if (extractExt(sources[i]) != fmt)
                ioError("inconsistent input formats");
        }

        for (seq_int_t i = 0; i < numSources; i++)
        {
            ins.push_back(new std::ifstream(sources[i]));
            if (!ins.back()->good())
                ioError("could not open '" + std::string(sources[i]) + "' for reading");
        }
    }

    ~IOState()
    {
        for (auto *in : ins)
        {
            in->close();
            delete in;
        }
    }
};

SEQ_FUNC void *seq_init(char *source)
{
    char *sources[1] = {source};
    return new IOState(sources, 1);
}

SEQ_FUNC seq_int_t seq_read(void *state)
{
    auto *ioState = (IOState *)state;
    ioState->data.read(ioState->ins, ioState->fmt);
    return (seq_int_t)ioState->data.len;
}

SEQ_FUNC seq_t seq_get(void *state, seq_int_t idx)
{
    auto *ioState = (IOState *)state;
    return ioState->data.block[idx].seqs.data()[0];
}

SEQ_FUNC void seq_free(void *state)
{
    delete (IOState *)state;
}

SEQ_FUNC void print_i(int64_t i)
{
    fprintf(O, "[C] %lld\n", i);
}

SEQ_FUNC void print_f(double i)
{
    fprintf(O, "[C] %.1lf\n", i);
}

SEQ_FUNC void print_s(seq_t s)
{
    fprintf(O, "[C] ");
    for (int64_t i = 0; i < s.len; i++)
        putc(s.seq[i], O);
    putc('\n', O);
}
