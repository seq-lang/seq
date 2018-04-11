#ifndef SEQ_COPYTEST_H
#define SEQ_COPYTEST_H

#include "../testhelp.h"

struct CopyTest : ::testing::TestWithParam<std::string> {
	SeqModule s;

	CopyTest() :
	    ::testing::TestWithParam<std::string>(),
	    s()
	{
		s.source(GetParam());
	}
};

TEST_P(CopyTest, ArrCopy)
{
	arr_t<seq_int_t> arr1 = {0, nullptr};
	arr_t<seq_int_t> arr2 = {-1, nullptr};

	Var x = s | range(10) | collect();
	x | capture(&arr1);
	x | copy() | capture(&arr2);
	s.execute();

	ASSERT_EQ(arr1.len, arr2.len);
	for (seq_int_t i = 0; i < arr1.len; i++)
		EXPECT_EQ(arr1.arr[i], arr2.arr[i]);
}

TEST_P(CopyTest, SeqCopy)
{
	seq_t seq1 = {0, nullptr};
	seq_t seq2 = {-1, nullptr};

	s | capture(&seq1);
	s | copy() | capture(&seq2);
	s.execute();

	ASSERT_EQ(seq1.len, seq2.len);
	for (seq_int_t i = 0; i < seq1.len; i++)
		EXPECT_EQ(seq1.seq[i], seq2.seq[i]);
}

INSTANTIATE_TEST_CASE_P(CopyTestDefault,
                        CopyTest,
                        ::testing::Values(TEST_INPUTS_SINGLE));

#endif /* SEQ_COPYTEST_H */
