#ifndef SEQ_COUNTTEST_H
#define SEQ_COUNTTEST_H

#include "../testhelp.h"

struct CountTest : ::testing::TestWithParam<std::string> {
	SeqModule s;
	seq_int_t countGot;

	CountTest() :
	    ::testing::TestWithParam<std::string>(),
	    s(), countGot(-1)
	{
		s.source(GetParam());
	}
};

TEST_P(CountTest, PositiveCount)
{
	Var x = s | split(10,10) | count();
	x | capture(&countGot);
	s.execute();
	EXPECT_EQ(countGot, (s.data->block[0].seqs.data()[0].len)/10);
}

TEST_P(CountTest, ZeroCount)
{
	Var x = s | split(99999,1) | count();
	x | capture(&countGot);
	s.execute();
	EXPECT_EQ(countGot, 0);
}

INSTANTIATE_TEST_CASE_P(PositiveCountDefault,
                        CountTest,
                        ::testing::Values(TEST_INPUTS_SINGLE));

INSTANTIATE_TEST_CASE_P(ZeroCountDefault,
                        CountTest,
                        ::testing::Values(TEST_INPUTS_SINGLE));

#endif /* SEQ_COUNTTEST_H */
