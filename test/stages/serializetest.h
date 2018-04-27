#ifndef SEQ_SERIALIZETEST_H
#define SEQ_SERIALIZETEST_H

#include "../testhelp.h"

#define SER_TEST_FILE TEST_SERIALIZE_DIR "test.dat"

TEST(SerTestInt, SerTest)
{
	seq_int_t intGot1 = -1;
	seq_int_t intGot2 = -1;
	seq_int_t intGot3 = -1;

	SeqModule s;
	Lambda x;
	s | count() | lambda(x + 41) | ser(SER_TEST_FILE);
	s | deser(Int, SER_TEST_FILE) | capture(&intGot1);
	s | deser(Int, SER_TEST_FILE) | capture(&intGot2);
	s.source(DEFAULT_TEST_INPUT_SINGLE);
	s.execute();

	{
		SeqModule s;
		s | deser(Int, SER_TEST_FILE) | capture(&intGot3);
		s.source(DEFAULT_TEST_INPUT_SINGLE);
		s.execute();
	}

	EXPECT_EQ(intGot1, 42);
	EXPECT_EQ(intGot2, 42);
	EXPECT_EQ(intGot3, 42);
}

TEST(SerTestSeq, SerTest)
{
	seq_t seqGot1 = {-1, nullptr};
	seq_t seqGot2 = {-1, nullptr};
	seq_t seqGot3 = {-1, nullptr};

	SeqModule s;
	s | ser(SER_TEST_FILE);
	s | deser(Seq, SER_TEST_FILE) | capture(&seqGot1);
	s | deser(Seq, SER_TEST_FILE) | capture(&seqGot2);
	s.source(DEFAULT_TEST_INPUT_SINGLE);
	s.execute();

	{
		SeqModule s;
		s | deser(Seq, SER_TEST_FILE) | capture(&seqGot3);
		s.source(DEFAULT_TEST_INPUT_SINGLE);
		s.execute();
	}

	char *seq = s.data->block[0].seqs.data()[0].seq;
	seq_int_t len = s.data->block[0].seqs.data()[0].len;

	EXPECT_TRUE(util::eq(seqGot1.seq, seqGot1.len, seq, len));
	EXPECT_TRUE(util::eq(seqGot2.seq, seqGot2.len, seq, len));
	EXPECT_TRUE(util::eq(seqGot3.seq, seqGot3.len, seq, len));
}

TEST(SerTestIntSeqRecArrayArray, SerTest)
{
	struct rec_t {
		seq_int_t i;
		seq_t s;
	};

	arr_t<arr_t<rec_t>> arrGot = {-1, nullptr};

	SeqModule s;
	Var a = s | split(32,1) | (count(), _) | collect();
	Var b = a | collect();
	b | ser(SER_TEST_FILE);
	s | deser(Array.of(Array.of(Record.of({Int, Seq}))), SER_TEST_FILE) | capture(&arrGot);
	s.source(DEFAULT_TEST_INPUT_SINGLE);
	s.execute();

	char *seq = s.data->block[0].seqs.data()[0].seq;
	seq_int_t len = s.data->block[0].seqs.data()[0].len;

	ASSERT_EQ(arrGot.len, 1);
	arr_t<rec_t> arrInner = arrGot.arr[0];
	ASSERT_EQ(arrInner.len, len - 32 + 1);

	for (seq_int_t i = 0; i < arrGot.len; i++) {
		EXPECT_EQ(arrInner.arr[i].i, i+1);
		EXPECT_TRUE(util::eq(arrInner.arr[i].s.seq,
		                     arrInner.arr[i].s.len,
		                     &seq[i],
		                     32));
	}
}

#endif /* SEQ_SERIALIZETEST_H */
