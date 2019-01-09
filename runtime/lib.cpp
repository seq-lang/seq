#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <unwind.h>
#include <gc.h>
#include "lib.h"
#include "secure/socket.h"

using namespace std;


/*
 * General
 */

void seq_exc_init();

SEQ_FUNC void seq_init(seq_int_t flags)
{
	GC_INIT();

	seq_exc_init();

	if ((uint64_t)flags & SEQ_FLAG_FASTIO) {
		ios_base::sync_with_stdio(false);
		cin.tie(nullptr);
	}
}

SEQ_FUNC void seq_assert(bool check, seq_str_t file, seq_int_t line)
{
	if (!check) {
		std::cerr << "assertion failed on line " << line << " (" << file.str << ")" << std::endl;
		exit(EXIT_FAILURE);
	}
}


/*
 * GC
 */

SEQ_FUNC void *seq_alloc(size_t n)
{
	return GC_MALLOC(n);
}

SEQ_FUNC void *seq_alloc_atomic(size_t n)
{
	return GC_MALLOC_ATOMIC(n);
}

SEQ_FUNC void *seq_realloc(void *p, size_t n)
{
	return GC_REALLOC(p, n);
}

SEQ_FUNC void seq_register_finalizer(void *p, void (*f)(void *obj, void *data))
{
	GC_REGISTER_FINALIZER(p, f, nullptr, nullptr, nullptr);
}


/*
 * Printing
 */

SEQ_FUNC void seq_print_int(seq_int_t n)
{
	cout << n;
}

SEQ_FUNC void seq_print_float(double f)
{
	cout << f;
}

SEQ_FUNC void seq_print_bool(bool b)
{
	cout << (b ? "True" : "False");
}

SEQ_FUNC void seq_print_byte(char c)
{
	cout << c;
}

SEQ_FUNC void seq_print_str(seq_str_t str)
{
	cout.write(str.str, str.len);
}

SEQ_FUNC void seq_print_seq(seq_t seq)
{
	cout.write(seq.seq, seq.len);
}

SEQ_FUNC void seq_print_ptr(void *p)
{
	if (p)
		cout << p;
	else
		cout << "None";
}

SEQ_FUNC void seq_print_base_2bit(uint8_t base)
{
	static const char table[] = {'A','C','G','T'};
	cout << table[base];
}


/*
 * General I/O
 */

void error(const string& msg)
{
	cerr << "I/O error: " << msg << endl;
	exit(EXIT_FAILURE);
}

static inline void *seqio_openx(const char *filename, const char *mode)
{
	void *fp = fopen(filename, mode);

	if (!fp)
		error("unable to open file '" + string(filename) + "'");

	return fp;
}

SEQ_FUNC void *seq_io_openr(const char *filename)
{
	return seqio_openx(filename, "rb");
}

SEQ_FUNC void *seq_io_openw(const char *filename)
{
	return seqio_openx(filename, "wb");
}

SEQ_FUNC void seq_io_close(void *fp)
{
	if (fclose((FILE *)fp) != 0)
		error("unable to close file");
}

SEQ_FUNC void seq_io_read(void *ptr,
                          seq_int_t size,
                          seq_int_t nmemb,
                          void *fp)
{
	if ((seq_int_t)fread(ptr, (size_t)size, (size_t)nmemb, (FILE *)fp) != nmemb)
		error("unable to read from file");
}

SEQ_FUNC void seq_io_write(const void *ptr,
                           seq_int_t size,
                           seq_int_t nmemb,
                           void *fp)
{
	if ((seq_int_t)fwrite(ptr, (size_t)size, (size_t)nmemb, (FILE *)fp) != nmemb)
		error("unable to write to file");
}


/*
 * Bioinformatics-related I/O
 */

static const int DEFAULT_BLOCK_SIZE = 1000;
static const int MAX_INPUTS = 5;

enum Format {
	TXT,
	FASTQ,
	FASTA,
	SAM,
	BAM
};

enum SeqData {
	SEQ,
	QUAL,
	IDENT,
	SEQ_DATA_COUNT
};

extern const map<string, Format> EXT_CONV;

struct DataCell {
	char *buf;
	size_t used;
	size_t cap;
	array<array<ptrdiff_t, SeqData::SEQ_DATA_COUNT>, MAX_INPUTS> data;
	array<array<seq_int_t, SeqData::SEQ_DATA_COUNT>, MAX_INPUTS> lens;
	array<seq_t, MAX_INPUTS> seqs;

	DataCell();

	bool read(vector<ifstream *>& ins, Format fmt);
	void clear();

