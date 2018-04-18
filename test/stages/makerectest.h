#ifndef SEQ_MAKERECTEST_H
#define SEQ_MAKERECTEST_H

#include "../testhelp.h"

namespace makerectest {
	struct rec_t {
		seq_int_t n;
		seq_t s1;
		seq_t s2;
		seq_t s3;
	};
}

struct MakeRecTest : ::testing::TestWithParam<std::string> {
	SeqModule s;
	arr_t<> arrGot;

	MakeRecTest() :
	    ::testing::TestWithParam<std::string>(),
	    s(), arrGot({-1, nullptr})
	{
		s.source(GetParam());
	}
};

TEST_P(MakeRecTest, ComplexMakeRec)
{
	Var v = s | split(32,1) | (count(), _, copy(), copy() | split(32,1)) | collect();
	v | capture(&arrGot);
	s.execute();

	char *seq = s.data->block[0].seqs.data()[0].seq;
	seq_int_t len = s.data->block[0].seqs.data()[0].len;

	EXPECT_EQ(arrGot.len, len - 32 + 1);
	for (seq_int_t i = 0; i < arrGot.len; i++) {
		makerectest::rec_t rec = ((makerectest::rec_t *)arrGot.arr)[i];
		EXPECT_EQ(rec.n, i+1);
		EXPECT_TRUE(util::eq(rec.s1.seq, rec.s1.len, &seq[i], 32));
		EXPECT_TRUE(util::eq(rec.s1.seq, rec.s1.len, rec.s2.seq, rec.s2.len));
		EXPECT_TRUE(util::eq(rec.s1.seq, rec.s1.len, rec.s3.seq, rec.s3.len));
	}
}

INSTANTIATE_TEST_CASE_P(MakeRecTestDefault,
                        MakeRecTest,
                        ::testing::Values(TEST_INPUTS_SINGLE));

#endif /* SEQ_MAKERECTEST_H */
