#ifndef SEQ_CALLTEST_H
#define SEQ_CALLTEST_H

#include "../testhelp.h"

namespace calltest {
	static bool call1 = false;
	static bool call2 = false;
	static bool call3 = false;
	static bool call4 = false;
	static bool call5 = false;

	SEQ_FUNC seq_int_t testFunc1(seq_int_t n)
	{
		call1 = true;
		return n + 1;
	}

	SEQ_FUNC arr_t<> testFunc2(seq_t s)
	{
		call2 = true;
		return {0, nullptr};
	}

	SEQ_FUNC void testFunc3(seq_int_t n)
	{
		call3 = true;
	}

	SEQ_FUNC seq_int_t testFunc4()
	{
		call4 = true;
		return 42;
	}

	SEQ_FUNC void testFunc5()
	{
		call5 = true;
	}

	static void reset()
	{
		call1 = call2 = call3 = call4 = call5 = false;
	}
}

TEST(CallTestIntInt, CallTest)
{
	calltest::reset();
	seq_int_t got = -1;

	SeqModule s;
	Func f(Int, Int, "testFunc1", (void *)calltest::testFunc1);
	s | count() | f() | capture(&got);
	s.source(DEFAULT_TEST_INPUT);
	s.execute();

	EXPECT_TRUE(calltest::call1);
	EXPECT_EQ(got, 2);
}

TEST(CallTestArrSeq, CallTest)
{
	calltest::reset();
	seq_int_t got = -1;

	SeqModule s;
	Func f(Seq, Array.of(Int), "testFunc2", (void *)calltest::testFunc2);
	s | f() | len() | capture(&got);
	s.source(DEFAULT_TEST_INPUT);
	s.execute();

	EXPECT_TRUE(calltest::call2);
	EXPECT_EQ(got, 0);
}

TEST(CallTestIntVoid, CallTest)
{
	calltest::reset();

	SeqModule s;
	Func f(Int, Void, "testFunc3", (void *)calltest::testFunc3);
	s | count() | f();
	s.source(DEFAULT_TEST_INPUT);
	s.execute();

	EXPECT_TRUE(calltest::call3);
}

TEST(CallTestVoidInt, CallTest)
{
	calltest::reset();
	seq_int_t got = -1;

	SeqModule s;
	Func f(Void, Int, "testFunc4", (void *)calltest::testFunc4);
	s | f() | capture(&got);
	s.source(DEFAULT_TEST_INPUT);
	s.execute();

	EXPECT_TRUE(calltest::call4);
	EXPECT_EQ(got, 42);
}

TEST(CallTestVoidVoid, CallTest)
{
	calltest::reset();

	SeqModule s;
	Func f(Void, Void, "testFunc5", (void *)calltest::testFunc5);
	s | f();
	s.source(DEFAULT_TEST_INPUT);
	s.execute();

	EXPECT_TRUE(calltest::call5);
}

#endif /* SEQ_CALLTEST_H */