	inline char *getData(size_t idx, SeqData key)
	{
		return buf + data[idx][key];
	}

	inline seq_int_t getLen(size_t idx, SeqData key)
	{
		return lens[idx][key];
	}

	inline seq_t getSeq(size_t idx=0)
	{
		return seqs[0];
	}

	seq_arr_t<seq_t> getSeqs(size_t count);
private:
	DataCell(char *buf, size_t cap);
	char *ensureSpace(size_t idx, size_t space);

	bool read(size_t idx, ifstream& in, Format fmt);
	bool readTXT(size_t idx, ifstream& in);
	bool readFASTQ(size_t idx, ifstream& in);
	bool readFASTA(size_t idx, ifstream& in);
};

struct DataBlock {
	DataCell block[DEFAULT_BLOCK_SIZE];
	size_t len;
	const size_t cap;
	bool last;

	explicit DataBlock(size_t cap);
	DataBlock();

	void read(vector<ifstream *>& ins, Format fmt);
};

Format extractExt(const string& source)
{
	auto fmtIter = EXT_CONV.find(source.substr(source.find_last_of('.') + 1));

	if (fmtIter == EXT_CONV.end())
		error("unknown file extension in '" + source + "'");

	return fmtIter->second;
}

#define SAFE(x) if (!(x)) return false;

static map<string, Format> makeExtConvMap() noexcept
{
	return {{"txt",   Format::TXT},
	        {"fastq", Format::FASTQ},
	        {"fq",    Format::FASTQ},
	        {"fasta", Format::FASTA},
	        {"fa",    Format::FASTA},
	        {"sam",   Format::SAM},
	        {"bam",   Format::FASTQ}};
};

const map<string, Format> EXT_CONV = makeExtConvMap();

DataCell::DataCell(char *buf, const size_t cap) :
    buf(buf), used(0), cap(cap), data(), lens(), seqs()
{
}

DataCell::DataCell() :
    DataCell(nullptr, 0)
{
}

