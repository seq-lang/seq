#ifndef SEQ_CHUNKTEST_H
#define SEQ_CHUNKTEST_H

#include "../testhelp.h"

struct ChunkTest : ::testing::TestWithParam<std::string> {
	SeqModule s;
	arr_t<> arrGot;

	ChunkTest() :
	    ::testing::TestWithParam<std::string>(),
	    s(), arrGot({-1, nullptr})
	{
		s.source(GetParam());
	}
};

TEST_P(ChunkTest, IntChunk)
{
	Func f(Int, Int);
	Lambda v;
	f | lambda(v/10);

	Var x = s | range(100) | collect();
	Var y = x | chunk(f) | collect();
	y | capture(&arrGot);
	s.execute();

	ASSERT_EQ(arrGot.len, 10);
	for (seq_int_t i = 0; i < arrGot.len; i++) {
		arr_t<seq_int_t> arr = ((arr_t<seq_int_t> *)arrGot.arr)[i];
		ASSERT_EQ(arr.len, 10);

		for (seq_int_t j = 0; j < arr.len; j++)
			EXPECT_EQ(arr.arr[j], i*10 + j);
	}
}

TEST_P(ChunkTest, SeqChunk)
{
	Func f(Seq, Seq);
	f | substr(1,1);
	Var x = s | split(2,2) | collect();
	Var y = x | chunk(f) | collect();
	y | capture(&arrGot);
	s.execute();

	for (seq_int_t i = 0; i < arrGot.len; i++) {
		arr_t<seq_t> arr = ((arr_t<seq_t> *)arrGot.arr)[i];

		for (seq_int_t j = 1; j < arr.len; j++)
			EXPECT_EQ(arr.arr[j].seq[0], arr.arr[0].seq[0]);
	}
}

INSTANTIATE_TEST_CASE_P(ChunkTestDefault,
                        ChunkTest,
                        ::testing::Values(TEST_INPUTS_SINGLE));

#endif /* SEQ_CHUNKTEST_H */
