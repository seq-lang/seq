#ifndef SEQ_CALLTEST_H
#define SEQ_CALLTEST_H

#include "../testhelp.h"

namespace calltest {
	static bool call1 = false;
	static bool call2 = false;
	static bool call3 = false;
	static bool call4 = false;
	static bool call5 = false;

	static void reset()
	{
		call1 = call2 = call3 = call4 = call5 = false;
	}
}

SEQ_FUNC seq_int_t callTestFunc1(seq_int_t n)
{
	calltest::call1 = true;
	return n + 1;
}

SEQ_FUNC arr_t<> callTestFunc2(seq_t s)
{
	calltest::call2 = true;
	return {0, nullptr};
}

SEQ_FUNC void callTestFunc3(seq_int_t n)
{
	calltest::call3 = true;
}

SEQ_FUNC seq_int_t callTestFunc4()
{
	calltest::call4 = true;
	return 42;
}

SEQ_FUNC void callTestFunc5()
{
	calltest::call5 = true;
}

TEST(CallTestIntInt, CallTest)
{
	calltest::reset();
	seq_int_t got = -1;

	SeqModule s;
	Func f(Int, Int, SEQ_NATIVE(callTestFunc1));
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
	Func f(Seq, Array.of(Int), SEQ_NATIVE(callTestFunc2));
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
	Func f(Int, Void, SEQ_NATIVE(callTestFunc3));
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
	Func f(Void, Int, SEQ_NATIVE(callTestFunc4));
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
	Func f(Void, Void, SEQ_NATIVE(callTestFunc5));
	s | f();
	s.source(DEFAULT_TEST_INPUT);
	s.execute();

	EXPECT_TRUE(calltest::call5);
}

#endif /* SEQ_CALLTEST_H */