char *DataCell::ensureSpace(const size_t idx, const size_t space)
{
	const size_t new_cap = used + space;
	if (new_cap > cap) {
		buf = (char *)seq_realloc(buf, new_cap);
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
	buf = (char *)seq_alloc_atomic(cap);
	used = 0;
}

seq_arr_t<seq_t> DataCell::getSeqs(const size_t count)
{
	const size_t bytes = count * sizeof(seq_t);
	auto *data = (seq_t *)seq_alloc(bytes);
	memcpy(data, seqs.data(), bytes);
	return {(seq_int_t)count, data};
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

	assert(0);
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

struct IOState {
	DataBlock data;
	vector<ifstream *> ins;
	Format fmt;

	IOState() :
	    data(), ins(), fmt()
	{
	}

	void setSource(const std::string& source)
	{
		fmt = extractExt(source);
		ins.push_back(new ifstream(source));
		if (!ins.back()->good())
			error("could not open '" + source + "' for reading");
	}

	void close()
	{
		for (auto *in : ins) {
			in->close();
			delete in;
		}
	}
};

SEQ_FUNC void *seq_source_new()
{
	auto *state = (IOState *)seq_alloc(sizeof(IOState));
	new (state) IOState();
	return state;
}

SEQ_FUNC void seq_source_init(void *state, seq_str_t source)
{
	auto *ioState = (IOState *)state;
	ioState->setSource(string(source.str, (unsigned long)source.len));
}

SEQ_FUNC seq_int_t seq_source_read(void *state)
{
	auto *ioState = (IOState *)state;
	ioState->data.read(ioState->ins, ioState->fmt);
	return (seq_int_t)ioState->data.len;
}

SEQ_FUNC seq_arr_t<seq_t> seq_source_get(void *state, seq_int_t idx)
{
	auto *ioState = (IOState *)state;
	return ioState->data.block[idx].getSeqs(ioState->ins.size());
}

SEQ_FUNC seq_t seq_source_get_single(void *state, seq_int_t idx)
{
	auto *ioState = (IOState *)state;
	return ioState->data.block[idx].getSeq();
}

SEQ_FUNC void seq_source_dealloc(void *state)
{
	auto *ioState = (IOState *)state;
	ioState->close();
}

/************************************************************************/

struct RawInput {
	FILE *f;
	char *buf;
	size_t n;
	RawInput(): f(nullptr), buf(nullptr), n(0) {}
};

SEQ_FUNC void *seq_raw_new()
{
	auto *state = (RawInput *)seq_alloc(sizeof(RawInput));
	new (state) RawInput();
	return state;
}

SEQ_FUNC void seq_raw_init(void *st, seq_str_t source)
{
	auto *state = (RawInput *)st;
	state->f = fopen(string(source.str, (unsigned long)source.len).data(), "r");
}

SEQ_FUNC seq_t seq_raw_read(void *st)
{
	auto *state = (RawInput *)st;
	auto read = getline(&state->buf, &state->n, state->f);
	if (read == -1) {
		if (state->n > 0) {
			state->buf[0] = 0;
		}
		read = 0;
	} else {
		if (state->buf[read - 1] == '\n')
			state->buf[--read] = 0;
	}
	return seq_t {(seq_int_t) read, state->buf};
}

SEQ_FUNC void seq_raw_dealloc(void *st)
{
	auto *state = (RawInput *)st;
	if (state->f)
		fclose(state->f);
	if (state->buf)
		free(state->buf);
}


/*
 * MPC sockets
 */

static seq_str_t string_conv(const std::string& s)
{
	auto len = s.length();
	auto *buf = (char *)seq_alloc_atomic(len);
	std::memcpy(buf, s.data(), len);
	return {(seq_int_t)len, buf};
}

SEQ_FUNC void seq_socket_del(void *sock, void *unused)
{
	((CSocket *)sock)->~CSocket();
}

SEQ_FUNC void *seq_socket_new()
{
	void *sock = seq_alloc(sizeof(CSocket));
	seq_register_finalizer(sock, seq_socket_del);
	return (void *)(new (sock) CSocket());
}

SEQ_FUNC bool seq_socket_socket(void *sock)
{
	return ((CSocket *)sock)->Socket();
}

SEQ_FUNC void seq_socket_close(void *sock)
{
	((CSocket *)sock)->Close();
}

SEQ_FUNC void seq_socket_attach_from(void *sock, void *other)
{
	((CSocket *)sock)->AttachFrom(*(CSocket *)other);
}

SEQ_FUNC void seq_socket_detach(void *sock)
{
	((CSocket *)sock)->Detach();
}

SEQ_FUNC seq_str_t seq_socket_get_ip(void *sock)
{
	return string_conv(((CSocket *)sock)->GetIP());
}

SEQ_FUNC seq_int_t seq_socket_get_port(void *sock)
{
	return ((CSocket *)sock)->GetPort();
}

SEQ_FUNC seq_int_t seq_socket_bind(void *sock, seq_str_t ip, seq_int_t port)
{
	return ((CSocket *)sock)->Bind((uint16_t)port, std::string(ip.str, (size_t)ip.len));
}

SEQ_FUNC bool seq_socket_listen(void *sock, seq_int_t q)
{
	return ((CSocket *)sock)->Listen((int)q);
}

SEQ_FUNC bool seq_socket_accept(void *sock, void *other)
{
	return ((CSocket *)sock)->Accept(*(CSocket *)other);
}

SEQ_FUNC bool seq_socket_connect(void *sock, seq_str_t ip, seq_int_t port, seq_int_t timeout)
{
	return ((CSocket *)sock)->Connect(std::string(ip.str, (size_t)ip.len), (uint16_t)port, (int32_t)timeout);
}

SEQ_FUNC seq_int_t seq_socket_receive(void *sock, char *buf, seq_int_t len, seq_int_t flags)
{
	return ((CSocket *)sock)->Receive(buf, len, (int)flags);
}

SEQ_FUNC seq_int_t seq_socket_send(void *sock, char *buf, seq_int_t len, seq_int_t flags)
{
	return ((CSocket *)sock)->Send(buf, len, (int)flags);
}

SEQ_FUNC seq_int_t seq_socket_receive_secure(void *sock, unsigned char *buf, seq_int_t len, seq_int_t flags)
{
	return ((CSocket *)sock)->ReceiveSecure(buf, len, (int)flags);
}

SEQ_FUNC seq_int_t seq_socket_send_secure(void *sock, unsigned char *buf, seq_int_t len, seq_int_t flags)
{
	return ((CSocket *)sock)->SendSecure(buf, len, (int)flags);
}

SEQ_FUNC seq_int_t seq_socket_get_bytes_received(void *sock)
{
	return ((CSocket *)sock)->GetBytesReceived();
}

SEQ_FUNC seq_int_t seq_socket_get_bytes_sent(void *sock)
{
	return ((CSocket *)sock)->GetBytesSent();
}

SEQ_FUNC void seq_socket_reset_stats(void *sock)
{
	((CSocket *)sock)->ResetStats();
}

SEQ_FUNC bool seq_socket_set_key(void *sock, seq_str_t keyfile)
{
	return ((CSocket *)sock)->SetKey(string(keyfile.str, (size_t)keyfile.len));
}
