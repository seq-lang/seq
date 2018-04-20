#ifndef SEQ_BRANCHTEST_H
#define SEQ_BRANCHTEST_H

#include "../testhelp.h"

TEST(BranchTest, SimpleBranch)
{
	SeqModule s;
	seq_int_t got1 = -1, got2 = -1;

	s & (len() | capture(&got1), substr(1,10) | len() | capture(&got2));

	s.source(DEFAULT_TEST_INPUT_SINGLE);
	s.execute();

	EXPECT_EQ(got1, s.data->block[0].seqs.data()[0].len);
	EXPECT_EQ(got2, 10);
}

TEST(BranchTest, SeqBranch)
{
	SeqModule s;
	arr_t<seq_t> arrGot = {-1, nullptr};

	Var v = s | range(10) | (s && (copy(), collect()));
	v | capture(&arrGot);

	s.source(DEFAULT_TEST_INPUT_SINGLE);
	s.execute();

	ASSERT_EQ(arrGot.len, 10);
	for (seq_int_t i = 0; i < arrGot.len; i++) {
		EXPECT_TRUE(util::eq(arrGot.arr[i].seq,
		                     arrGot.arr[i].len,
		                     s.data->block[0].seqs.data()[0].seq,
		                     s.data->block[0].seqs.data()[0].len));
	}
}

TEST(BranchTest, VarBranch)
{
	SeqModule s;
	arr_t<seq_t> arrGot = {-1, nullptr};

	Var t = s | copy();
	Var v = s | range(10) | (t && (copy(), collect()));
	v | capture(&arrGot);

	s.source(DEFAULT_TEST_INPUT_SINGLE);
	s.execute();

	ASSERT_EQ(arrGot.len, 10);
	for (seq_int_t i = 0; i < arrGot.len; i++) {
		EXPECT_TRUE(util::eq(arrGot.arr[i].seq,
		                     arrGot.arr[i].len,
		                     s.data->block[0].seqs.data()[0].seq,
		                     s.data->block[0].seqs.data()[0].len));
	}
}

TEST(BranchTest, FuncBranch)
{
	SeqModule s;
	arr_t<seq_t> arrGot = {-1, nullptr};

	Func f(Seq, Void);
	Var v = f | range(10) | (f && (copy(), collect()));
	v | capture(&arrGot);
	s | f();

	s.source(DEFAULT_TEST_INPUT_SINGLE);
	s.execute();

	ASSERT_EQ(arrGot.len, 10);
	for (seq_int_t i = 0; i < arrGot.len; i++) {
		EXPECT_TRUE(util::eq(arrGot.arr[i].seq,
		                     arrGot.arr[i].len,
		                     s.data->block[0].seqs.data()[0].seq,
		                     s.data->block[0].seqs.data()[0].len));
	}
}

#endif /* SEQ_BRANCHTEST_H */
