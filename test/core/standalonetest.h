#ifndef SEQ_STANDALONETEST_H
#define SEQ_STANDALONETEST_H

#include "../testhelp.h"

TEST(StandaloneTest, StandaloneTest)
{
	ParseState p = parse(TEST_DIR "/data/seq/test.seq");
	p.module("s").source(DEFAULT_TEST_INPUT_MULTI);
	EXPECT_NO_THROW(p.module("s").execute(true));
}

#endif /* SEQ_STANDALONETEST_H */
