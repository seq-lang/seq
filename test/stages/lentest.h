#ifndef SEQ_LENTEST_H
#define SEQ_LENTEST_H

#include "../testhelp.h"

struct LenTest : ::testing::TestWithParam<std::string> {
	SeqModule s;
	seq_int_t lenGot;

	LenTest() :
	    ::testing::TestWithParam<std::string>(),
	    s(), lenGot(-1)
	{
		s.source(GetParam());
	}
};

TEST_P(LenTest, PositiveLen)
{
	s | len() | capture(&lenGot);
	s.execute();
	EXPECT_EQ(lenGot, s.data->block[0].seqs.data()[0].len);
}

TEST_P(LenTest, ZeroLen)
{
	s | substr(1,0) | len() | capture(&lenGot);
	s.execute();
	EXPECT_EQ(lenGot, 0);
}

INSTANTIATE_TEST_CASE_P(LenTestDefault,
                        LenTest,
                        ::testing::Values(TEST_INPUTS_SINGLE));

#endif /* SEQ_LENTEST_H */
