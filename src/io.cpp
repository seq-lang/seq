#include <cstdint>
#include <iostream>
#include "io.h"

using namespace seq;

io::SeqData::SeqData(char *data, const uint32_t len) :
    data(data), len(len)
{
}

io::SeqData::SeqData() : SeqData(nullptr, 0)
{
}

io::SeqData::~SeqData()
{
	delete[] data;
}

bool io::SeqData::read(std::ifstream &in)
{
	std::string line;

	if (std::getline(in, line)) {
		const auto new_len = (uint32_t)line.length();

		if (new_len > len) {
			auto *old_data = data;
			data = new char[new_len + 1];
			delete[] old_data;
		}

		len = new_len;
		line.copy(data, len);
		data[len] = '\0';
		return true;
	} else {
		return false;
	}
}

io::DataBlock::DataBlock(const size_t cap) : len(0), cap(cap)
{
}

io::DataBlock::DataBlock() : DataBlock(io::DEFAULT_BLOCK_SIZE)
{
}

void io::DataBlock::read(std::ifstream& in)
{
	for (len = 0; len < cap; len++) {
		if (!block[len].read(in))
			break;
	}
}
