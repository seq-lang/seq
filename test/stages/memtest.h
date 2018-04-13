#ifndef SEQ_MEMTEST_H
#define SEQ_MEMTEST_H

struct MemTest : ::testing::TestWithParam<std::string> {
	SeqModule s;
	arr_t<> arrGot;

	MemTest() :
	    ::testing::TestWithParam<std::string>(),
	    s(), arrGot({-1, nullptr})
	{
		s.source(GetParam());
	}
};

TEST_P(MemTest, IntMem)
{
	Var m = s | Int[100];
	s | range(100) | m[_];
	m | capture(&arrGot);

	for (seq_int_t i = 0; i < arrGot.len; i++)
		EXPECT_EQ(((seq_int_t *)arrGot.arr)[i], i);
}

INSTANTIATE_TEST_CASE_P(MemTestDefault,
                        MemTest,
                        ::testing::Values(TEST_INPUTS_SINGLE));

#endif /* SEQ_MEMTEST_H */
