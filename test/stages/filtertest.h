#ifndef SEQ_FILTERTEST_H
#define SEQ_FILTERTEST_H

#include "../testhelp.h"

SEQ_FUNC bool filterTestFunc1(seq_int_t n)
{
	return n%2 == 0;
}

SEQ_FUNC bool filterTestFunc2(seq_t seq)
{
	return seq.len > 0 && seq.seq[0] == 'A';
}

struct FilterTest : ::testing::TestWithParam<std::string> {
	SeqModule s;
	arr_t<> arrGot;

	FilterTest() :
	    ::testing::TestWithParam<std::string>(),
	    s(), arrGot({-1, nullptr})
	{
		s.source(GetParam());
	}
};

TEST_P(FilterTest, IntFilter)
{
	Func f(Int, Bool, SEQ_NATIVE(filterTestFunc1));
	Var x = s | range(20) | filter(f) | collect();
	x | capture(&arrGot);
	s.execute();

	for (seq_int_t i = 0; i < arrGot.len; i++)
		EXPECT_EQ(((seq_int_t *)arrGot.arr)[i] % 2, 0);
}

TEST_P(FilterTest, SeqFilter)
{
	Func f(Seq, Bool, SEQ_NATIVE(filterTestFunc2));
	Var x = s | split(5,5) | filter(f) | collect();
	x | capture(&arrGot);
	s.execute();

	for (seq_int_t i = 0; i < arrGot.len; i++)
		EXPECT_EQ(((seq_t *)arrGot.arr)[i].seq[0], 'A');
}

INSTANTIATE_TEST_CASE_P(FilterTestDefault,
                        FilterTest,
                        ::testing::Values(TEST_INPUTS_SINGLE));

#endif /* SEQ_FILTERTEST_H */
