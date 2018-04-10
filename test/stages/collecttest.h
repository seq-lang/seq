#ifndef SEQ_CAPTURETEST_H
#define SEQ_CAPTURETEST_H

#include "../testhelp.h"

struct CollectTest : ::testing::TestWithParam<std::string> {
	SeqModule s;
	arr_t<> arrGot;

	CollectTest() :
	    ::testing::TestWithParam<std::string>(),
	    s(), arrGot({-1, nullptr})
	{
		s.source(GetParam());
	}
};

TEST_P(CollectTest, IntCollect)
{
	Var x = s | range(100) | collect();
	x | capture(&arrGot);
	s.execute();
	auto *arr = (seq_int_t *)arrGot.arr;

	ASSERT_EQ(arrGot.len, 100);
	for (seq_int_t i = 0; i < arrGot.len; i++)
		EXPECT_EQ(arr[i], i);
}

TEST_P(CollectTest, SeqCollect)
{
	Var x = s | split(10,10) | collect();
	x | capture(&arrGot);
	s.execute();
	EXPECT_EQ(arrGot.len, s.data->block[0].seqs.data()[0].len/10);
	EXPECT_EQ(((seq_t *)arrGot.arr)[0].len, 10);
}

TEST_P(CollectTest, ArrCollect)
{
	Var x = s | split(10,10) | collect();
	Var y = x | collect();
	y | capture(&arrGot);
	s.execute();
	EXPECT_EQ(((arr_t<> *)arrGot.arr)[0].len,
	          s.data->block[0].seqs.data()[0].len/10);
}

TEST_P(CollectTest, EmptyCollect)
{
	Var x = s | split(99999,1) | collect();
	x | capture(&arrGot);
	s.execute();
	EXPECT_EQ(arrGot.len, 0);
}

INSTANTIATE_TEST_CASE_P(CollectTestDefault,
                        CollectTest,
                        ::testing::Values(TEST_INPUTS_SINGLE));

#endif /* SEQ_CAPTURETEST_H */
