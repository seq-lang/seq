#ifndef SEQ_STANDALONETEST_H
#define SEQ_STANDALONETEST_H

#include "../testhelp.h"
#include "seq/parser.h"

TEST(StandaloneTest, StandaloneTest)
{
	SeqModule *s = parse(TEST_DIR "/code/test.seq");
	execute(s, {DEFAULT_TEST_INPUT_MULTI}, {}, true);
}

TEST(StandaloneTestOpt, StandaloneTest)
{
	SeqModule *s = parse(TEST_DIR "/code/test.seq");
	execute(s, {DEFAULT_TEST_INPUT_MULTI});
}

#endif /* SEQ_STANDALONETEST_H */
